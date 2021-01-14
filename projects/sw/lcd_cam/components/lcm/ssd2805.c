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

static void ssd2805_write_cmd(uint8_t cmd, uint32_t len, ...)
{
    if (len > 32) {
        return;
    }
    va_list arg_ptr; 
    uint8_t command_param[32] = { 0 };
    va_start(arg_ptr, len);
    for (int x = 0; x < len; x++) {
        command_param[x] = va_arg(arg_ptr, int);
        // printf("[%d]: 0x%x\n", x, command_param[x]);
    }
    va_end(arg_ptr); 

    ssd2805_set_level(ssd2805_obj->config.pin.dc, 0, ssd2805_obj->config.invert.dc);
    ssd2805_obj->write_cb(&cmd, 1);
    ssd2805_set_level(ssd2805_obj->config.pin.dc, 1, ssd2805_obj->config.invert.dc);
    if (len > 0) {
        ssd2805_obj->write_cb(command_param, len);
    }
}

static void ssd2805_write_reg(uint8_t cmd, uint16_t data)
{
    ssd2805_write_cmd(cmd, 2, data & 0xFF, (data >> 8) & 0xFF);
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
    //Step 1: Set PLL

    ssd2805_write_reg(0xba, 0x0004);    //PLL     = clock*MUL/(PDIV*DIV) 
                    //        = clock*(BAh[7:0]+1)/((BAh[15:12]+1)*(BAh[11:8]+1))
                    //        = 20*(0x0f+1)/1*1 = 20*16 = 320MHz
                    //Remark: 350MHz >= fvco >= 225MHz for SSD2805 since the max. speed per lane is 350Mbps
    ssd2805_write_reg(0xb9, 0x0001);    //enable PLL

    ssd2805_delay_ms(200);        //simply wait for 2 ms for PLL lock, more stable as SSD2805ReadReg(arg) doesn't work at full compiler optimzation
    
    //Step 3: set clock control register for SYS_CLK & LP clock speed
    //SYS_CLK = TX_CLK/(BBh[7:6]+1), TX_CLK = external oscillator clock speed
    //In this case, SYS_CLK = 20MHz/(1+1)=10MHz. Measure SYS_CLK pin to verify it.
    //LP clock = PLL/(8*(BBh[5:0]+1)) = 320/(8*(4+1)) = 8MHz, conform to AUO panel's spec, default LP = 8Mbps
    //S6D04D2 is the controller of AUO 1.54" panel.
    ssd2805_write_reg(0xBB, 0x0044);
    ssd2805_write_reg(0xD6, 0x0100);    //output sys_clk for debug. Now check sys_clk pin for 10MHz signal

    //Step 4: Set MIPI packet format
    ssd2805_write_reg(0xB7, 0x0201);    //0x0243 EOT packet enable, write operation, it is a DCS packet
                                    //HS clock is disabled, video mode disabled, in HS mode to send data

    //Step 5: set Virtual Channel (VC) to use
    ssd2805_write_reg(0xB8, 0x0000);

    //Step 6: Now write command to panel
    ssd2805_write_reg(0xBE, 0x0050);

    // ssd2805_write_reg(0xC9, 0x0B02);
    // ssd2805_write_reg(0xCA, 0x2003);
    // ssd2805_write_reg(0xCB, 0x021a);
    // ssd2805_write_reg(0xCC, 0x0d12);
    // ssd2805_write_reg(0xCD, 0x1000);
    // ssd2805_write_reg(0xCE, 0x0405);
    // ssd2805_write_reg(0xCF, 0x0000);
    // ssd2805_write_reg(0xD0, 0x0010);
    // ssd2805_write_reg(0xD1, 0x0000);
    // ssd2805_write_reg(0xD2, 0x0010);
}

void ssd2805_gen_write_cmd(uint8_t cmd, uint32_t len, ...)
{
    va_list arg_ptr; 
    uint8_t *data = malloc(len + 1);
    va_start(arg_ptr, len);
    ssd2805_write_reg(0xB7, 0x0201);
    ssd2805_write_reg(0xBC, (len+1) & 0xFFFF);
    ssd2805_write_reg(0xBD, (len+1) >> 16);

    data[0] = cmd;
    for (int x = 0; x < len; x++) {
        data[x + 1] = va_arg(arg_ptr, int);
    }
    va_end(arg_ptr); 
    ssd2805_write_cmd(0xBF, 0);
    ssd2805_obj->write_cb(data, len+1);
    free(data);
}

void ssd2805_dcs_write_cmd(uint8_t cmd, uint32_t len, ...)
{
    va_list arg_ptr; 
    uint8_t *data = malloc(len);
    va_start(arg_ptr, len);
    ssd2805_write_reg(0xB7, 0x0241);
    ssd2805_write_reg(0xBC, (len) & 0xFFFF);
    ssd2805_write_reg(0xBD, ((len) >> 16) & 0xFFFF);

    for (int x = 0; x < len; x++) {
        data[x] = va_arg(arg_ptr, int);
    }
    va_end(arg_ptr); 
    ssd2805_write_cmd(cmd, 0);
    if (len > 0) {
        ssd2805_obj->write_cb(data, len);
    }
    free(data);
}

void ssd2805_dcs_write_data(uint8_t *data, uint32_t len)
{
    ssd2805_write_reg(0xB7, 0x0241);
    ssd2805_write_reg(0xBC, (len) & 0xFFFF);
    ssd2805_write_reg(0xBD, (len) >> 16);
    
    ssd2805_write_cmd(0x3c, 0);
    ssd2805_obj->write_cb(data, len);
}

static void ssd2805_lcm_config(ssd2805_config_t *config)
{   
    // ssd2805_dcs_write_cmd(0x01, 0);
    // ssd2805_delay_ms(100);

    // ssd2805_dcs_write_cmd(0x11, 0);

    // ssd2805_dcs_write_cmd(0x29, 0);
    // // Refresh
    // ssd2805_dcs_write_cmd(0x36, 1, 0x00);
    // // Pixel Format
    // ssd2805_dcs_write_cmd(0x3A, 1, 0x55);
    // // Normal Display Mode On
    // ssd2805_dcs_write_cmd(0x13, 0);

    ssd2805_dcs_write_cmd(0x11, 0);         //Sleep Out
    ssd2805_delay_ms(200);
    ssd2805_dcs_write_cmd(0x36, 1, 0x00);
    
    ssd2805_dcs_write_cmd(0x3a, 1, 0x57);         //16bit pixel

    ssd2805_dcs_write_cmd(0x13, 0); 
    ssd2805_dcs_write_cmd(0x38, 0); //Normal mode
    ssd2805_delay_ms(120);
    ssd2805_dcs_write_cmd(0x29, 0); //Display ON
}

static void ssd2805_set_index(uint16_t x_start, uint16_t y_start, uint16_t x_end, uint16_t y_end)
{
    ssd2805_dcs_write_cmd(0x2a, 4, (x_start >> 8) & 0xFF, x_start & 0xff, (x_end >> 8) & 0xFF, x_end & 0xff);
    // ssd2805_dcs_write_cmd(0x2a, 4, x_start & 0xff, (x_start >> 8) & 0xFF, x_end & 0xff, (x_end >> 8) & 0xFF);
    ssd2805_dcs_write_cmd(0x2b, 4, (y_start >> 8) & 0xFF, y_start & 0xff, (y_end >> 8) & 0xFF, y_end & 0xff);
    // ssd2805_dcs_write_cmd(0x2b, 4, y_start & 0xff, (y_start >> 8) & 0xFF, y_end & 0xff, (y_end >> 8) & 0xFF);
}

static void ssd2805_write_data(uint8_t *data, size_t len)
{
    if (len <= 0) {
        return;
    }
    ssd2805_dcs_write_data(data, len);
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
    if (ssd2805_obj->config.width == 8 && (ssd2805_obj->config.pin.rst == -1)) { // 当没有外部复位和位宽为8位时，需要配置两次寄存器
        ssd2805_config(config);
        ssd2805_lcm_config(config);
    }
    ssd2805_set_level(ssd2805_obj->config.pin.bk, 0, ssd2805_obj->config.invert.bk);

    typedef struct {
        uint8_t data[24 / 8];
    } rgb_data_t;

    uint32_t data = 0;
    
    rgb_data_t tx_data[128];

    for (int x = 0; x < 128; x++) {
        data = 0xFFFF00;
        memcpy(&tx_data[x], &data, sizeof(rgb_data_t));
    }

    printf("test\n");

    // ssd2805_set_index(10, 10, 110 - 1, 110 - 1);
    
    while (1) {
        // Memory write
        for (int y = 0; y < 480; y++) {
            for (int x = 0; x < 320; x+=1) {
                ssd2805_dcs_write_data((uint8_t *)tx_data, 1 * sizeof(rgb_data_t));
                // ssd2805_dcs_write_cmd(0x3C, 4, 0x1F, 0x00, 0x1F, 0x00);
                // ssd2805_dcs_write_cmd(0x3C, 3, 0x00, 0x00, 0xFF);
            }
        }
    }

    // while (1) {
    //     // Memory write
    //     for (int y = 10; y < 110; y++) {
    //         for (int x = 10; x < 110; x+=50) {
    //             ssd2805_dcs_write_data((uint8_t *)tx_data, 50 * sizeof(rgb_data_t));
    //             // ssd2805_dcs_write_cmd(0x3C, 4, 0x1F, 0x00, 0x1F, 0x00);
    //             // ssd2805_gen_write_cmd(0x3C, 2, 0x00, 0x1F);
    //         }
    //     }
    // }
    handle->set_index = ssd2805_set_index;
    handle->write_data = ssd2805_write_data;
    return ESP_OK;
}
