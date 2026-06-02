#ifndef OTA_FLAG_H
#define OTA_FLAG_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

#define OTA_FLAG_ADDR       0x08020000UL        /* 标志区地址 */
#define OTA_MAGIC_WORD      0xDEADBEEFUL        /* 有效性魔数 */
#define APP_ADDR            0x08030000UL        /* APP 起始地址 */
#define OTA_BACKUP_ADDR     0x08100000UL        /* 备用固件地址 */

typedef struct __attribute__((aligned(32))) {
    uint32_t magic;             /* 必须等于 OTA_MAGIC_WORD 才有效 */
    uint32_t upgrade_flag;      /* 1 = 有待升级固件 */
    uint32_t firmware_size;      /* 新固件字节数 */
    uint32_t firware_crc;       /* 新固件 CRC32 （备用，主要用 MD5) */
    char     version[32];       /* 版本字符串，如 "2.0.0" */
    uint32_t retry_count;        /* 升级尝试次数，超过3次放弃 */
    uint32_t boot_ok;           /* APP 启动成功后写 1 */
    uint32_t reserved[10];       /* 保留字段，凑足 32 字节对齐 */
} OTA_Flag_t;

void OTA_WriteFlag(const OTA_Flag_t *flag);
void OTA_ReadFlag(OTA_Flag_t *flag);
void OTA_ClearFlag(void);

#ifdef __cplusplus
}
#endif

#endif // OTA_FLAG_H