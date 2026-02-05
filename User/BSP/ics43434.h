#ifndef ICS43434_H
#define ICS43434_H

#ifdef __cplusplus
extern "C" {
#endif

#include "i2s.h"

#define ICS43434_BUFFER_SIZE    (I2S2_BUFFER_SIZE / 2)
#define RING_DEPTH              (4)

typedef struct
{
    int32_t pcm[ICS43434_BUFFER_SIZE];
}AudioFrame_t;

typedef struct
{
    AudioFrame_t frames[RING_DEPTH];

    volatile uint8_t write_idx;
    volatile uint8_t read_idx;
    volatile uint8_t count;

    volatile uint32_t drop_count;       // 统计丢帧（调试用）
}AudioRingBuffer_t;

typedef struct
{
    int32_t pcm[ICS43434_BUFFER_SIZE];
} Ics43434_Frame_t;

void AudioRing_Init(AudioRingBuffer_t *rb);
void AudioRing_PushFromISR(AudioRingBuffer_t *rb, const int32_t *pcm);
int AudioRing_Pop(AudioRingBuffer_t *rb, AudioFrame_t **out);
void I2S_DMA_Callback(const int32_t *dma_buf);

void Ics43434_Init(void);
int  Ics43434_GetFrame(Ics43434_Frame_t **out);
uint32_t Ics43434_GetDropCount(void);

#ifdef __cplusplus
}
#endif

#endif // ICS43434_H


