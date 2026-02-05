#ifndef CLOCK_TEST_H
#define CLOCK_TEST_H

#ifdef __cplusplus
extern "C" {
#endif

#include "rtc.h"

void RTC_SetFromUnix(uint32_t unix_time);
void RTC_PrintTime(void);
void RTC_SetAndVerifyTest(uint32_t unix_time);

#ifdef __cplusplus
}
#endif

#endif // CLOCK_H