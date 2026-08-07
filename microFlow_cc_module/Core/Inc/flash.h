#ifndef _FLASH_H
#define _FLASH_H
#include "main.h"

/* Flash 页大小 2KB */
#define PAGE_SIZE                FLASH_PAGE_SIZE
/* 待存储的寄存器组数目 */
#define REG_GROUP_NUM            6
/* 每个寄存器组字节数 = 52个uint16_t */
#define REG_GROUP_BYTES          104
/* Modbus 寄存器保存偏移地址 (Page 70) */
#define REG_STORAGE_OFFSET       ((70) * 2048)

/* Modbus 寄存器组存储基地址 */
#define REG_STORAGE_ADDR         (0x08000000 + REG_STORAGE_OFFSET)

/* 用户参数区 (组 0~5) */
#define SYS_STATUS_SADDR         (REG_STORAGE_ADDR + 0*REG_GROUP_BYTES)
#define COM_SET_SADDR            (REG_STORAGE_ADDR + 1*REG_GROUP_BYTES)
#define MEASURE_SET_SADDR        (REG_STORAGE_ADDR + 2*REG_GROUP_BYTES)
#define CAL_SET_SADDR            (REG_STORAGE_ADDR + 3*REG_GROUP_BYTES)
#define FILTER_SET_SADDR         (REG_STORAGE_ADDR + 4*REG_GROUP_BYTES)
#define RSVD_PARA_SADDR          (REG_STORAGE_ADDR + 5*REG_GROUP_BYTES)

/* 出厂默认值区 (组 6~11) */
#define SYS_STATUS_SADDR_F       (REG_STORAGE_ADDR + 6*REG_GROUP_BYTES)
#define COM_SET_SADDR_F          (REG_STORAGE_ADDR + 7*REG_GROUP_BYTES)
#define MEASURE_SET_SADDR_F      (REG_STORAGE_ADDR + 8*REG_GROUP_BYTES)
#define CAL_SET_SADDR_F          (REG_STORAGE_ADDR + 9*REG_GROUP_BYTES)
#define FILTER_SET_SADDR_F       (REG_STORAGE_ADDR + 10*REG_GROUP_BYTES)
#define RSVD_PARA_SADDR_F        (REG_STORAGE_ADDR + 11*REG_GROUP_BYTES)

HAL_StatusTypeDef StoreModbusRegs(void);
void ResetFlash(void);
void PowerOn_ReadModbusReg(void);
void LoadRegsFromFactory(void);
void SaveRegsToFactory(void);

#endif /* _FLASH_H */
