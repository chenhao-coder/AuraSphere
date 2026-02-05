#include "ics43434.h"
#include <string.h>
#include <stdio.h>

#define TIME_NOISE_GATE 600  // 经验值，建议串口打印微调

static AudioRingBuffer_t s_audio_rb;

void Ics43434_Init(void)
{
    AudioRing_Init(&s_audio_rb);
}

void AudioRing_Init(AudioRingBuffer_t *rb)
{
    rb->write_idx = 0;
    rb->read_idx = 0;
    rb->count = 0;
    rb->drop_count = 0;
}

void AudioRing_PushFromISR(AudioRingBuffer_t *rb, const int32_t *pcm)
{
    if(rb->count == RING_DEPTH)
    {
        // 丢最老的一帧
        rb->read_idx = (rb->read_idx + 1) % RING_DEPTH;
        rb->count--;
        rb->drop_count++;
    }

    AudioFrame_t *f = &rb->frames[rb->write_idx];
    memcpy(f->pcm, pcm, sizeof(f->pcm));

    rb->write_idx = (rb->write_idx + 1) % RING_DEPTH;
    rb->count++;
}

int AudioRing_Pop(AudioRingBuffer_t *rb, AudioFrame_t **out)
{
    if(rb->count == 0)
        return 0;
    
    *out = &rb->frames[rb->read_idx];

    rb->read_idx = (rb->read_idx + 1) % RING_DEPTH;
    rb->count--;

    return 1;
}

void I2S_DMA_Callback(const int32_t *dma_buf)
{
    static int32_t pcm[ICS43434_BUFFER_SIZE];

    // 解包 + 符号扩展
    for(uint16_t i = 0; i < ICS43434_BUFFER_SIZE; i++)
    {
        int32_t sample = (dma_buf[i * 2]) & 0xFFFFFF;
        // PRINT("sample: %lx", sample);

        if(sample & 0x00800000)
        {
            sample = sample | 0xFF000000;
        }

        if(sample > -TIME_NOISE_GATE && sample < TIME_NOISE_GATE)
        {
            sample = 0;
        }

        pcm[i] = sample;
        // PRINT("ICS43434: %ld", ics43434_out[i]);
    }

    AudioRing_PushFromISR(&s_audio_rb, pcm);
}

int Ics43434_GetFrame(Ics43434_Frame_t **out)
{
    AudioFrame_t *f;
    if(AudioRing_Pop(&s_audio_rb, &f))
    {
        *out = (Ics43434_Frame_t *)f;
        return 1;
    }
    return 0;
}

uint32_t Ics43434_GetDropCount(void)
{
    return s_audio_rb.drop_count;
}







