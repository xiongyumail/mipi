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

#pragma once

#include "esp_types.h"
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Structure to store config information of gc9a01 lcm driver
 */
typedef struct {
    uint8_t  width;                              /*!< Data bus width 1: 1bit, 8: 8bit, 16: 16bit */
    struct {
        int8_t dc;                               /*!< DC output pin */
        int8_t rd;                               /*!< RD output pin */
        int8_t cs;                               /*!< CS output pin */
        int8_t rst;                              /*!< RST output pin */
        int8_t bk;                               /*!< BK output pin */
    } pin;                                       /*!< Pin configuration */
    struct {
        bool dc;                                 /*!< DC output signal inversion */
        bool rd;                                 /*!< RD output signal inversion */
        bool cs;                                 /*!< CS output signal inversion */
        bool rst;                                /*!< RST output signal inversion */
        bool bk;                                 /*!< BK output signal inversion */
    } invert;                                    /*!< Signal inversion configuration */
    uint8_t horizontal;                          /*!< Screen orientation */
    uint8_t dis_invert;                          /*!< Display inversion */
    uint8_t dis_bgr;                             /*!< bgr exchange */
    void (*write_cb)(uint8_t *data, size_t len); /*!< Write data callback function */
} gc9a01_config_t;

/**
 * @brief Structure to store handle information of gc9a01 lcm driver
 */
typedef struct {
    /**
     * @brief Write data
     *
     * @param data Data pointer
     * @param len Write data length, unit: byte
     */
    void (*write_data)(uint8_t *data, size_t len);

    /**
     * @brief Set image coordinates
     *
     * @param x_start Horizontal start coordinate
     * @param y_start Vertical start coordinate
     * @param x_end Horizontal end coordinate
     * @param y_end Vertical end coordinate
     */
    void (*set_index)(uint16_t x_start, uint16_t y_start, uint16_t x_end, uint16_t y_end);
} gc9a01_handle_t;

/**
 * @brief Uninitialize the gc9a01 lcm driver
 *
 * @param handle Provide handle pointer to release resources
 *
 * @return
 *     - ESP_OK Success
 *     - ESP_FAIL Uninitialize fail
 */
esp_err_t gc9a01_deinit(gc9a01_handle_t *handle);

/**
 * @brief Initialize the gc9a01 lcm driver
 *
 * @param handle Return handle pointer after successful initialization - see lcd_cam_handle_t struct
 * @param config Configurations - see lcd_cam_config_t struct
 *
 * @return
 *     - ESP_OK Success
 *     - ESP_ERR_INVALID_ARG Parameter error
 *     - ESP_ERR_NO_MEM No memory to initialize lcd_cam
 *     - ESP_FAIL Initialize fail
 */
esp_err_t gc9a01_init(gc9a01_handle_t *handle, gc9a01_config_t *config);

#ifdef __cplusplus
}
#endif