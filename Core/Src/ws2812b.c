#include "ws2812b.h"
#include <stdio.h>
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
    if(led_num > WS2812B_NUM_LEDS) return;

    // 每个LED使用3个SPI字节
    uint8_t *ptr = &spi_buffer[led_num * BYTES_PER_LED];

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
    for(uint8_t i = 0; i < WS2812B_NUM_LEDS; i++)
    {
        WS2812B_SetColor(i, 0, 0, 0);
    }
    WS2812B_Update();
}



