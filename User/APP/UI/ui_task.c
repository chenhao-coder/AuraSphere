#include "ui_task.h"
#include "matrix.h"
#include "audio_visual_processor.h"
#include "cmsis_os.h"
#include <stdio.h>
#include "spectrum.h"

typedef struct
{
    uint32_t enter_tick;
    uint8_t anim_step;
    bool    finished;
} UiStartupState;

static UiStartupState g_startup;

extern osMessageQueueId_t FFT_queueHandle;

static void ui_startup_enter(void)
{
    g_startup.enter_tick = 0;
    g_startup.anim_step = 0;
    g_startup.finished = false;
}

static void ui_startup_exit(void)
{

}

static void ui_draw_startup(UiContext *ctx)
{
    char buf[16];
    Color c = {255, 0, 0};

    /* 每 30ms tick 一次 */
    if(ctx->tick % 10 == 0)     // 300ms
    {
        g_startup.anim_step++;
    }

    snprintf(buf, sizeof(buf), "%.*s", 
            (g_startup.anim_step % 3) + 1, 
            "...");

    printf(">>> ui_draw_startup: %s", buf);

    Matrix_Clear();
    Matrix_DrawText(0, 0, buf, c);
    
    /* 启动动画持续 3 秒 */
    if(ctx->tick > 100) 
    {
        UiMsg_t msg = {
            .type = UI_MSG_SWITCH_PAGE,
            .page = UI_PAGE_RHYTHM,
        };
        osMessageQueuePut(FFT_queueHandle, &msg, 0, 0);
    }
}

static void ui_draw_rhythm(UiContext *ctx)
{
    SpectrumData snapshot;

    taskENTER_CRITICAL();
    snapshot = g_spectrum;
    taskEXIT_CRITICAL();
    Spectrum_Draw(snapshot.bar);
}

void ui_page_enter(UiPage_t page)
{
    switch(page)
    {
        case UI_PAGE_STARTUP:
        {
            ui_startup_enter();
        }
        break;

        case UI_PAGE_RHYTHM:
        {
            // ui_rhythm_enter();
        }
        break;

        default:
        {

        }
        break;
    }
}

void ui_page_exit(UiPage_t page)
{
    switch(page)
    {
        case UI_PAGE_STARTUP:
        {
            ui_startup_exit();
        }
        break;

        default:
        {

        }
        break;
    }
}

void ui_page_draw(UiContext *ctx)
{
    switch(ctx->current_page)
    {
        case UI_PAGE_STARTUP:
        {
            ui_draw_startup(ctx);
        }
        break;

        case UI_PAGE_RHYTHM:
        {
            ui_draw_rhythm(ctx);
        }
        break;

        case UI_PAGE_ANIMATION:
        {
            // ui_draw_animation(ctx);
        }
        break;

        case UI_PAGE_ALARM_SET:
        {
            // ui_draw_alarm(ctx);
        }
        break;

        case UI_PAGE_BRIGHTNESS_SET:
        {
            // ui_draw_brightness(ctx);
        }
        break;
        
        default:
        break;
    }
}

/* UI 消息处理（把“变化”集中在一处 */
void ui_handle_msg(UiContext *ctx, UiMsg_t *msg)
{
    switch(msg->type)
    {
        case UI_MSG_SWITCH_PAGE:
        {
            ui_page_exit(ctx->current_page);
            ctx->current_page = msg->page;
            ui_page_enter(ctx->current_page);
        }
        break;

        case UI_MSG_SET_BRIGHTNESS:
        {

        }
        break;

        default:
        break;
    }
}


