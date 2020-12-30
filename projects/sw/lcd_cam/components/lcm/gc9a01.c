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
#include "gc9a01.h"

static const char *TAG = "lcm";

typedef struct {
    gc9a01_config_t config;
    void (*write_cb)(uint8_t *data, size_t len);
} gc9a01_obj_t;

static gc9a01_obj_t *gc9a01_obj = NULL;

static void inline gc9a01_set_level(int8_t io_num, uint8_t state, bool invert)
{
    if (io_num < 0) {
        return;
    }
    gpio_set_level(io_num, invert ? !state : state);
}

static void gc9a01_delay_ms(uint32_t time)
{
    vTaskDelay(time / portTICK_RATE_MS);
}

static void gc9a01_write_cmd(uint8_t cmd)
{
    gc9a01_set_level(gc9a01_obj->config.pin.dc, 0, gc9a01_obj->config.invert.dc);
    gc9a01_obj->write_cb(&cmd, 1);
}

static void gc9a01_write_reg(uint8_t data)
{
    gc9a01_set_level(gc9a01_obj->config.pin.dc, 1, gc9a01_obj->config.invert.dc);
    gc9a01_obj->write_cb(&data, 1);
}

static void gc9a01_write_data(uint8_t *data, size_t len)
{
    if (len <= 0) {
        return;
    }
    gc9a01_set_level(gc9a01_obj->config.pin.dc, 1, gc9a01_obj->config.invert.dc);
    gc9a01_obj->write_cb(data, len);
}

static void gc9a01_rst()
{
    gc9a01_set_level(gc9a01_obj->config.pin.rst, 0, gc9a01_obj->config.invert.rst);
    gc9a01_delay_ms(100);
    gc9a01_set_level(gc9a01_obj->config.pin.rst, 1, gc9a01_obj->config.invert.rst);
    gc9a01_delay_ms(100);
}

static void gc9a01_config(gc9a01_config_t *config)
{
    uint8_t bgr = (config->dis_bgr ? 0x08 : 0x0);

    gc9a01_write_cmd(0xEF);
    gc9a01_write_cmd(0xEB);
    gc9a01_write_reg(0x14); 

    gc9a01_write_cmd(0xFE);             
    gc9a01_write_cmd(0xEF); 

    gc9a01_write_cmd(0xEB);    
    gc9a01_write_reg(0x14); 

    gc9a01_write_cmd(0x84);            
    gc9a01_write_reg(0x40); 

    gc9a01_write_cmd(0x85);            
    gc9a01_write_reg(0xFF); 

    gc9a01_write_cmd(0x86);            
    gc9a01_write_reg(0xFF); 

    gc9a01_write_cmd(0x87);            
    gc9a01_write_reg(0xFF);

    gc9a01_write_cmd(0x88);            
    gc9a01_write_reg(0x0A);

    gc9a01_write_cmd(0x89);            
    gc9a01_write_reg(0x21); 

    gc9a01_write_cmd(0x8A);            
    gc9a01_write_reg(0x00); 

    gc9a01_write_cmd(0x8B);            
    gc9a01_write_reg(0x80); 

    gc9a01_write_cmd(0x8C);            
    gc9a01_write_reg(0x01); 

    gc9a01_write_cmd(0x8D);            
    gc9a01_write_reg(0x01); 

    gc9a01_write_cmd(0x8E);            
    gc9a01_write_reg(0xFF); 

    gc9a01_write_cmd(0x8F);            
    gc9a01_write_reg(0xFF); 


    gc9a01_write_cmd(0xB6);
    gc9a01_write_reg(0x00);
    gc9a01_write_reg(0x20);

    gc9a01_write_cmd(0x36);
    switch (config->horizontal) {
        case 0: {
            gc9a01_write_reg(0x00 | bgr);
        }
        break;

        case 1: {
            gc9a01_write_reg(0xC0 | bgr);
        }
        break;

        case 2: {
            gc9a01_write_reg(0x60 | bgr);
        }
        break;

        case 3: {
            gc9a01_write_reg(0xA0 | bgr);
        }
        break;

        default: {
            gc9a01_write_reg(0x00 | bgr);
        }
        break;
    }

    gc9a01_write_cmd(0x3A);            
    gc9a01_write_reg(0x05); 


    gc9a01_write_cmd(0x90);            
    gc9a01_write_reg(0x08);
    gc9a01_write_reg(0x08);
    gc9a01_write_reg(0x08);
    gc9a01_write_reg(0x08); 

    gc9a01_write_cmd(0xBD);            
    gc9a01_write_reg(0x06);

    gc9a01_write_cmd(0xBC);            
    gc9a01_write_reg(0x00);    

    gc9a01_write_cmd(0xFF);            
    gc9a01_write_reg(0x60);
    gc9a01_write_reg(0x01);
    gc9a01_write_reg(0x04);

    gc9a01_write_cmd(0xC3);            
    gc9a01_write_reg(0x13);
    gc9a01_write_cmd(0xC4);            
    gc9a01_write_reg(0x13);

    gc9a01_write_cmd(0xC9);            
    gc9a01_write_reg(0x22);

    gc9a01_write_cmd(0xBE);            
    gc9a01_write_reg(0x11); 

    gc9a01_write_cmd(0xE1);            
    gc9a01_write_reg(0x10);
    gc9a01_write_reg(0x0E);

    gc9a01_write_cmd(0xDF);            
    gc9a01_write_reg(0x21);
    gc9a01_write_reg(0x0c);
    gc9a01_write_reg(0x02);

    gc9a01_write_cmd(0xF0);   
    gc9a01_write_reg(0x45);
    gc9a01_write_reg(0x09);
    gc9a01_write_reg(0x08);
    gc9a01_write_reg(0x08);
    gc9a01_write_reg(0x26);
    gc9a01_write_reg(0x2A);

    gc9a01_write_cmd(0xF1);    
    gc9a01_write_reg(0x43);
    gc9a01_write_reg(0x70);
    gc9a01_write_reg(0x72);
    gc9a01_write_reg(0x36);
    gc9a01_write_reg(0x37);  
    gc9a01_write_reg(0x6F);


    gc9a01_write_cmd(0xF2);   
    gc9a01_write_reg(0x45);
    gc9a01_write_reg(0x09);
    gc9a01_write_reg(0x08);
    gc9a01_write_reg(0x08);
    gc9a01_write_reg(0x26);
    gc9a01_write_reg(0x2A);

    gc9a01_write_cmd(0xF3);   
    gc9a01_write_reg(0x43);
    gc9a01_write_reg(0x70);
    gc9a01_write_reg(0x72);
    gc9a01_write_reg(0x36);
    gc9a01_write_reg(0x37); 
    gc9a01_write_reg(0x6F);

    gc9a01_write_cmd(0xED);    
    gc9a01_write_reg(0x1B); 
    gc9a01_write_reg(0x0B); 

    gc9a01_write_cmd(0xAE);            
    gc9a01_write_reg(0x77);

    gc9a01_write_cmd(0xCD);            
    gc9a01_write_reg(0x63);        


    gc9a01_write_cmd(0x70);            
    gc9a01_write_reg(0x07);
    gc9a01_write_reg(0x07);
    gc9a01_write_reg(0x04);
    gc9a01_write_reg(0x0E); 
    gc9a01_write_reg(0x0F); 
    gc9a01_write_reg(0x09);
    gc9a01_write_reg(0x07);
    gc9a01_write_reg(0x08);
    gc9a01_write_reg(0x03);

    gc9a01_write_cmd(0xE8);            
    gc9a01_write_reg(0x34);

    gc9a01_write_cmd(0x62);            
    gc9a01_write_reg(0x18);
    gc9a01_write_reg(0x0D);
    gc9a01_write_reg(0x71);
    gc9a01_write_reg(0xED);
    gc9a01_write_reg(0x70); 
    gc9a01_write_reg(0x70);
    gc9a01_write_reg(0x18);
    gc9a01_write_reg(0x0F);
    gc9a01_write_reg(0x71);
    gc9a01_write_reg(0xEF);
    gc9a01_write_reg(0x70); 
    gc9a01_write_reg(0x70);

    gc9a01_write_cmd(0x63);            
    gc9a01_write_reg(0x18);
    gc9a01_write_reg(0x11);
    gc9a01_write_reg(0x71);
    gc9a01_write_reg(0xF1);
    gc9a01_write_reg(0x70); 
    gc9a01_write_reg(0x70);
    gc9a01_write_reg(0x18);
    gc9a01_write_reg(0x13);
    gc9a01_write_reg(0x71);
    gc9a01_write_reg(0xF3);
    gc9a01_write_reg(0x70); 
    gc9a01_write_reg(0x70);

    gc9a01_write_cmd(0x64);            
    gc9a01_write_reg(0x28);
    gc9a01_write_reg(0x29);
    gc9a01_write_reg(0xF1);
    gc9a01_write_reg(0x01);
    gc9a01_write_reg(0xF1);
    gc9a01_write_reg(0x00);
    gc9a01_write_reg(0x07);

    gc9a01_write_cmd(0x66);            
    gc9a01_write_reg(0x3C);
    gc9a01_write_reg(0x00);
    gc9a01_write_reg(0xCD);
    gc9a01_write_reg(0x67);
    gc9a01_write_reg(0x45);
    gc9a01_write_reg(0x45);
    gc9a01_write_reg(0x10);
    gc9a01_write_reg(0x00);
    gc9a01_write_reg(0x00);
    gc9a01_write_reg(0x00);

    gc9a01_write_cmd(0x67);            
    gc9a01_write_reg(0x00);
    gc9a01_write_reg(0x3C);
    gc9a01_write_reg(0x00);
    gc9a01_write_reg(0x00);
    gc9a01_write_reg(0x00);
    gc9a01_write_reg(0x01);
    gc9a01_write_reg(0x54);
    gc9a01_write_reg(0x10);
    gc9a01_write_reg(0x32);
    gc9a01_write_reg(0x98);

    gc9a01_write_cmd(0x74);            
    gc9a01_write_reg(0x10);    
    gc9a01_write_reg(0x85);    
    gc9a01_write_reg(0x80);
    gc9a01_write_reg(0x00); 
    gc9a01_write_reg(0x00); 
    gc9a01_write_reg(0x4E);
    gc9a01_write_reg(0x00);                    

    gc9a01_write_cmd(0x98);            
    gc9a01_write_reg(0x3e);
    gc9a01_write_reg(0x07);

    gc9a01_write_cmd(0x35);    
    gc9a01_write_cmd(config->dis_invert ? 0x21 : 0x20);

    gc9a01_write_cmd(0x11);
    gc9a01_delay_ms(120);
    gc9a01_write_cmd(0x29);
    gc9a01_delay_ms(20);
}


static void gc9a01_set_index(uint16_t x_start, uint16_t y_start, uint16_t x_end, uint16_t y_end)
{
    uint16_t start_pos, end_pos;
    gc9a01_write_cmd(0x2a);    // CASET (2Ah): Column Address Set 
    // Must write byte than byte
    start_pos = x_start;
    end_pos = x_end;
    gc9a01_write_reg(start_pos >> 8);
    gc9a01_write_reg(start_pos & 0xFF);
    gc9a01_write_reg(end_pos >> 8);
    gc9a01_write_reg(end_pos & 0xFF);

    gc9a01_write_cmd(0x2b);    // RASET (2Bh): Row Address Set
    start_pos = y_start;
    end_pos = y_end;
    gc9a01_write_reg(start_pos >> 8);
    gc9a01_write_reg(start_pos & 0xFF);
    gc9a01_write_reg(end_pos >> 8);
    gc9a01_write_reg(end_pos & 0xFF); 
    gc9a01_write_cmd(0x2c);    // RAMWR (2Ch): Memory Write 
}


esp_err_t gc9a01_deinit(gc9a01_handle_t *handle)
{
    free(gc9a01_obj);
    return ESP_OK;
}

esp_err_t gc9a01_init(gc9a01_handle_t *handle, gc9a01_config_t *config)
{
    if (handle == NULL || config == NULL) {
        ESP_LOGE(TAG, "arg error\n");
        return ESP_FAIL;
    }
    gc9a01_obj = (gc9a01_obj_t *)heap_caps_calloc(1, sizeof(gc9a01_obj_t), MALLOC_CAP_DEFAULT);
    if (!gc9a01_obj) {
        ESP_LOGE(TAG, "lcm object malloc error\n");
        return ESP_FAIL;
    }

    memcpy(&gc9a01_obj->config, config, sizeof(gc9a01_config_t));
    gc9a01_obj->write_cb = config->write_cb;
    if (gc9a01_obj->write_cb == NULL) {
        ESP_LOGE(TAG, "lcm callback NULL\n");
        gc9a01_deinit(handle);
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
    gc9a01_set_level(gc9a01_obj->config.pin.rd, 1, gc9a01_obj->config.invert.rd);
    gc9a01_set_level(gc9a01_obj->config.pin.cs, 1, gc9a01_obj->config.invert.cs);
    gc9a01_rst();//gc9a01_rst before LCD Init.
    gc9a01_delay_ms(100);
    gc9a01_set_level(gc9a01_obj->config.pin.cs, 0, gc9a01_obj->config.invert.cs);
    gc9a01_config(config);
    gc9a01_set_level(gc9a01_obj->config.pin.bk, 1, gc9a01_obj->config.invert.bk);
    handle->set_index = gc9a01_set_index;
    handle->write_data = gc9a01_write_data;
    return ESP_OK;
}
