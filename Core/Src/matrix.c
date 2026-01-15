#include "matrix.h"
#include "ws2812b.h"
#include <string.h>

static inline int XYtoIndex(int x, int y)
{
    return y * MATRIX_W + x;
}

void Matrix_Clear(void)
{
    WS2812B_Clear();
}

void Matrix_SetPixel(int x, int y, Color c)
{
    if(x < 0 || x >= MATRIX_W || y < 0 || y >= MATRIX_H) return;

    int index = XYtoIndex(x, y);
    WS2812B_SetColor(index, c.r, c.g, c.b);
}

void Matrix_DrawBar(int x, int height, Color c)
{
    if(height > MATRIX_H) height = MATRIX_H;

    for(int i = 0; i < height; i++)
    {
        Matrix_SetPixel(x, MATRIX_H - 1 - i, c);
    }
}

void Matrix_DrawFastVLine(int x, int y, int h, Color c)
{
    for (int i = 0; i < h; i++)
    {
        Matrix_SetPixel(x, y + i, c);
    }
}

void Matrix_Show(void)
{
    WS2812B_Update();
}