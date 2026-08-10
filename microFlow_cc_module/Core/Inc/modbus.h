/**
 * @file modbus.h
 * @brief Modbus RTU 应用层接口
 *
 * 结构与 TEC 工程保持一致，适配 microFlow 双串口：
 * - USART1: RS485 (硬件 DE, PA12)
 * - USART2: CH340 USB-to-USART (PA2/PA3)
 */

#ifndef _MODBUS_H
#define _MODBUS_H

#include "main.h"
#include "regmap.h"
#include "mb.h"

/* ----------------------- 外部寄存器实例声明 ------------------------------*/
extern SYS_STATUS_T       system_status;
extern COMM_SETTINGS_T    comm_settings;
extern MEASURE_SETTINGS_T measure_settings;
extern CALIB_SETTINGS_T   calib_settings;
extern FILTER_SETTINGS_T  filter_settings;
extern MEASURE_VALUES_T   measure_values;
extern RSVD_PARAM_T       rsvd_param;

/* ----------------------- 诊断计数器 --------------------------------------*/
typedef struct {
    uint32_t usart1_rx_frames;
    uint32_t usart2_rx_frames;
    uint32_t usart1_tx_frames;
    uint32_t usart2_tx_frames;
    uint32_t crc_errors;
    uint32_t illegal_addr_errors;
    uint32_t illegal_func_errors;
    uint32_t usart1_ore_count;
    uint32_t usart1_fe_count;
    uint32_t usart1_ne_count;
    uint32_t usart1_pe_count;
    uint32_t usart2_ore_count;
    uint32_t usart2_fe_count;
    uint32_t usart2_ne_count;
    uint32_t usart2_pe_count;
    uint32_t conflict_discards;   /* 双串口冲突丢弃计数 */
    uint32_t timeout_recoveries;  /* 超时恢复计数 */
} MB_DiagCounters_t;

extern MB_DiagCounters_t mb_diag;

/* Flash 自动保存标志（portrw.c 写入时置位） */
extern volatile uint8_t autoSavePending;

/* ----------------------- 函数声明 ----------------------------------------*/
void SystemParam_init(void);
void Modbus_Init(void);
void Modbus_USART_Init(ULONG ulBaudRate, UCHAR ucDataBits, eMBParity eParity);
void Modbus_TIM7_Init(USHORT usTim1Timerout50us);

/* UART IRQ 用户处理入口 */
void USART1_IRQHandler_USER(void);
void USART2_IRQHandler_USER(void);

/* Poll 后事务释放 */
void MB_PortAfterPoll(void);

/* TIM7 T3.5 到期时更新双串口事务状态 */
void MB_PortOnT35Expired(void);

#endif /* _MODBUS_H */
