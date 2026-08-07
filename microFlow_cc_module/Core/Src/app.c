/**
 * @file app.c
 * @brief 应用层命令处理 - 从 TEC 工程移植（精简版）
 *
 * 负责：
 * - autoSavePending 检测与自动保存触发
 * - Modbus 命令码分发（保存/恢复出厂/跳转 Bootloader 等）
 *
 * 在 StartModbusTask 的 eMBPoll() 循环中调用 MeasureFunc()
 */

#include "app.h"
#include "flash.h"
#include "main.h"
#include "modbus.h"
#include <string.h>

/* ======================== 手动保存接口 ========================*/
void updateParameters(void)
{
    __disable_irq();
    StoreModbusRegs();
    __enable_irq();
}

void saveParametersToFlash(void)
{
    HAL_StatusTypeDef ret;
    __disable_irq();
    ret = StoreModbusRegs();
    __enable_irq();

    /* 保存结果记录到 system_status，可通过 Modbus 读取查看 */
    if (ret == HAL_OK)
        system_status.calibStatus = 0x201;  /* SAVE_SUCCESS */
    else
        system_status.calibStatus = 0x200;  /* SAVE_FAILED */
}

/* ======================== 主循环调用函数 ========================*/
/**
 * @brief Modbus 任务主循环中的周期处理函数
 *
 * 功能：
 * 1. 检测 autoSavePending 标志，触发自动保存
 * 2. 处理 measure_settings.command 命令码
 *
 * 在 StartModbusTask 的 for(;;) 循环中每次 eMBPoll() 后调用
 */
void MeasureFunc(void)
{
    /* ---- 自动保存检测 ----
     * portrw.c 中每次写保持寄存器会设置 autoSavePending=1
     * 这里检测到后清零并设置保存命令
     */
    if (autoSavePending == 1)
    {
        autoSavePending = 0;
        measure_settings.command = SaveToFlahCMD;
    }

    /* ---- 命令分发处理 ---- */
    if (measure_settings.command == SaveToFlahCMD)
    {
        /* 保存到 Flash */
        measure_settings.command = 0;
        saveParametersToFlash();
    }
    else if (measure_settings.command == JumpBootloaderCMD)
    {
        /* 跳转 Bootloader 并复位 */
        measure_settings.command = 0;
        HAL_Delay(200);
        __disable_irq();
        NVIC_SystemReset();
    }
    else if (measure_settings.command == RestoreFlashCMD)
    {
        /* 恢复出厂设置 */
        measure_settings.command = 0;
        ResetFlash();
    }
    else if (measure_settings.command == LoadParFromFactory)
    {
        /* 从出厂默认值区加载 */
        measure_settings.command = 0;
        LoadRegsFromFactory();
        system_status.calibStatus = 0x205;  /* RESTORE_FACTORY_DATA_SUCCESS */
    }
    else if (measure_settings.command == SaveParToFactory)
    {
        /* 保存当前值为新的出厂默认值 */
        measure_settings.command = 0;
        SaveRegsToFactory();
        system_status.calibStatus = 0x204;  /* SAVE_FACTORY_DATA_SUCCESS */
    }
}
