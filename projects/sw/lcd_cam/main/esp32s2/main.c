#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include "esp_wifi.h"
#include "esp_system.h"
#include "nvs_flash.h"
#include "esp_event_loop.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "ov2640.h"
#include "ov3660.h"
#include "sensor.h"
#include "sccb.h"
#include "driver/gpio.h"
#include "driver/lcd_cam.h"
#include "ssd2805.h"
#include "jpeg.h"
#include "config.h"
#include "ft5x06.h"

static const char *TAG = "main";

#define LCD_RATE_TEST   (1)

static void lcd_cam_task(void *arg)
{
    lcd_cam_handle_t lcd_cam;
    lcd_cam_config_t lcd_cam_config = {
        .lcd = {
            .en = true,
            .width = LCD_BIT,
            .fre = LCD_FRE,
            .pin = {
                .clk  = LCD_WR,
                .data = {LCD_D0, LCD_D1, LCD_D2, LCD_D3, LCD_D4, LCD_D5, LCD_D6, LCD_D7},
            },
            .invert = {
                .clk  = false,
                .data = {false, false, false, false, false, false, false, false},
            },
            .max_dma_buffer_size = 16 * 1024,
            .swap_data = false
        },
        .cam = {
            .en = false,
        }
    };

    lcd_cam_init(&lcd_cam, &lcd_cam_config);

    ssd2805_handle_t ssd2805;
    ssd2805_config_t ssd2805_config = {
        .width = LCD_BIT,
        .pin = {
            .dc  = LCD_RS,
            .rd = LCD_RD,
            .cs = LCD_CS,
            .rst = LCD_RST,
            .bk = LCD_BK,
        },
        .invert = {
            .dc  = false,
            .rd = false,
            .cs = false,
            .rst = false,
            .bk = false,
        },
        .horizontal = 0, // 2: UP, 3： DOWN
        .dis_invert = true,
        .dis_bgr = false,
        .write_cb = lcd_cam.lcd.write_data,
    };
    ssd2805_init(&ssd2805, &ssd2805_config);

    uint8_t *img_buf = (uint8_t *)heap_caps_malloc(sizeof(uint16_t) * LCD_WIDTH * LCD_HIGH, MALLOC_CAP_SPIRAM);

    extern const uint8_t pic[];
    for (int y = 0; y < LCD_HIGH; y++) {
        for (int x = 0; x < LCD_WIDTH * 2; x++) {
            img_buf[y * (LCD_WIDTH * 2) + x] = pic[y * (800 * 2) + x];
        }  
    }
    ssd2805.set_index(0, 0, LCD_WIDTH - 1, LCD_HIGH - 1);
    ssd2805.write_data((uint8_t *)img_buf, LCD_WIDTH * LCD_HIGH * 2);
    uint32_t ticks_now = 0, ticks_last = 0;
    struct timeval now;   
    while (LCD_RATE_TEST) {
        gettimeofday(&now, NULL);
        ticks_last = now.tv_sec * 1000 + now.tv_usec / 1000;
        ssd2805.set_index(0, 0, LCD_WIDTH - 1, LCD_HIGH - 1);
        ssd2805.write_data((uint8_t *)img_buf, LCD_WIDTH * LCD_HIGH * 2);
        gettimeofday(&now, NULL);
        ticks_now = now.tv_sec * 1000 + now.tv_usec / 1000;
        if (ticks_now - ticks_last > 0) {
            printf("fps: %.2f\n", 1000.0 / (int)(ticks_now - ticks_last));
        }
    }
    free(img_buf);
    lcd_cam.lcd.swap_data(true);

// fail:
    ssd2805_deinit(&ssd2805);
    lcd_cam_deinit(&lcd_cam);
    vTaskDelete(NULL);
}

void app_main(void) 
{
    xTaskCreate(lcd_cam_task, "lcd_cam_task", 4096, NULL, 5, NULL);
}