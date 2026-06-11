#include "cJSON.h"
#include "ota_flag.h"
#include "at_driver.h"
#include "mqtt_task.h"
#include "cmsis_os.h"

#include "md5.h"

void OTA_ReportProgress(int step)
{
    char payload[256], escaped_payload[400], cmd[512];
    const char *desc;

    /* step 含义遵循阿里云 OTA 协议:
       -1:升级失败, -2:下载失败, -3:校验失败, -4:烧写失败, 0-100:进度百分比 */
    switch (step) {
        case 100: desc = "upgrade success"; break;
        case -1:  desc = "upgrade failed";  break;
        case -2:  desc = "download failed"; break;
        case -3:  desc = "verify failed";   break;
        case -4:  desc = "flash failed";    break;
        default:  desc = "downloading";     break;
    }
    snprintf(payload, sizeof(payload),
            "{\"id\":\"%lu\",\"params\":{\"step\":\"%d\",\"desc\":\"%s\",\"module\":\"MCU\"}}",
            (uint32_t)osKernelGetTickCount(), step, desc);
    EscapeEspAtMqttField(payload, escaped_payload, sizeof(escaped_payload));
    snprintf(cmd, sizeof(cmd),
            "AT+MQTTPUB=0,\"%s\",\"%s\",1,0",
            OTA_TOPIC_PROGRESS, escaped_payload);
    AT_Send(cmd, "OK", 3000);
}

void OTA_Task(void *arg)
{
    char json_buf[512];
    for(;;)
    {
        if(osMessageQueueGet(ota_notify_queue, json_buf, 0, portMAX_DELAY) != osOK)
        {
            continue;
        } 

        cJSON *root = cJSON_Parse(json_buf);
        if(!root) continue;

        /* 字段 NULL 检查 */
        cJSON *data = cJSON_GetObjectItem(root, "data");
        if(!data) { cJSON_Delete(root); continue; }

        cJSON *url_item  = cJSON_GetObjectItem(data, "url");
        cJSON *md5_item  = cJSON_GetObjectItem(data, "md5");
        cJSON *size_item = cJSON_GetObjectItem(data, "size");
        cJSON *ver_item  = cJSON_GetObjectItem(data, "version");
        if(!url_item || !md5_item || !size_item || !ver_item) {
            printf("[OTA] Missing required fields\r\n");
            cJSON_Delete(root);
            continue;
        }

        /* 先拷贝字符串再释放 cJSON 树, 避免野指针 */
        char url[256], md5_exp[33], version[32];
        strncpy(url,     url_item->valuestring, sizeof(url) - 1);
        url[sizeof(url) - 1] = '\0';
        strncpy(md5_exp, md5_item->valuestring, sizeof(md5_exp) - 1);
        md5_exp[sizeof(md5_exp) - 1] = '\0';
        strncpy(version, ver_item->valuestring, sizeof(version) - 1);
        version[sizeof(version) - 1] = '\0';
        uint32_t size = (uint32_t)size_item->valueint;

        cJSON_Delete(root);  /* 安全释放, url/md5_exp 已是独立副本 */

        printf("[OTA] New firmware: %s size=%lu\r\n", version, size);
        OTA_ReportProgress(0);   

        FLASH_EraseInitTypeDef erase = {
            .TypeErase  = FLASH_TYPEERASE_MASSERASE,
            .Banks      = FLASH_BANK_2
        };
        uint32_t err;
        HAL_FLASH_Unlock();
        HAL_FLASHEx_Erase(&erase, &err);
        HAL_FLASH_Lock();

        /* Range 分块: 1024 字节每块, 跟最后一包(正确)的尺寸相近 */
        osDelay(2000);

        mbedtls_md5_context md5_ctx;
        mbedtls_md5_init(&md5_ctx);
        mbedtls_md5_starts(&md5_ctx);

        #define CHUNK_SZ 1024
        uint8_t  chunk[CHUNK_SZ];
        uint32_t offset = 0, recv_len, md5_bytes = 0;
        int      ok_flag = 1, retry = 0;

        while (offset < size)
        {
            uint32_t remaining = size - offset;
            recv_len = (CHUNK_SZ < remaining) ? CHUNK_SZ : remaining;
            HAL_StatusTypeDef r = AT_HttpGetChunk(url, offset, chunk, &recv_len);
            if (r != HAL_OK || recv_len == 0) {
                if(retry < 5) { retry++; osDelay(2000); continue; }
                printf("[OTA] Err at %lu\r\n", offset); ok_flag = 0; break;
            }
            retry = 0;
            osDelay(3000); /* OSS auth_key 限速: 70+ 请求后会拒绝, 3s 间隔避免 */

            uint32_t ws = (recv_len + 31) & ~31U;
            HAL_FLASH_Unlock();
            for (uint32_t i = 0; i < ws; i += 32) {
                uint8_t wbuf[32] = {0xFF};
                memcpy(wbuf, chunk + i, (i+32 <= recv_len) ? 32 : recv_len - i);
                HAL_FLASH_Program(FLASH_TYPEPROGRAM_FLASHWORD, OTA_BACKUP_ADDR + offset + i, (uint32_t)wbuf);
            }
            HAL_FLASH_Lock();

            mbedtls_md5_update(&md5_ctx, chunk, recv_len);
            md5_bytes += recv_len;
            offset += recv_len;
            printf("[OTA] %u%% (%lu b)\r\n", (uint8_t)(offset*100/size), recv_len);
        }

        if (!ok_flag)
        {
            OTA_ReportProgress(-2);   /* -2: 下载失败 */
            continue;
        }

        uint8_t md5_bin[16]; char md5_str[33];
        mbedtls_md5_finish(&md5_ctx, md5_bin);
        mbedtls_md5_free(&md5_ctx);

        for (int i = 0; i < 16; i++)
            sprintf(md5_str + i*2, "%02x", md5_bin[i]);

        if (strncasecmp(md5_str, md5_exp, 32) != 0) 
        {
            printf("[OTA] MD5 FAIL! got=%s exp=%s (hashed %lu bytes)\r\n",
                   md5_str, md5_exp, md5_bytes);
            OTA_ReportProgress(-3);   /* -3: 校验失败 */
            continue;
        }
        printf("[OTA] MD5 OK! (hashed %lu bytes) Rebooting...\r\n", md5_bytes);

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



