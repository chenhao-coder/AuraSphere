#ifndef UI_TASK_H
#define UI_TASK_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>

typedef enum {
    UI_PAGE_STARTUP = 0,
    UI_PAGE_RHYTHM,
    UI_PAGE_ANIMATION,
    UI_PAGE_ALARM_SET,
    UI_PAGE_BRIGHTNESS_SET,
    UI_PAGE_MAX
} UiPage_t;

/* UI 消息结构（页面切换/数据更新 */
typedef enum {
    UI_MSG_SWITCH_PAGE,
    UI_MSG_SET_BRIGHTNESS,
    UI_MSG_UPDATE_DATA,
} UiMsgType_t;

typedef struct {
    UiMsgType_t type;
    UiPage_t    page;

    union {
        uint8_t brightness;
        struct {
            uint8_t hour;
            uint8_t min;
        } alarm;
    }data;
} UiMsg_t;

typedef struct {
    UiPage_t current_page;
    uint32_t tick;
} UiContext;

void ui_page_enter(UiPage_t page);
void ui_page_exit(UiPage_t page);
void ui_page_draw(UiContext *ctx);
void ui_handle_msg(UiContext *ctx, UiMsg_t *msg);

#ifdef __cplusplus
}
#endif

#endif // UI_TASK_H