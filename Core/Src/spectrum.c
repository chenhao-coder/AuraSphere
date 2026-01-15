#include "spectrum.h"
#include "matrix.h"
#include <stdlib.h>

static RhythmMode currentMode = RHYTHM_MODEL1;

static uint8_t peak[MATRIX_W] = {0};
static uint32_t lastPeakDecay = 0;
static uint32_t lastColorChange = 0;
static uint16_t colorTime = 0;

uint32_t millis(void)
{
    return HAL_GetTick();
}

static Color hsv2rgb(uint16_t h, uint8_t s, uint8_t v)
{
    float hf = h / 60.0f;
    int i = (int)hf;
    float f = hf - i;

    uint8_t p = v * (1.0f - s / 100.0f);
    uint8_t q = v * (1.0f - s / 100.0f * f);
    uint8_t t = v * (1.0f - s / 100.0f * (1.0f - f));

    switch (i % 6) {
        case 0: return (Color){v, t, p};
        case 1: return (Color){q, v, p};
        case 2: return (Color){p, v, t};
        case 3: return (Color){p, q, v};
        case 4: return (Color){t, p, v};
        case 5: return (Color){v, p, q};
    }
    return (Color){0,0,0};
}

static Color getGradientColor(uint8_t y)
{
    uint8_t t = (y * 255) / (MATRIX_H - 1);

    Color c;
    c.r = t;
    c.b = 255 - t;
    c.g = 255 - abs(c.r - c.b);

    // 限亮40
    c.r = (c.r * 40) / 255;
    c.g = (c.g * 40) / 255;
    c.b = (c.b * 40) / 255;
}

void Spectrum_SetMode(RhythmMode mode)
{
    currentMode = mode;
}
void Spectrum_Draw(uint8_t *bands)
{
    Matrix_Clear();

    for(int band = 0; band < MATRIX_W; band++)
    {
        int barHeight = bands[band];
        if (barHeight > MATRIX_H) barHeight = MATRIX_H;

        // 峰值逻辑
        if (barHeight > peak[band])
            peak[band] = barHeight;

        int x = MATRIX_W - 1 - band;

        switch (currentMode)
        {
            case RHYTHM_MODEL1:
            {
                Color c = hsv2rgb((band * 360) / MATRIX_W + colorTime, 100, 100);
                Matrix_DrawFastVLine(x, MATRIX_H - barHeight, barHeight, c);

                // 峰值点
                Matrix_SetPixel(x, MATRIX_H - peak[band] - 1, (Color){150,150,150});
            }
            break;

            case RHYTHM_MODEL2:
            {
                // 你可以自定义渐变
                for (int y = 0; y < barHeight; y++)
                {
                    uint8_t t = (y * 255) / (MATRIX_H - 1);
                    Color c = {t, 255 - t, 100};
                    Matrix_SetPixel(x, MATRIX_H - 1 - y, c);
                }

                Matrix_SetPixel(x, MATRIX_H - peak[band] - 1, (Color){150,150,150});
            }
            break;

            case RHYTHM_MODEL3:
            {
                for (int y = 0; y < barHeight; y++)
                {
                    Color c = hsv2rgb(y * 20 + colorTime, 100, 100);
                    Matrix_SetPixel(x, MATRIX_H - 1 - y, c);
                }
            }
            break;

            case RHYTHM_MODEL4:
            {
                Matrix_SetPixel(x, MATRIX_H - peak[band] - 1,
                                (Color){255, 255 - peak[band]*20, 0});
            }
            break;

            default:
                break;
        }
    }

    // 峰值下落
    if ((millis() - lastPeakDecay) >= 70)
    {
        for (int i = 0; i < MATRIX_W; i++)
        {
            if (peak[i] > 0) peak[i]--;
        }
        lastPeakDecay = millis();
    }

    // 颜色变化
    if ((millis() - lastColorChange) >= 10)
    {
        colorTime++;
        lastColorChange = millis();
    }

    Matrix_Show();
}