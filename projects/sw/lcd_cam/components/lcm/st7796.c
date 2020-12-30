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
#include "st7796.h"

static const char *TAG = "lcm";

typedef struct {
    st7796_config_t config;
    void (*write_cb)(uint8_t *data, size_t len);
} st7796_obj_t;

static st7796_obj_t *st7796_obj = NULL;

static void inline st7796_set_level(int8_t io_num, uint8_t state, bool invert)
{
    if (io_num < 0) {
        return;
    }
    gpio_set_level(io_num, invert ? !state : state);
}

static void st7796_delay_ms(uint32_t time)
{
    vTaskDelay(time / portTICK_RATE_MS);
}

static void st7796_write_cmd(uint8_t cmd)
{
    st7796_set_level(st7796_obj->config.pin.dc, 0, st7796_obj->config.invert.dc);
    st7796_obj->write_cb(&cmd, 1);
}

static void st7796_write_reg(uint8_t data)
{
    st7796_set_level(st7796_obj->config.pin.dc, 1, st7796_obj->config.invert.dc);
    st7796_obj->write_cb(&data, 1);
}

static void st7796_write_data(uint8_t *data, size_t len)
{
    if (len <= 0) {
        return;
    }
    st7796_set_level(st7796_obj->config.pin.dc, 1, st7796_obj->config.invert.dc);
    st7796_obj->write_cb(data, len);
}

static void st7796_rst()
{
    st7796_set_level(st7796_obj->config.pin.rst, 0, st7796_obj->config.invert.rst);
    st7796_delay_ms(100);
    st7796_set_level(st7796_obj->config.pin.rst, 1, st7796_obj->config.invert.rst);
    st7796_delay_ms(100);
}

static void st7796_config(st7796_config_t *config)
{
	st7796_write_cmd(0x11); 		//Sleep Out
	st7796_delay_ms(200);
	st7796_write_cmd(0xf0); 
	st7796_write_reg(0xc3); 			//enable command 2 part 1
	st7796_write_cmd(0xf0); 
	st7796_write_reg(0x96); 			//enable command 2 part 2
	st7796_write_cmd(0x36); 		//内存数据访问控制
    switch (config->horizontal) {
        case 0: {
            st7796_write_reg(0x28);
        }
        break;

        case 1: {
            st7796_write_reg(0xA8);
        }
        break;

        case 2: {
            st7796_write_reg(0x48);
        }
        break;

        case 3: {
            st7796_write_reg(0xC8);
        }
        break;

        default: {
            st7796_write_reg(0x28);
        }
        break;
    }
	
	st7796_write_cmd(0x3a); 		//16bit pixel
	st7796_write_reg(0x55);
	
	st7796_write_cmd(0xb4);
	st7796_write_reg(0x01);

	st7796_write_cmd(0xb7); st7796_write_reg(0xc6);
	
	st7796_write_cmd(0xe8); st7796_write_reg(0x40); 
	st7796_write_reg(0x8a); st7796_write_reg(0x00); 
	st7796_write_reg(0x00); st7796_write_reg(0x29); 
	st7796_write_reg(0x19); st7796_write_reg(0xa5); 
	st7796_write_reg(0x33);
	
	st7796_write_cmd(0xc1); st7796_write_reg(0x06);
	st7796_write_cmd(0xc2); st7796_write_reg(0xa7);
	st7796_write_cmd(0xc5); st7796_write_reg(0x18);
	
	st7796_write_cmd(0xe0); st7796_write_reg(0xf0); 
	st7796_write_reg(0x09); st7796_write_reg(0x0b); 
	st7796_write_reg(0x06); st7796_write_reg(0x04); 
	st7796_write_reg(0x15);st7796_write_reg(0x2f); 
	st7796_write_reg(0x54); st7796_write_reg(0x42); 
	st7796_write_reg(0x3c); st7796_write_reg(0x17); 
	st7796_write_reg(0x14); st7796_write_reg(0x18); 
	st7796_write_reg(0x1b);
	
	//Negative Voltage Gamma Coltrol
	st7796_write_cmd(0xe1); st7796_write_reg(0xf0); 
	st7796_write_reg(0x09); st7796_write_reg(0x0b); 
	st7796_write_reg(0x06); st7796_write_reg(0x04); 
	st7796_write_reg(0x03); st7796_write_reg(0x2d); 
	st7796_write_reg(0x43); st7796_write_reg(0x42); 
	st7796_write_reg(0x3b); st7796_write_reg(0x16); 
	st7796_write_reg(0x14); st7796_write_reg(0x17); 
	st7796_write_reg(0x1b);
	
	st7796_write_cmd(0xf0); st7796_write_reg(0x3c);
	st7796_write_cmd(0xf0); st7796_write_reg(0x69); 
	st7796_delay_ms(120);
	st7796_write_cmd(0x29); //Display ON
}


static void st7796_set_index(uint16_t x_start, uint16_t y_start, uint16_t x_end, uint16_t y_end)
{
    uint16_t start_pos, end_pos;
    st7796_write_cmd(0x2a);    // CASET (2Ah): Column Address Set 
    // Must write byte than byte
    start_pos = x_start;
    end_pos = x_end;
    st7796_write_reg(start_pos >> 8);
    st7796_write_reg(start_pos & 0xFF);
    st7796_write_reg(end_pos >> 8);
    st7796_write_reg(end_pos & 0xFF);

    st7796_write_cmd(0x2b);    // RASET (2Bh): Row Address Set
    start_pos = y_start;
    end_pos = y_end;
    st7796_write_reg(start_pos >> 8);
    st7796_write_reg(start_pos & 0xFF);
    st7796_write_reg(end_pos >> 8);
    st7796_write_reg(end_pos & 0xFF); 
    st7796_write_cmd(0x2c);    // RAMWR (2Ch): Memory Write 
}


esp_err_t st7796_deinit(st7796_handle_t *handle)
{
    free(st7796_obj);
    return ESP_OK;
}

esp_err_t st7796_init(st7796_handle_t *handle, st7796_config_t *config)
{
    if (handle == NULL || config == NULL) {
        ESP_LOGE(TAG, "arg error\n");
        return ESP_FAIL;
    }
    st7796_obj = (st7796_obj_t *)heap_caps_calloc(1, sizeof(st7796_obj_t), MALLOC_CAP_DEFAULT);
    if (!st7796_obj) {
        ESP_LOGE(TAG, "lcm object malloc error\n");
        return ESP_FAIL;
    }

    memcpy(&st7796_obj->config, config, sizeof(st7796_config_t));
    st7796_obj->write_cb = config->write_cb;
    if (st7796_obj->write_cb == NULL) {
        ESP_LOGE(TAG, "lcm callback NULL\n");
        st7796_deinit(handle);
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
    st7796_set_level(st7796_obj->config.pin.rd, 1, st7796_obj->config.invert.rd);
    st7796_set_level(st7796_obj->config.pin.cs, 1, st7796_obj->config.invert.cs);
    st7796_rst();//st7796_rst before LCD Init.
    st7796_delay_ms(100);
    st7796_set_level(st7796_obj->config.pin.cs, 0, st7796_obj->config.invert.cs);
    st7796_config(config);
    st7796_set_level(st7796_obj->config.pin.bk, 1, st7796_obj->config.invert.bk);
    handle->set_index = st7796_set_index;
    handle->write_data = st7796_write_data;
    return ESP_OK;
}
