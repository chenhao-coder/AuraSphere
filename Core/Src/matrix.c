#include "matrix.h"
#include "font5x7.h"
#include <string.h>

#include "ws2812b.h"

#define FONT_W      5
#define FONT_H      7
#define FONT_GAP    1

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

void Matrix_DrawChar(int x, int y, char ch, Color c)
{
    if(ch < 32 || ch > 126) return;

    const uint8_t *bitmap = Font5x7[ch - 32];

    for(int col = 0; col < FONT_W; col++)
    {
        uint8_t bits = bitmap[col];

        for(int row = 0; row < FONT_H; row++)
        {
            if(bits & ( 1 << row))
            {
                Matrix_SetPixel(x + col, y + row, c);
            }
        }
    }
}

void Matrix_DrawText(int x, int y, const char *text, Color c)
{
    int cursorX = x;

    while(*text)
    {
        Matrix_DrawChar(cursorX, y, *text, c);
        cursorX += FONT_W + FONT_GAP;
        text++;
    }

    Matrix_Show();
}