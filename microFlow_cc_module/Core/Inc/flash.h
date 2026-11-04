#ifndef _FLASH_H
#define _FLASH_H
#include "main.h"

/* Flash 页大小 2KB */
#define PAGE_SIZE                FLASH_PAGE_SIZE
/* 待存储的寄存器组数目 */
#define REG_GROUP_NUM            6
/* 基础参数组字节数 = 52个uint16_t；RSVD 单独扩展为 208 字节 */
#define REG_GROUP_BYTES          104
/* Modbus 寄存器保存偏移地址 (Page 63, 128KB Flash 的最后一页) */
#define REG_STORAGE_OFFSET       ((63) * 2048)

/* Modbus 寄存器组存储基地址 */
#define REG_STORAGE_ADDR         (0x08000000 + REG_STORAGE_OFFSET)

/* 原用户参数槽；组 5 地址仅用于旧格式迁移 */
#define SYS_STATUS_SADDR         (REG_STORAGE_ADDR + 0*REG_GROUP_BYTES)
#define COM_SET_SADDR            (REG_STORAGE_ADDR + 1*REG_GROUP_BYTES)
#define MEASURE_SET_SADDR        (REG_STORAGE_ADDR + 2*REG_GROUP_BYTES)
#define CAL_SET_SADDR            (REG_STORAGE_ADDR + 3*REG_GROUP_BYTES)
#define FILTER_SET_SADDR         (REG_STORAGE_ADDR + 4*REG_GROUP_BYTES)
#define RSVD_PARA_LEGACY_SADDR   (REG_STORAGE_ADDR + 5*REG_GROUP_BYTES)

/* 原出厂默认值槽；组 11 地址仅用于旧格式迁移 */
#define SYS_STATUS_SADDR_F       (REG_STORAGE_ADDR + 6*REG_GROUP_BYTES)
#define COM_SET_SADDR_F          (REG_STORAGE_ADDR + 7*REG_GROUP_BYTES)
#define MEASURE_SET_SADDR_F      (REG_STORAGE_ADDR + 8*REG_GROUP_BYTES)
#define CAL_SET_SADDR_F          (REG_STORAGE_ADDR + 9*REG_GROUP_BYTES)
#define FILTER_SET_SADDR_F       (REG_STORAGE_ADDR + 10*REG_GROUP_BYTES)
#define RSVD_PARA_LEGACY_SADDR_F (REG_STORAGE_ADDR + 11*REG_GROUP_BYTES)

/* RSVD alone grows to 104 registers / 208 bytes. Keep all other slots intact.
 * Extended user + factory payloads and tag fit within the same 2 KB page.
 */
#define RSVD_STORAGE_BYTES       208U
#define RSVD_PARA_SADDR          (REG_STORAGE_ADDR + 12*REG_GROUP_BYTES)
#define RSVD_PARA_SADDR_F        (RSVD_PARA_SADDR + RSVD_STORAGE_BYTES)
#define RSVD_LAYOUT_TAG_ADDR     (RSVD_PARA_SADDR_F + RSVD_STORAGE_BYTES)
#define RSVD_LAYOUT_TAG          UINT64_C(0x5253564431303401)

HAL_StatusTypeDef StoreModbusRegs(void);
void ResetFlash(void);
void PowerOn_ReadModbusReg(void);
void LoadRegsFromFactory(void);
void SaveRegsToFactory(void);

#endif /* _FLASH_H */
