#include "ws2812b.h"
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>

static uint8_t spi_buffer[WS2812B_BUFFER_SIZE] = {0};

// 设置单个字节到SPI缓冲区（核心编码函数）
static uint8_t *set_pixel_byte(uint8_t *ptr, uint8_t value)
{
    // 初始化4个SPI字节的模板
    ptr[0] = 0x00;
    ptr[1] = 0x00;
    ptr[2] = 0x00;
    ptr[3] = 0x00;

    for(uint8_t i = 0; i < 8; i++)
    {
        ptr[i / 2] <<= 4;

        if(value & 0x80)
        {
            ptr[i / 2] |= WS2812B_BIT_1;
        }
        else
        {
            ptr[i / 2] |= WS2812B_BIT_0;
        }
        
        value <<= 1;
    }
    
    return ptr + 4;
}

void WS2812B_Init(void)
{
    // 初始化缓冲区位0（复位信号）
    for(int i = 0; i < WS2812B_BUFFER_SIZE; i ++)
    {
        spi_buffer[i] = 0;
    }
    WS2812B_Update();
}

void WS2812B_SetColor(uint8_t led_num, uint8_t red, uint8_t green, uint8_t blue)
{
    if(led_num >= NUM_LEDS) return;

    // 每个LED使用3个SPI字节
    uint8_t *ptr = &spi_buffer[RESET_BYTES + led_num * BYTES_PER_LED];

    // WS2812B使用GRB顺序
    ptr = set_pixel_byte(ptr, green);
    ptr = set_pixel_byte(ptr, red);
    ptr = set_pixel_byte(ptr, blue);
}

void WS2812B_Set_ColorRGB(uint8_t led_num, WS2812B_Color_t color)
{
    WS2812B_SetColor(led_num, color.red, color.green, color.blue);
}

void WS2812B_Update(void)
{
    while(HAL_SPI_GetState(&hspi1) == HAL_SPI_STATE_BUSY_TX);
    HAL_SPI_Transmit_DMA(&hspi1, spi_buffer, WS2812B_BUFFER_SIZE);
}

void WS2812B_Clear(void)
{
    for(uint8_t i = 0; i < NUM_LEDS; i++)
    {
        WS2812B_SetColor(i, 0, 0, 0);
    }
    WS2812B_Update();
}

void LED_DisplaySpectrum(uint8_t *spectrum)
{
    WS2812B_Clear();

    for(uint8_t x = 0; x < LED_COLS; x++)
    {
        uint8_t height = spectrum[x];
        if(height > LED_ROWS) height = LED_ROWS;

        for(uint8_t y = 0; y < height; y++)
        {
            uint8_t led_index = x + y * LED_ROWS;  // ← 普通布线正确公式

            // 渐变 & 限亮到 40
            uint8_t t = (y * 255) / (LED_ROWS - 1);
            uint8_t r = t;
            uint8_t b = 255 - t;
            uint8_t g = 255 - abs(r - b);

            r = (r * 40) / 255;
            g = (g * 40) / 255;
            b = (b * 40) / 255;

            WS2812B_SetColor(led_index, r, g, b);
        }
    }

    WS2812B_Update();
}

void LED_DisplaySpectrum_Test(void)
{
    uint8_t spectrum[LED_COLS] = {2, 4, 6, 8, 7, 5, 3, 1};
    LED_DisplaySpectrum(spectrum);
}


