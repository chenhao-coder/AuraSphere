#ifndef UI_TIME_H
#define UI_TIME_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

typedef struct 
{
    uint8_t hour;
    uint8_t min;
    uint8_t sec;
    uint8_t week;
    uint8_t month;
    uint8_t day;
} TimeInfo;

typedef enum
{
    TIME_PAGE_HMS = 0,
    TIME_PAGE_HM,
    TIME_PAGE_DATE,
    TIME_PAGE_MAX
} TimePage_t;

void UI_DrawTime(const TimeInfo *t, TimePage_t page);

#ifdef __cplusplus
}
#endif

#endif // UI_TIME_H