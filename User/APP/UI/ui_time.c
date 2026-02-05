#include "matrix.h"
#include <stdio.h>

#include "ui_time.h"

Color mainColor;
Color weekColor;

void UI_DrawTime(const TimeInfo *t, TimePage_t page)
{
    char buf[9];

    Matrix_Clear();

    if(page == TIME_PAGE_HMS)
    {
        // HH:MM:SS
        snprintf(buf, sizeof(buf), "%02d:%02d:%02d",
                    t->hour, t->min, t->sec);
        Matrix_DrawText(2, 5, buf, mainColor);

        // ÐÇÆÚÌõ£¨7¸ñ£©
        for(int i = 1; i <= 7; i++)
        {
            Color c = (i == t->week) ? mainColor : weekColor;
            Matrix_DrawFastVLine(2 + (i - 1) * 4, 7, 3, c);
        }
    }
    else if(page == TIME_PAGE_HM)
    {
        // HH:MM
        snprintf(buf, sizeof(buf), "%02d:%02d",
                    t->hour, t->min);
        Matrix_DrawText(4, 5, buf, mainColor);

        for(int i = 1; i <= 7; i++)
        {
            Color c = (i == t->week) ? mainColor : weekColor;
            Matrix_DrawFastVLine(3 + (i - 1) * 3, 7, 2, c);
        }
    }
    else if (page == TIME_PAGE_DATE)
    {
        // MM-DD
        snprintf(buf, sizeof(buf), "%02d-%02d",
                 t->month, t->day);

        Matrix_DrawText(3, 5, buf, mainColor);

        for (int i = 1; i <= 7; i++)
        {
            Color c = (i == t->week) ? mainColor : weekColor;
            Matrix_DrawFastHLine(3 + (i - 1) * 3, 7, 2, c);
        }
    }

    Matrix_Show();
}