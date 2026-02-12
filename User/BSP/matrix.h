#ifndef MATRIX_H_
#define MATRIX_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

#define MATRIX_W    16
#define MATRIX_H    8

typedef struct {
    uint8_t r;
    uint8_t g;
    uint8_t b;
} Color;

void Matrix_Clear(void);
void Matrix_SetPixel(int x, int y, Color c);
void Matrix_DrawVBar(int x, int height, Color c);
void Matrix_DrawFastVLine(int x, int y, int h, Color c);
void Matrix_Show(void);
void Matrix_DrawText(int x, int y, const char *text, Color c);

#ifdef __cplusplus
}
#endif

#endif