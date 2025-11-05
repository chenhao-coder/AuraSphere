#ifndef WS2812B_H
#define WS2812B_H

#ifdef __cplusplus
extern "C" {
#endif

#include "main.h"
#include "spi.h"

#define WS2812B_NUM_LEDS        8

// 每个SPI需要3个SPI字节，加上复位脉冲
#define BYTES_PER_LED           12
#define RESET_BYTES             80
#define WS2812B_BUFFER_SIZE     (WS2812B_NUM_LEDS * BYTES_PER_LED + RESET_BYTES)

// 编码常量
#define WS2812B_BIT_0           0x08
#define WS2812B_BIT_1           0x0C

typedef struct {
    uint8_t green;
    uint8_t red;
    uint8_t blue;
}WS2812B_Color_t;

// 函数声明
void WS2812B_Init(void);
void WS2812B_SetColor(uint8_t led_num, uint8_t red, uint8_t green, uint8_t blue);
void WS2812B_SetColorRGB(uint8_t led_num, WS2812B_Color_t color);
void WS2812B_Update(void);
void WS2812B_Clear(void);

#ifdef __cplusplus
}
#endif

#endif






