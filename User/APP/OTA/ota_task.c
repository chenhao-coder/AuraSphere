#include "cJSON.h"
#include "ota_flag.h"
#include "at_driver.h"
#include "mqtt_task.h"
#include "cmsis_os.h"

/* ── MD5 上报辅助（用 mbedTLS）── */
#include "md5.h"

void OTA_ReportProgress(int step)
{
    char payload[256], cmd[512];
    const char *desc = (step >= 0 && step < 100) ? "downloading" : 
                        (step == 100) ? "success" : "failed";
    snprintf(payload, sizeof(payload), 
            "{\"id\":\"%lu\",\"Params\":{\"step\":\"%d\",\"desc\":\"%s\"}}",
            (uint32_t)osKernelGetTickCount(), step, desc);
    snprintf(cmd, sizeof(cmd),
            "AT+MQTTPUB=0,\"%s\",\"%s\",1,0",
            OTA_TOPIC_IN, payload);
    AT_Send(cmd, "OK", 3000);
}

void OTA_Task(void *arg)
{
    char json_buf[512];
    for(;;)
    {
        /* 等待 MQTT 任务推来的 OTA 通知 */
        if(osMessageQueueGet(ota_notify_queue, json_buf, 0, portMAX_DELAY) != osOK)
        {
            continue;
        } 

        /* 解析 JSON */
        cJSON *root = cJSON_Parse(json_buf);
        if(!root) continue;
        cJSON *data     = cJSON_GetObjectItem(root, "data");
        char *url       = cJSON_GetObjectItem(data, "url")->valuestring;
        char *md5_exp   = cJSON_GetObjectItem(data, "md5")->valuestring;
        uint32_t size   = cJSON_GetObjectItem(data, "size")->valueint;
        char version[32];
        strncpy(version, cJSON_GetObjectItem(data, "version")->valuestring, 31);
        cJSON_Delete(root);
        printf("[OTA] New firmware: %s size=%lu\r\n", version, size);
        OTA_ReportProgress(0);   

        /* ── 分块下载 ── */
        /* 第一包：先整片擦除 Bank2 */
        FLASH_EraseInitTypeDef erase = {
            .TypeErase  = FLASH_TYPEERASE_MASSERASE,
            .Banks      = FLASH_BANK_2
        };
        uint32_t err;
        HAL_FLASH_Unlock();
        HAL_FLASHEx_Erase(&erase, &err);
        HAL_FLASH_Lock();

        mbedtls_md5_context md5_ctx;
        mbedtls_md5_init(&md5_ctx);
        mbedtls_md5_starts(&md5_ctx);

        uint8_t  chunk[4096];
        uint32_t offset = 0, recv_len = 0;
        int      ok_flag = 1;

        while (offset < size)
        {
            if (AT_HttpGetChunk(url, offset,
                                chunk, &recv_len) != HAL_OK)
            {
                printf("[OTA] Download error at %lu\r\n", offset);
                ok_flag = 0; break;
            }
            /* 写入备用分区，32 字节对齐 */
            uint32_t write_size = (recv_len + 31) & ~31U;
            HAL_FLASH_Unlock();
            for (uint32_t i = 0; i < write_size; i += 32) 
            {
                uint8_t wbuf[32] = {0xFF};
                memcpy(wbuf, chunk + i,
                       (i + 32 <= recv_len) ? 32 : recv_len - i);
                HAL_FLASH_Program(FLASH_TYPEPROGRAM_FLASHWORD,
                    OTA_BACKUP_ADDR + offset + i,
                    (uint32_t)wbuf);
            }
            HAL_FLASH_Lock();

            mbedtls_md5_update(&md5_ctx, chunk, recv_len);
            offset += recv_len;

            uint8_t prog = (uint8_t)(offset * 100 / size);
            OTA_ReportProgress(prog);
            printf("[OTA] %u%%\r\n", prog);
        }

        if (!ok_flag) 
        {
            OTA_ReportProgress(-1);
            continue;
        }

        /* ── MD5 校验 ── */
        uint8_t md5_bin[16]; char md5_str[33];
        mbedtls_md5_finish(&md5_ctx, md5_bin);
        mbedtls_md5_free(&md5_ctx);

        for (int i = 0; i < 16; i++)
            sprintf(md5_str + i*2, "%02x", md5_bin[i]);

        if (strncasecmp(md5_str, md5_exp, 32) != 0) 
        {
            printf("[OTA] MD5 FAIL! got=%s exp=%s\r\n",
                   md5_str, md5_exp);
            OTA_ReportProgress(-2);
            continue;
        }
        printf("[OTA] MD5 OK! Rebooting...\r\n");

        /* ── 写升级标志，重启 ── */
        OTA_Flag_t flag = {
            .magic         = OTA_MAGIC_WORD,
            .upgrade_flag  = 1,
            .firmware_size = size,
            .retry_count   = 0,
            .boot_ok       = 0
        };
        strncpy(flag.version, version, 31);
        OTA_WriteFlag(&flag);

        OTA_ReportProgress(100);
        vTaskDelay(pdMS_TO_TICKS(500));
        HAL_NVIC_SystemReset();
    }
}



