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
#include "st7789.h"

static const char *TAG = "lcm";

typedef struct {
    st7789_config_t config;
    void (*write_cb)(uint8_t *data, size_t len);
} st7789_obj_t;

static st7789_obj_t *st7789_obj = NULL;

static void inline st7789_set_level(int8_t io_num, uint8_t state, bool invert)
{
    if (io_num < 0) {
        return;
    }
    gpio_set_level(io_num, invert ? !state : state);
}

static void st7789_delay_ms(uint32_t time)
{
    vTaskDelay(time / portTICK_RATE_MS);
}

static void st7789_write_cmd(uint8_t cmd)
{
    st7789_set_level(st7789_obj->config.pin.dc, 0, st7789_obj->config.invert.dc);
    st7789_obj->write_cb(&cmd, 1);
}

static void st7789_write_reg(uint8_t data)
{
    st7789_set_level(st7789_obj->config.pin.dc, 1, st7789_obj->config.invert.dc);
    st7789_obj->write_cb(&data, 1);
}

static void st7789_write_data(uint8_t *data, size_t len)
{
    if (len <= 0) {
        return;
    }
    st7789_set_level(st7789_obj->config.pin.dc, 1, st7789_obj->config.invert.dc);
    st7789_obj->write_cb(data, len);
}

static void st7789_rst()
{
    st7789_set_level(st7789_obj->config.pin.rst, 0, st7789_obj->config.invert.rst);
    st7789_delay_ms(100);
    st7789_set_level(st7789_obj->config.pin.rst, 1, st7789_obj->config.invert.rst);
    st7789_delay_ms(100);
}

static void st7789_config(st7789_config_t *config)
{
    st7789_write_cmd(0x36); // MADCTL (36h): Memory Data Access Control

    uint8_t bgr = (config->dis_bgr ? 0x08 : 0x0);

    switch (config->horizontal) {
        case 0: {
            st7789_write_reg(0x00 | bgr);
        }
        break;

        case 1: {
            st7789_write_reg(0xC0 | bgr);
        }
        break;

        case 2: {
            st7789_write_reg(0x70 | bgr);
        }
        break;

        case 3: {
            st7789_write_reg(0xA0 | bgr);
        }
        break;

        default: {
            st7789_write_reg(0x00 | bgr);
        }
        break;
    }

    st7789_write_cmd(0x3A);  // COLMOD (3Ah): Interface Pixel Format 
    st7789_write_reg(0x05);

    st7789_write_cmd(0xB2); // PORCTRL (B2h): Porch Setting 
    st7789_write_reg(0x0C);
    st7789_write_reg(0x0C);
    st7789_write_reg(0x00);
    st7789_write_reg(0x33);
    st7789_write_reg(0x33); 

    st7789_write_cmd(0xB7); // GCTRL (B7h): Gate Control 
    st7789_write_reg(0x35);  

    st7789_write_cmd(0xBB); // VCOMS (BBh): VCOM Setting 
    st7789_write_reg(0x19);

    st7789_write_cmd(0xC0); // LCMCTRL (C0h): LCM Control 
    st7789_write_reg(0x2C);

    st7789_write_cmd(0xC2); // VDVVRHEN (C2h): VDV and VRH Command Enable
    st7789_write_reg(0x01);

    st7789_write_cmd(0xC3); // VRHS (C3h): VRH Set
    st7789_write_reg(0x12);   

    st7789_write_cmd(0xC4); // VDVS (C4h): VDV Set 
    st7789_write_reg(0x20);  

    st7789_write_cmd(0xC6); // FRCTRL2 (C6h): Frame Rate Control in Normal Mode 
    st7789_write_reg(0x0F);    

    st7789_write_cmd(0xD0); // PWCTRL1 (D0h): Power Control 1 
    st7789_write_reg(0xA4);
    st7789_write_reg(0xA1);

    st7789_write_cmd(0xE0); // PVGAMCTRL (E0h): Positive Voltage Gamma Control
    st7789_write_reg(0xD0);
    st7789_write_reg(0x04);
    st7789_write_reg(0x0D);
    st7789_write_reg(0x11);
    st7789_write_reg(0x13);
    st7789_write_reg(0x2B);
    st7789_write_reg(0x3F);
    st7789_write_reg(0x54);
    st7789_write_reg(0x4C);
    st7789_write_reg(0x18);
    st7789_write_reg(0x0D);
    st7789_write_reg(0x0B);
    st7789_write_reg(0x1F);
    st7789_write_reg(0x23);

    st7789_write_cmd(0xE1); // NVGAMCTRL (E1h): Negative Voltage Gamma Control
    st7789_write_reg(0xD0);
    st7789_write_reg(0x04);
    st7789_write_reg(0x0C);
    st7789_write_reg(0x11);
    st7789_write_reg(0x13);
    st7789_write_reg(0x2C);
    st7789_write_reg(0x3F);
    st7789_write_reg(0x44);
    st7789_write_reg(0x51);
    st7789_write_reg(0x2F);
    st7789_write_reg(0x1F);
    st7789_write_reg(0x1F);
    st7789_write_reg(0x20);
    st7789_write_reg(0x23);

    st7789_write_cmd(config->dis_invert ? 0x21 : 0x20); // INVON (21h): Display Inversion On

    st7789_write_cmd(0x11); // SLPOUT (11h): Sleep Out 

    st7789_write_cmd(0x29); // DISPON (29h): Display On
}


static void st7789_set_index(uint16_t x_start, uint16_t y_start, uint16_t x_end, uint16_t y_end)
{
    uint16_t start_pos, end_pos;
    st7789_write_cmd(0x2a);    // CASET (2Ah): Column Address Set 
    // Must write byte than byte
    if (st7789_obj->config.horizontal == 3) {
        start_pos = x_start + 80;
        end_pos = x_end + 80;
    } else {
        start_pos = x_start;
        end_pos = x_end;
    }
    st7789_write_reg(start_pos >> 8);
    st7789_write_reg(start_pos & 0xFF);
    st7789_write_reg(end_pos >> 8);
    st7789_write_reg(end_pos & 0xFF);

    st7789_write_cmd(0x2b);    // RASET (2Bh): Row Address Set
    if (st7789_obj->config.horizontal == 1) {
        start_pos = x_start + 80;
        end_pos = x_end + 80;
    } else {
        start_pos = y_start;
        end_pos = y_end;
    }
    st7789_write_reg(start_pos >> 8);
    st7789_write_reg(start_pos & 0xFF);
    st7789_write_reg(end_pos >> 8);
    st7789_write_reg(end_pos & 0xFF); 
    st7789_write_cmd(0x2c);    // RAMWR (2Ch): Memory Write 
}


esp_err_t st7789_deinit(st7789_handle_t *handle)
{
    free(st7789_obj);
    return ESP_OK;
}

esp_err_t st7789_init(st7789_handle_t *handle, st7789_config_t *config)
{
    if (handle == NULL || config == NULL) {
        ESP_LOGE(TAG, "arg error\n");
        return ESP_FAIL;
    }
    st7789_obj = (st7789_obj_t *)heap_caps_calloc(1, sizeof(st7789_obj_t), MALLOC_CAP_DEFAULT);
    if (!st7789_obj) {
        ESP_LOGE(TAG, "lcm object malloc error\n");
        return ESP_FAIL;
    }

    memcpy(&st7789_obj->config, config, sizeof(st7789_config_t));
    st7789_obj->write_cb = config->write_cb;
    if (st7789_obj->write_cb == NULL) {
        ESP_LOGE(TAG, "lcm callback NULL\n");
        st7789_deinit(handle);
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
    st7789_set_level(st7789_obj->config.pin.rd, 1, st7789_obj->config.invert.rd);
    st7789_set_level(st7789_obj->config.pin.cs, 1, st7789_obj->config.invert.cs);
    st7789_rst();//st7789_rst before LCD Init.
    st7789_delay_ms(100);
    st7789_set_level(st7789_obj->config.pin.cs, 0, st7789_obj->config.invert.cs);
    st7789_config(config);
    st7789_set_level(st7789_obj->config.pin.bk, 1, st7789_obj->config.invert.bk);
    handle->set_index = st7789_set_index;
    handle->write_data = st7789_write_data;
    return ESP_OK;
}
