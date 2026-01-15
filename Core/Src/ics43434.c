#include "ics43434.h"
#include <string.h>
#include <stdio.h>

#define TIME_NOISE_GATE 600  // 经验值，建议串口打印微调

static int32_t ics43434_out[ICS43434_BUFFER_SIZE];

void Ics43434_Out_Process(int32_t *data)
{
    for(uint16_t i = 0; i < ICS43434_BUFFER_SIZE; i++)
    {
        int32_t sample = (data[i * 2]) & 0xFFFFFF;
        // PRINT("sample: %lx", sample);

        if(sample & 0x00800000)
        {
            sample = sample | 0xFF000000;
        }

        if(sample > -TIME_NOISE_GATE && sample < TIME_NOISE_GATE)
        {
            sample = 0;
        }

        ics43434_out[i] = sample;
        // PRINT("ICS43434: %ld", ics43434_out[i]);
    }
}

int32_t *Ics43434_Get_Output_Ptr(void)
{
    return ics43434_out;
}


// 错误码解析函数
void Print_HAL_Status(HAL_StatusTypeDef status)
{
    switch(status)
    {
        case HAL_OK:
            printf("HAL_OK - Operation completed successfully\r\n");
            break;
        case HAL_ERROR:
            printf("HAL_ERROR - Generic error\r\n");
            break;
        case HAL_BUSY:
            printf("HAL_BUSY - Peripheral or resource busy\r\n");
            break;
        case HAL_TIMEOUT:
            printf("HAL_TIMEOUT - Timeout error\r\n");
            break;
        default:
            printf("Unknown status: %d\r\n", status);
            break;
    }
}






