// Copyright 2010-2020 Espressif Systems (Shanghai) PTE LTD
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "driver/gpio.h"
#include "ssd2805.h"

static const char *TAG = "lcm";

typedef struct {
    ssd2805_config_t config;
    void (*write_cb)(uint8_t *data, size_t len);
} ssd2805_obj_t;

static ssd2805_obj_t *ssd2805_obj = NULL;

static void inline ssd2805_set_level(int8_t io_num, uint8_t state, bool invert)
{
    if (io_num < 0) {
        return;
    }
    gpio_set_level(io_num, invert ? !state : state);
}

static void ssd2805_delay_ms(uint32_t time)
{
    vTaskDelay(time / portTICK_RATE_MS);
}

static void ssd2805_write_cmd(uint8_t cmd)
{
    ssd2805_set_level(ssd2805_obj->config.pin.dc, 0, ssd2805_obj->config.invert.dc);
    ssd2805_set_level(ssd2805_obj->config.pin.cs, 0, ssd2805_obj->config.invert.cs);
    ssd2805_obj->write_cb(&cmd, 1);
    ssd2805_set_level(ssd2805_obj->config.pin.cs, 1, ssd2805_obj->config.invert.cs);
}

static void ssd2805_write_byte(uint8_t data)
{
    ssd2805_set_level(ssd2805_obj->config.pin.dc, 1, ssd2805_obj->config.invert.dc);
    ssd2805_set_level(ssd2805_obj->config.pin.cs, 0, ssd2805_obj->config.invert.cs);
    ssd2805_obj->write_cb(&data, 1);
    ssd2805_set_level(ssd2805_obj->config.pin.cs, 1, ssd2805_obj->config.invert.cs);
}

static void ssd2805_write_reg(uint8_t cmd, uint16_t data)
{
    ssd2805_write_cmd(cmd);
    ssd2805_set_level(ssd2805_obj->config.pin.dc, 1, ssd2805_obj->config.invert.dc);
    ssd2805_set_level(ssd2805_obj->config.pin.cs, 0, ssd2805_obj->config.invert.cs);
    ssd2805_obj->write_cb(&data, 2);
    ssd2805_set_level(ssd2805_obj->config.pin.cs, 1, ssd2805_obj->config.invert.cs);
}

static void ssd2805_rst()
{
    ssd2805_set_level(ssd2805_obj->config.pin.rst, 1, ssd2805_obj->config.invert.rst);
    ssd2805_delay_ms(100);
    ssd2805_set_level(ssd2805_obj->config.pin.rst, 0, ssd2805_obj->config.invert.rst);
    ssd2805_delay_ms(100);
    ssd2805_set_level(ssd2805_obj->config.pin.rst, 1, ssd2805_obj->config.invert.rst);
    ssd2805_delay_ms(100);
}

static void ssd2805_config(ssd2805_config_t *config)
{
    ssd2805_write_reg(0xB9, 0x0000); //PLL Control, Disable
    ssd2805_delay_ms(20);
    ssd2805_write_reg(0xBA, 0x0012); //PLL Configuration, P & N M, P = 0x1, N = 0x1, M = 0x12
    ssd2805_write_reg(0xB9, 0x0001); //PLL Control, Enable
    ssd2805_delay_ms(20);
    ssd2805_write_reg(0xD6, 0x0105); //Test, R2 PNB & END & COLOR
    ssd2805_write_reg(0xB8, 0x0000); //Virtual Channel Control
    ssd2805_write_reg(0xBB, 0x0003); //Clock Control, SYS_CLK & LP Clock
    ssd2805_delay_ms(25);

		// //Step 1: Set PLL
		// ssd2805_write_reg(0xba, 0x000f);	//PLL 	= clock*MUL/(PDIV*DIV) 
		// 				//		= clock*(BAh[7:0]+1)/((BAh[15:12]+1)*(BAh[11:8]+1))
		// 				//		= 20*(0x0f+1)/1*1 = 20*16 = 320MHz
		// 				//Remark: 350MHz >= fvco >= 225MHz for SSD2805 since the max. speed per lane is 350Mbps
		// ssd2805_write_reg(0xb9, 0x0001);	//enable PLL

		// //	while((SSD2805ReadReg(0xc6)&0x0080)!=0x0080)	//infinite loop to wait for PLL to lock
		// //		;

		// ssd2805_delay_ms(2);		//simply wait for 2 ms for PLL lock, more stable as SSD2805ReadReg(arg) doesn't work at full compiler optimzation
		
		// //Step 3: set clock control register for SYS_CLK & LP clock speed
		// //SYS_CLK = TX_CLK/(BBh[7:6]+1), TX_CLK = external oscillator clock speed
		// //In this case, SYS_CLK = 20MHz/(1+1)=10MHz. Measure SYS_CLK pin to verify it.
		// //LP clock = PLL/(8*(BBh[5:0]+1)) = 320/(8*(4+1)) = 8MHz, conform to AUO panel's spec, default LP = 8Mbps
		// //S6D04D2 is the controller of AUO 1.54" panel.
		// ssd2805_write_reg(0xBB, 0x0044);
		// ssd2805_write_reg(0xD6, 0x0100);	//output sys_clk for debug. Now check sys_clk pin for 10MHz signal

		// //Step 4: Set MIPI packet format
		// ssd2805_write_reg(0xB7, 0x0243);	//EOT packet enable, write operation, it is a DCS packet
		// 								//HS clock is disabled, video mode disabled, in HS mode to send data

		// //Step 5: set Virtual Channel (VC) to use
		// ssd2805_write_reg(0xB8, 0x0000);

		// //Step 6: Now write DCS command to AUO panel for system power-on upon reset
		// ssd2805_write_reg(0xbc, 0x0000);			//define TDC size
		// ssd2805_write_reg(0xbd, 0x0000);
		// ssd2805_write_cmd(0x11);				//DCS sleep-out command
}

static void ssd2805_gen_packet(uint32_t len)
{
    ssd2805_write_reg(0xBC, len & 0xFFFF);
    ssd2805_write_reg(0xBD, len >> 16);
    ssd2805_write_cmd(0xBF);
}

void ssd2805_gen_write(uint8_t cmd, uint32_t len, ...)
{
	va_list arg_ptr; 
	int data = 0; 
    va_start(arg_ptr, len);
    ssd2805_write_reg(0xBC, (len+1) & 0xFFFF);
    ssd2805_write_reg(0xBD, (len+1) >> 16);
	ssd2805_write_cmd(0xBF);
    // ssd2805_write_cmd(0x2c);
	ssd2805_write_byte(cmd);
    for (int x = 0; x < len; x++) {
		data = va_arg(arg_ptr, int);
        ssd2805_write_byte(data);
    }
	va_end(arg_ptr); 
}

static void ssd2805_dcs_packet(uint32_t len)
{
    ssd2805_write_reg(0xBC, len & 0xFFFF);
    ssd2805_write_reg(0xBD, len >> 16);
    ssd2805_write_reg(0xB7, 0x0340);
    ssd2805_write_cmd(0x2c);
}

static void ssd2805_lcm_config(ssd2805_config_t *config)
{
    ssd2805_write_reg(0xB7,0x0210); //Generic Packet
    ssd2805_delay_ms(200);
    
    ssd2805_gen_write(0xFF,4,0xAA,0x55,0x25,0x01);	
    ssd2805_gen_write(0xF0,5,0x55,0xAA,0x52,0x08,0x00);
    ssd2805_gen_write(0xB1,1,0xFC);
    ssd2805_gen_write(0xB8,4,0x01,0x02,0x02,0x02);
    ssd2805_gen_write(0xBC,3,0x05,0x05,0x05);
    ssd2805_gen_write(0xB7,2,0x00,0x00);
    ssd2805_gen_write(0xC8,18,0x01,0x00,0x46,0x1E,0x46,0x1E,0x46,0x1E,0x46,0x1E,0x64,0x3C,0x3C,0x64,0x64,0x3C,0x3C,0x64);
    ssd2805_gen_write(0xF0,5,0xAA,0x55,0x52,0x08,0x01);
    ssd2805_gen_write(0xB0,3,0x05,0x05,0x05);
    ssd2805_gen_write(0xB6,3,0x44,0x44,0x44);
    ssd2805_gen_write(0xB1,3,0x05,0x05,0x05);
    ssd2805_gen_write(0xB7,3,0x34,0x34,0x34);
    ssd2805_gen_write(0xB3,3,0x16,0x16,0x16);
    ssd2805_gen_write(0xB4,3,0x0A,0x0A,0x0A);
    ssd2805_gen_write(0xBC,3,0x00,0x90,0x11);
    ssd2805_gen_write(0xBD,3,0x00,0x90,0x11);
    ssd2805_gen_write(0xBE,1,0x51);	
    ssd2805_gen_write(0xD1,16,0x00,0x17,0x00,0x24,0x00,0x3D,0x00,0x52,0x00,0x66,0x00,0x86,0x00,0xA0,0x00,0xCC);
    ssd2805_gen_write(0xD2,16,0x00,0xF1,0x01,0x26,0x01,0x4E,0x01,0x8C,0x01,0xBC,0x01,0xBE,0x01,0xE7,0x02,0x0E);
    ssd2805_gen_write(0xD3,16,0x02,0x22,0x02,0x3C,0x02,0x4F,0x02,0x71,0x02,0x90,0x02,0xC6,0x02,0xF1,0x03,0x3A);
    ssd2805_gen_write(0xD4,4,0x03,0xB15,0x03,0xC1);
    ssd2805_gen_write(0xD5,16,0x00,0x17,0x00,0x24,0x00,0x3D,0x00,0x52,0x00,0x66,0x00,0x86,0x00,0xA0,0x00,0xCC);
    ssd2805_gen_write(0xD6,16,0x00,0xF1,0x01,0x26,0x01,0x4E,0x01,0x8C,0x01,0xBC,0x01,0xBE,0x01,0xE7,0x02,0x0E);
    ssd2805_gen_write(0xD7,16,0x02,0x22,0x02,0x3C,0x02,0x4F,0x02,0x71,0x02,0x90,0x02,0xC6,0x02,0xF1,0x03,0x3A);
    ssd2805_gen_write(0xD8,4,0x03,0xB5,0x03,0xC1);
    ssd2805_gen_write(0xD9,16,0x00,0x17,0x00,0x24,0x00,0x3D,0x00,0x52,0x00,0x66,0x00,0x86,0x00,0xA0,0x00,0xCC);
    ssd2805_gen_write(0xDD,16,0x00,0xF1,0x01,0x26,0x01,0x4E,0x01,0x8C,0x01,0xBC,0x01,0xBE,0x01,0xE7,0x02,0x0E);
    ssd2805_gen_write(0xDE,16,0x02,0x22,0x02,0x3C,0x02,0x4F,0x02,0x71,0x02,0x90,0x02,0xC6,0x02,0xF1,0x03,0x3A);
    ssd2805_gen_write(0xDF,4,0x03,0xB5,0x03,0xC1);
    ssd2805_gen_write(0xE0,16,0x00,0x17,0x00,0x24,0x00,0x3D,0x00,0x52,0x00,0x66,0x00,0x86,0x00,0xA0,0x00,0xCC);
    ssd2805_gen_write(0xE1,16,0x00,0xF1,0x01,0x26,0x01,0x4E,0x01,0x8C,0x01,0xBC,0x01,0xBE,0x01,0xE7,0x02,0x0E);
    ssd2805_gen_write(0xE2,16,0x02,0x22,0x02,0x3C,0x02,0x4F,0x02,0x71,0x02,0x90,0x02,0xC6,0x02,0xF1,0x03,0x3A);
    ssd2805_gen_write(0xE3,4,0x03,0xB5,0x03,0xC1);
    ssd2805_gen_write(0xE4,16,0x00,0x17,0x00,0x24,0x00,0x3D,0x00,0x52,0x00,0x66,0x00,0x86,0x00,0xA0,0x00,0xCC);
    ssd2805_gen_write(0xE5,16,0x00,0xF1,0x01,0x26,0x01,0x4E,0x01,0x8C,0x01,0xBC,0x01,0xBE,0x01,0xE7,0x02,0x0E);
    ssd2805_gen_write(0xE6,16,0x02,0x22,0x02,0x3C,0x02,0x4F,0x02,0x71,0x02,0x90,0x02,0xC6,0x02,0xF1,0x03,0x3A);
    ssd2805_gen_write(0xE7,4,0x03,0xB5,0x03,0xC1);
    ssd2805_gen_write(0xE8,16,0x00,0x17,0x00,0x24,0x00,0x3D,0x00,0x52,0x00,0x66,0x00,0x86,0x00,0xA0,0x00,0xCC);
    ssd2805_gen_write(0xE9,16,0x00,0xF1,0x01,0x26,0x01,0x4E,0x01,0x8C,0x01,0xBC,0x01,0xBE,0x01,0xE7,0x02,0x0E);
    ssd2805_gen_write(0xEA,16,0x02,0x22,0x02,0x3C,0x02,0x4F,0x02,0x71,0x02,0x90,0x02,0xC6,0x02,0xF1,0x03,0x3A);
    ssd2805_gen_write(0xEB,4,0x03,0xB5,0x03,0xC1);
    ssd2805_delay_ms(200);
    ssd2805_gen_write(0x35,0,0x00);
    ssd2805_gen_write(0x11,0,0x00);
    ssd2805_delay_ms(200);
    ssd2805_gen_write(0x29,0,0x00);

    // ssd2805_write_reg(0xB7, 0x0243);
}

static void ssd2805_set_index(uint16_t x_start, uint16_t y_start, uint16_t x_end, uint16_t y_end)
{
    ssd2805_write_reg(0xB7,0x0210); //Generic Packet 
    ssd2805_write_reg(0xBC, 5);
    ssd2805_write_reg(0xBD, 0);
	ssd2805_write_cmd(0xbf);  
	ssd2805_write_byte(0x2a);   
	ssd2805_write_byte(x_start >> 8);
	ssd2805_write_byte(x_start & 0xff);
	ssd2805_write_byte(x_end >> 8);
	ssd2805_write_byte(x_end & 0xff);
    ssd2805_write_reg(0xBC, 5);
    ssd2805_write_reg(0xBD, 0); 
    ssd2805_write_cmd(0xbf);  
	ssd2805_write_byte(0x2b);   
	ssd2805_write_byte(y_start >> 8);
	ssd2805_write_byte(y_start & 0xff);
	ssd2805_write_byte(y_end >> 8);
	ssd2805_write_byte(y_end & 0xff);
    // ssd2805_dcs_packet(540 * 960 * 2);
}

static void ssd2805_write_data(uint8_t *data, size_t len)
{
    if (len <= 0) {
        return;
    }
    ssd2805_write_reg(0xB7,0x0210); //Generic Packet 
    ssd2805_write_reg(0xBC, (len+1) & 0xFFFF);
    ssd2805_write_reg(0xBD, (len+1) >> 16);
    ssd2805_write_reg(0xbe, 0x0400);
    ssd2805_write_cmd(0xbf); 
    ssd2805_write_byte(0x2c);
    ssd2805_set_level(ssd2805_obj->config.pin.dc, 1, ssd2805_obj->config.invert.dc);
    ssd2805_set_level(ssd2805_obj->config.pin.cs, 0, ssd2805_obj->config.invert.cs);
    ssd2805_obj->write_cb(data, len);
    ssd2805_set_level(ssd2805_obj->config.pin.cs, 1, ssd2805_obj->config.invert.cs);
}

esp_err_t ssd2805_deinit(ssd2805_handle_t *handle)
{
    free(ssd2805_obj);
    return ESP_OK;
}

esp_err_t ssd2805_init(ssd2805_handle_t *handle, ssd2805_config_t *config)
{
    if (handle == NULL || config == NULL) {
        ESP_LOGE(TAG, "arg error\n");
        return ESP_FAIL;
    }
    ssd2805_obj = (ssd2805_obj_t *)heap_caps_calloc(1, sizeof(ssd2805_obj_t), MALLOC_CAP_DEFAULT);
    if (ssd2805_obj == NULL) {
        ESP_LOGE(TAG, "lcm object malloc error\n");
        return ESP_FAIL;
    }

    memcpy(&ssd2805_obj->config, config, sizeof(ssd2805_config_t));
    ssd2805_obj->write_cb = config->write_cb;
    if (ssd2805_obj->write_cb == NULL) {
        ESP_LOGE(TAG, "lcm callback NULL\n");
        ssd2805_deinit(handle);
        return ESP_FAIL;
    }

    //Initialize non-matrix GPIOs
    gpio_config_t io_conf;
    io_conf.intr_type = GPIO_INTR_DISABLE;
    io_conf.mode = GPIO_MODE_OUTPUT;
    io_conf.pin_bit_mask  = (config->pin.dc < 0) ? 0ULL : (1ULL << config->pin.dc);
    io_conf.pin_bit_mask |= (config->pin.rd < 0) ? 0ULL : (1ULL << config->pin.rd);
    io_conf.pin_bit_mask |= (config->pin.rst < 0) ? 0ULL : (1ULL << config->pin.rst);
    io_conf.pin_bit_mask |= (config->pin.bk < 0) ? 0ULL : (1ULL << config->pin.bk);
    io_conf.pin_bit_mask |= (config->pin.cs < 0) ? 0ULL : (1ULL << config->pin.cs);
    io_conf.pull_down_en = 0;
    io_conf.pull_up_en = 0;
    gpio_config(&io_conf);
    ssd2805_set_level(ssd2805_obj->config.pin.dc, 1, ssd2805_obj->config.invert.dc);
    ssd2805_set_level(ssd2805_obj->config.pin.rd, 1, ssd2805_obj->config.invert.rd);
    ssd2805_set_level(ssd2805_obj->config.pin.cs, 1, ssd2805_obj->config.invert.cs);
    ssd2805_rst();//ssd2805_rst before LCD Init.
    ssd2805_delay_ms(100);
    ssd2805_set_level(ssd2805_obj->config.pin.cs, 0, ssd2805_obj->config.invert.cs);
    ssd2805_config(config);
    ssd2805_lcm_config(config);
    // ssd2805_dcs_packet(540 * 960 * 2);
    // if (ssd2805_obj->config.width == 8 && (ssd2805_obj->config.pin.rst == -1)) { // 当没有外部复位和位宽为8位时，需要配置两次寄存器
    //     ssd2805_config(config);
    //     ssd2805_lcm_config(config);
    //     ssd2805_dcs_packet(800 * 480 * 2);
    // }
    ssd2805_set_level(ssd2805_obj->config.pin.bk, 0, ssd2805_obj->config.invert.bk);
    handle->set_index = ssd2805_set_index;
    handle->write_data = ssd2805_write_data;
    return ESP_OK;
}
