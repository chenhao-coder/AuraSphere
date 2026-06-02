#include "ota_flag.h"
#include "stm32h7xx_hal.h"
#include <string.h>

/* 读取标志 （直接内存映射读取) */
void OTA_ReadFlag(OTA_Flag_t *flag)
{
    memcpy(flag, (void*)OTA_FLAG_ADDR, sizeof(OTA_Flag_t));
}

void OTA_WriteFlag(const OTA_Flag_t *flag)
{
    // sizeof 已是 32 的整数倍，不再需要 + 32 的冗余空间
    // 也不需要向上取整计算
    static uint8_t alignedBuf[sizeof(OTA_Flag_t)] __attribute__((aligned(32)));
    const uint32_t FLASH_WORD_SIZE = 32U;
    
    // 不再需要 memset 补 0xFF, sizeof 已对齐，无多余字节
    memcpy(alignedBuf, flag, sizeof(OTA_Flag_t));

    FLASH_EraseInitTypeDef erase = {
        .TypeErase          = FLASH_TYPEERASE_SECTORS,
        .Banks              = FLASH_BANK_1,
        .Sector             = FLASH_SECTOR_1,
        .NbSectors          = 1,
        .VoltageRange       = FLASH_VOLTAGE_RANGE_3,
    };
    uint32_t err = 0;

    HAL_FLASH_Unlock();
    HAL_FLASHEx_Erase(&erase, &err);

    /* 按 32 字节循环写入 */
    for (uint32_t offset = 0; offset < sizeof(OTA_Flag_t); offset += FLASH_WORD_SIZE)
    {
        HAL_StatusTypeDef status = HAL_FLASH_Program(
            FLASH_TYPEPROGRAM_FLASHWORD,
            OTA_FLAG_ADDR + offset,
            (uint32_t)(alignedBuf + offset)
        );
        if (status != HAL_OK) break;
    }

    HAL_FLASH_Lock();
}

/* 清除标志 将 upgrade_flag 置 0 */
void OTA_ClearFlag(void)
{
    OTA_Flag_t flag;
    OTA_ReadFlag(&flag);
    flag.upgrade_flag = 0;
    flag.retry_count = 0;
    OTA_WriteFlag(&flag);
}


