#ifndef ICS43434_H
#define ICS43434_H

#ifdef __cplusplus
extern "C" {
#endif

#include "i2s.h"

#define ICS43434_BUFFER_SIZE    (I2S2_BUFFER_SIZE / 2)

void Ics43434_Out_Process(int32_t *data);
int32_t *Ics43434_Get_Output_Ptr(void);

#ifdef __cplusplus
}
#endif

#endif // ICS43434_H