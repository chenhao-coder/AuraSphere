#include "clock_test.h"
#include <time.h>
#include <stdio.h>

void RTC_SetFromUnix(uint32_t unix_time)
{
    struct tm tm_time;
    time_t t = unix_time;

    localtime_r(&t, &tm_time);

    RTC_TimeTypeDef sTime = {0};
    RTC_DateTypeDef sDate = {0};

    sTime.Hours = tm_time.tm_hour;
    sTime.Minutes = tm_time.tm_min;
    sTime.Seconds = tm_time.tm_sec;

    sDate.Year = tm_time.tm_year - 100;
    sDate.Month = tm_time.tm_mon + 1;
    sDate.Date = tm_time.tm_mday;
    sDate.WeekDay = tm_time.tm_wday + 1;

    HAL_RTC_SetTime(&hrtc, &sTime, RTC_FORMAT_BIN);
    HAL_RTC_SetDate(&hrtc, &sDate, RTC_FORMAT_BIN);
}

void RTC_PrintTime(void)
{
    RTC_TimeTypeDef sTime;
    RTC_DateTypeDef sDate;

    HAL_RTC_WaitForSynchro(&hrtc);
    HAL_RTC_GetTime(&hrtc, &sTime, RTC_FORMAT_BIN);
    HAL_RTC_GetDate(&hrtc, &sDate, RTC_FORMAT_BIN); 

    printf("RTC Time: 20%02d-%02d-%02d:%02d:%02d:%02d\r\n",
            sDate.Year,
            sDate.Month,
            sDate.Date,
            sTime.Hours,
            sTime.Minutes,
            sTime.Seconds);
}

void RTC_SetAndVerifyTest(uint32_t unix_time)
{
    printf("\r\n======RTC Set & Verify Test =====\r\n");

    printf("Set RTC from unix time: %lu\r\n", unix_time);
    RTC_SetFromUnix(unix_time);

    /* 等待RTC同步稳定*/
    HAL_Delay(100);

    printf("RTC Readback #1:\r\n");
    RTC_PrintTime();

    /* 延时两秒 */
    HAL_Delay(10000);

    printf("RTC Readback #2 (+10s expected):\r\n");
    RTC_PrintTime();

    printf("===== RTC Test End =====\r\n\r\n");
}