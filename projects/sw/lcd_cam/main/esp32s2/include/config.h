#pragma once
#include "driver/gpio.h"

#ifdef __cplusplus
extern "C" {
#endif

#define LCD_FRE   (5000000)
#define LCD_BIT   (8)
#define LCD_WIDTH (540)
#define LCD_HIGH  (960)

#define LCD_WR  (GPIO_NUM_34)
#define LCD_RS  (GPIO_NUM_1)
#define LCD_RD  (GPIO_NUM_2)
#define LCD_CS  (GPIO_NUM_21)
#define LCD_RST (GPIO_NUM_18)
#define LCD_BK  (-1)

#define LCD_D0  (GPIO_NUM_35)
#define LCD_D1  (GPIO_NUM_37)
#define LCD_D2  (GPIO_NUM_36)
#define LCD_D3  (GPIO_NUM_39)
#define LCD_D4  (GPIO_NUM_38)
#define LCD_D5  (GPIO_NUM_41)
#define LCD_D6  (GPIO_NUM_40)
#define LCD_D7  (GPIO_NUM_45)

#ifdef __cplusplus
}
#endif