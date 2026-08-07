#include "flash.h"
#include "main.h"
#include "modbus.h"
#include "parameter.h"
#include "portrw.h"
#include "version.h"
#include "stdio.h"
#include "string.h"

SYS_STATUS_T system_statusf;
COMM_SETTINGS_T comm_settingsf;
MEASURE_SETTINGS_T measure_settingsf;
CALIB_SETTINGS_T calib_settingsf;
FILTER_SETTINGS_T filter_settingsf;
MEASURE_VALUES_T measure_valuesf;
RSVD_PARAM_T rsvd_paramf;

typedef struct {
  void *pData;    // 指向modbus相关寄存器的数据指针
  uint32_t addr;  // 寄存器组Flash存储地址
  uint16_t nbyte; // 寄存器组字节数
} REG_STORE_TypeDef;

REG_STORE_TypeDef RegsStoreInfArr
    [REG_GROUP_NUM] = // 存入待存储寄存器的首地址,Flash地址和寄存器个数
    {{&system_status, SYS_STATUS_SADDR, SYSREG_NREGS},
     {&comm_settings, COM_SET_SADDR, COMSREG_NREGS},
     {&measure_settings, MEASURE_SET_SADDR, MSREG_NREGS},
     {&calib_settings, CAL_SET_SADDR, CALSREG_NREGS},
     {&filter_settings, FILTER_SET_SADDR, FSREG_NREGS},
     {&rsvd_param, RSVD_PARA_SADDR, RSVD_NREGS}};

REG_STORE_TypeDef RegsStoreInfArrf
    [REG_GROUP_NUM] = // 存入待存储寄存器的首地址,Flash地址和寄存器个数
    {{&system_statusf, SYS_STATUS_SADDR_F, SYSREG_NREGS},
     {&comm_settingsf, COM_SET_SADDR_F, COMSREG_NREGS},
     {&measure_settingsf, MEASURE_SET_SADDR_F, MSREG_NREGS},
     {&calib_settingsf, CAL_SET_SADDR_F, CALSREG_NREGS},
     {&filter_settingsf, FILTER_SET_SADDR_F, FSREG_NREGS},
     {&rsvd_paramf, RSVD_PARA_SADDR_F, RSVD_NREGS}};

void CheckParam();

void LoadRegsFromFactory() {
  memcpy((void *)&measure_settings, (void *)&measure_settingsf,
         REG_GROUP_BYTES);
  memcpy((void *)&calib_settings, (void *)&calib_settingsf, REG_GROUP_BYTES);
  memcpy((void *)&filter_settings, (void *)&filter_settingsf, REG_GROUP_BYTES);
  memcpy((void *)&rsvd_param, (void *)&rsvd_paramf, REG_GROUP_BYTES);
  CheckParam();
  __disable_irq();
  StoreModbusRegs();
  __enable_irq();
}

void SaveRegsToFactory() {
  memcpy((void *)&system_statusf, (void *)&system_status, REG_GROUP_BYTES);
  memcpy((void *)&comm_settingsf, (void *)&comm_settings, REG_GROUP_BYTES);
  memcpy((void *)&measure_settingsf, (void *)&measure_settings,
         REG_GROUP_BYTES);
  memcpy((void *)&calib_settingsf, (void *)&calib_settings, REG_GROUP_BYTES);
  memcpy((void *)&filter_settingsf, (void *)&filter_settings, REG_GROUP_BYTES);
  memcpy((void *)&rsvd_paramf, (void *)&rsvd_param, REG_GROUP_BYTES);
  __disable_irq();
  StoreModbusRegs();
  __enable_irq();
}

/**
 * @brief  Flash擦除
 * @param  地址
 * @retval 执行状态
 */
HAL_StatusTypeDef STMFLASH_Erase(uint32_t e_addr) {
  uint32_t PageError = 0;
  FLASH_EraseInitTypeDef pEraseInit;
  HAL_StatusTypeDef status = HAL_OK;

  HAL_FLASH_Unlock();

  pEraseInit.Banks = FLASH_BANK_1;                  // 擦除Bank1
  pEraseInit.NbPages = 1;                           // 擦除扇区的个数
  pEraseInit.Page = REG_STORAGE_OFFSET / PAGE_SIZE; // 擦除Page70
  pEraseInit.TypeErase = FLASH_TYPEERASE_PAGES;     // 擦除类型Page
  status = HAL_FLASHEx_Erase(&pEraseInit, &PageError);

  HAL_FLASH_Lock();

  return status;
}

/**
 * @brief  保存数据到stm32内部flash
 * @param  pdata:数据指针
 *         nbyte:写入字节数
 * @retval 执行状态
 */
static HAL_StatusTypeDef STMFLASH_Write(void *pdata, uint32_t addr,
                                        uint8_t nbyte) {
  HAL_StatusTypeDef status = HAL_OK;
  uint64_t *pd = pdata;
  HAL_FLASH_Unlock();
  uint8_t i = 0;
  // 写入数据
  for (; i < (nbyte / 4) && status == HAL_OK; i++) {
    status = HAL_FLASH_Program(FLASH_TYPEPROGRAM_DOUBLEWORD, addr, *pd);
    addr += 8;
    pd++;
  }

  HAL_FLASH_Lock();
  return status;
}

/**
 * @brief  存储modbus寄存器
 * @param  无
 * @retval 执行状态
 */
HAL_StatusTypeDef StoreModbusRegs(void) {
  HAL_StatusTypeDef s_status;

  // 擦除flash
  s_status = STMFLASH_Erase(REG_STORAGE_ADDR);
  if (s_status != HAL_OK)
    return s_status;
  uint8_t i = 0;
  // 保存寄存器到flash
  for (; i < REG_GROUP_NUM; i++) {
    s_status = STMFLASH_Write(RegsStoreInfArr[i].pData, RegsStoreInfArr[i].addr,
                              RegsStoreInfArr[i].nbyte);
    if (s_status != HAL_OK)
      return s_status;
  }
  for (i = 0; i < REG_GROUP_NUM; i++) {
    s_status =
        STMFLASH_Write(RegsStoreInfArrf[i].pData, RegsStoreInfArrf[i].addr,
                       RegsStoreInfArrf[i].nbyte);
    if (s_status != HAL_OK)
      return s_status;
  }
  return s_status;
}

#define HW_VERSION_NUM "v1"

void ResetFlash(void) {
  /*system_status结构体初始化*/
  memset((void *)&system_status, 0, sizeof(system_status));
  strcpy(system_status.softwareVer, SW_VERSION_NUM);

  memset((void *)&comm_settings, 0, sizeof(comm_settings));
  memset((void *)&measure_settings, 0, sizeof(measure_settings));

  ParametarsInitByDefaultValue();

  memcpy((void *)&system_statusf, (void *)&system_status, REG_GROUP_BYTES);
  memcpy((void *)&comm_settingsf, (void *)&comm_settings, REG_GROUP_BYTES);
  memcpy((void *)&measure_settingsf, (void *)&measure_settings,
         REG_GROUP_BYTES);
  memcpy((void *)&calib_settingsf, (void *)&calib_settings, REG_GROUP_BYTES);
  memcpy((void *)&filter_settingsf, (void *)&filter_settings, REG_GROUP_BYTES);
  memcpy((void *)&rsvd_paramf, (void *)&rsvd_param, REG_GROUP_BYTES);

  __disable_irq();
  StoreModbusRegs();
  __enable_irq();
}

uint8_t updateFlg = 0;

void CheckParam() {
  updateFlg = CheckParamyDefaultSetting();
  if (updateFlg > 0) {
    __disable_irq();
    StoreModbusRegs();
    __enable_irq();
  }
}

void PowerOn_ReadModbusReg(void) {
  // 检查寄存器长度
  if (sizeof(SYS_STATUS_T) != REG_GROUP_BYTES ||
      sizeof(COMM_SETTINGS_T) != REG_GROUP_BYTES ||
      sizeof(MEASURE_SETTINGS_T) != REG_GROUP_BYTES ||
      sizeof(CALIB_SETTINGS_T) != REG_GROUP_BYTES ||
      sizeof(FILTER_SETTINGS_T) != REG_GROUP_BYTES ||
      sizeof(RSVD_PARAM_T) != REG_GROUP_BYTES) {
    while (1) {
    }
  }

  memcpy((void *)&system_status, (void *)(SYS_STATUS_SADDR), REG_GROUP_BYTES);
  memcpy((void *)&comm_settings, (void *)(COM_SET_SADDR), REG_GROUP_BYTES);
  memcpy((void *)&measure_settings, (void *)(MEASURE_SET_SADDR),
         REG_GROUP_BYTES);
  memcpy((void *)&calib_settings, (void *)(CAL_SET_SADDR), REG_GROUP_BYTES);
  memcpy((void *)&filter_settings, (void *)(FILTER_SET_SADDR), REG_GROUP_BYTES);
  memcpy((void *)&rsvd_param, (void *)(RSVD_PARA_SADDR), REG_GROUP_BYTES);

  memcpy((void *)&system_statusf, (void *)(SYS_STATUS_SADDR_F),
         REG_GROUP_BYTES);
  memcpy((void *)&comm_settingsf, (void *)(COM_SET_SADDR_F), REG_GROUP_BYTES);
  memcpy((void *)&measure_settingsf, (void *)(MEASURE_SET_SADDR_F),
         REG_GROUP_BYTES);
  memcpy((void *)&calib_settingsf, (void *)(CAL_SET_SADDR_F), REG_GROUP_BYTES);
  memcpy((void *)&filter_settingsf, (void *)(FILTER_SET_SADDR_F),
         REG_GROUP_BYTES);
  memcpy((void *)&rsvd_paramf, (void *)(RSVD_PARA_SADDR_F), REG_GROUP_BYTES);

  if (system_status.newStructFlg != 0x2222) // 从未下载过程序的全新探头
  {
    ResetFlash();
  } else {
    CheckParam();
    if (strcmp(system_status.softwareVer, SW_VERSION_NUM) != 0) {
      memset(system_status.softwareVer, 0, sizeof(system_status.softwareVer));
      strcpy(system_status.softwareVer, SW_VERSION_NUM);
      __disable_irq();
      StoreModbusRegs();
      __enable_irq();
    }
  }
  strcpy(system_status.softwareVer, SW_VERSION_NUM);
  strcpy(system_status.hardwareVer, HW_VERSION_NUM);
}
