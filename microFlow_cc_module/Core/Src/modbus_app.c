/**
 * @file modbus_app.c
 * @brief Modbus RTU 应用层实现
 *
 * 结构与 TEC 工程保持一致，适配 microFlow 双串口。
 * 集成 Flash 参数存储和命令分发。
 */

#include "modbus.h"
#include "mb.h"
#include "portevent.h"
#include "app.h"
#include "flash.h"
#include "cmsis_os.h"
#include <string.h>

/* ----------------------- 外部变量（portserial.c 定义） ---------------------*/
extern volatile BOOL g_mb_rx_busy;

/* ----------------------- 寄存器实例定义 ----------------------------------*/
SYS_STATUS_T       system_status;
COMM_SETTINGS_T    comm_settings;
MEASURE_SETTINGS_T measure_settings;
CALIB_SETTINGS_T   calib_settings;
FILTER_SETTINGS_T  filter_settings;
MEASURE_VALUES_T   measure_values;
RSVD_PARAM_T       rsvd_param;

/* ----------------------- 诊断计数器 --------------------------------------*/
MB_DiagCounters_t mb_diag = {0};

/* ----------------------- Flash 调试变量 ----------------------------------*/
volatile uint32_t dbg_flash_err_addr = 0;   /* Flash 写入失败的地址 */
volatile uint32_t dbg_flash_err_sr = 0;    /* Flash 状态寄存器值 */

/* Flash 自动保存标志 */
volatile uint8_t autoSavePending = 0;

/* ----------------------- 默认参数 ----------------------------------------*/
#define MODBUS_DEFAULT_ADDR        1
#define MODBUS_DEFAULT_BAUD        115200UL
#define MODBUS_DEFAULT_DATABITS     8
#define MODBUS_DEFAULT_PARITY      MB_PAR_NONE

/* ----------------------- 私有函数声明 ------------------------------------*/
static void Modbus_LoadDefaultParams(void);

/* ----------------------- USART 初始化 -------------------------------------*/
void Modbus_USART_Init(ULONG ulBaudRate, UCHAR ucDataBits, eMBParity eParity)
{
    UART_HandleTypeDef *huart;

    /* 配置 USART1 (RS485 硬件 DE) */
    huart = &huart1;
    HAL_UART_DeInit(huart);
    huart->Init.BaudRate = ulBaudRate;

    switch(eParity)
    {
        case MB_PAR_NONE:
            huart->Init.WordLength = UART_WORDLENGTH_8B;
            huart->Init.Parity     = UART_PARITY_NONE;
            break;
        case MB_PAR_ODD:
            huart->Init.WordLength = UART_WORDLENGTH_9B;
            huart->Init.Parity     = UART_PARITY_ODD;
            break;
        case MB_PAR_EVEN:
            huart->Init.WordLength = UART_WORDLENGTH_9B;
            huart->Init.Parity     = UART_PARITY_EVEN;
            break;
        default:
            huart->Init.WordLength = UART_WORDLENGTH_8B;
            huart->Init.Parity     = UART_PARITY_NONE;
            break;
    }

    /* USART1 使用 RS485 硬件 DE 模式重新初始化 */
    HAL_RS485Ex_Init(huart, UART_DE_POLARITY_HIGH, 0, 0);

    /* 配置 USART2 (CH340) - 与 USART1 相同参数 */
    huart = &huart2;
    HAL_UART_DeInit(huart);
    huart->Init.BaudRate     = ulBaudRate;
    huart->Init.WordLength   = huart1.Init.WordLength;
    huart->Init.Parity       = huart1.Init.Parity;
    huart->Init.StopBits     = UART_STOPBITS_1;
    huart->Init.Mode         = UART_MODE_TX_RX;
    huart->Init.HwFlowCtl    = UART_HWCONTROL_NONE;
    huart->Init.OverSampling = UART_OVERSAMPLING_16;
    huart->Init.OneBitSampling = UART_ONE_BIT_SAMPLE_DISABLE;
    huart->AdvancedInit.AdvFeatureInit = UART_ADVFEATURE_NO_INIT;
    HAL_UART_Init(huart);
}

/* ----------------------- TIM7 初始化 --------------------------------------*/
void Modbus_TIM7_Init(USHORT usTim1Timerout50us)
{
    /*
     * TIM7 已由 CubeMX 完成基础初始化（时钟、NVIC、Handle）。
     * 这里只修改 ARR，不重新执行完整的 HAL_TIM_Base_Init()。
     */
    __HAL_TIM_DISABLE(&htim7);
    __HAL_TIM_SET_AUTORELOAD(&htim7, (uint32_t)(usTim1Timerout50us - 1));
    __HAL_TIM_SET_COUNTER(&htim7, 0);
    __HAL_TIM_CLEAR_IT(&htim7, TIM_IT_UPDATE);
}

/* ----------------------- Modbus 初始化与使能 ------------------------------*/
void Modbus_Init(void)
{
    eMBErrorCode eStatus;

    /*
     * 步骤 1：从 Flash 加载参数（或首次启动恢复出厂默认值）
     * 这会填充 system_status/comm_settings 等结构体
     */
    PowerOn_ReadModbusReg();

    /*
     * 步骤 2：初始化事件层
     */
    xMBPortEventInit();

    /*
     * 步骤 3：初始化协议栈
     * 使用从 Flash 加载的通信参数（地址、波特率、校验）
     */
    eStatus = eMBInit(MB_RTU,
                      (UCHAR)comm_settings.modbusAddr,
                      0,
                      (ULONG)comm_settings.modbusBaud,
                      (eMBParity)comm_settings.modbusParity);

    if(eStatus != MB_ENOERR)
    {
        while(1);  /* 调试用断点：初始化失败 */
    }

    /* 使能协议栈 - 内部会调用 vMBPortSerialEnable(TRUE, FALSE) 开启 RXNE 中断 */
    eStatus = eMBEnable();
    if(eStatus != MB_ENOERR)
    {
        while(1);  /* 调试用断点：使能失败 */
    }

    /*
     * 安全保障：显式确保 NVIC 和 RXNE 中断已开启
     */
    HAL_NVIC_EnableIRQ(USART1_IRQn);
    HAL_NVIC_EnableIRQ(USART2_IRQn);
}

/* ----------------------- Poll 后事务释放 ----------------------------------*/
void MB_PortAfterPoll(void)
{
    if(!xMBPortEventPending())
    {
        if(g_mb_rx_busy)
        {
            /* 保守处理：让 TC 中断或下一帧超时自然释放 */
        }
    }
}
