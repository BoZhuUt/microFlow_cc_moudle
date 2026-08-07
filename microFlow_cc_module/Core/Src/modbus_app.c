/**
 * @file modbus_app.c
 * @brief Modbus RTU 应用层实现
 *
 * 结构与 TEC 工程保持一致，适配 microFlow 双串口。
 */

#include "modbus.h"
#include "mb.h"
#include "portevent.h"
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

/* Flash 自动保存标志 */
volatile uint8_t autoSavePending = 0;

/* ----------------------- 默认参数 ----------------------------------------*/
#define MODBUS_DEFAULT_ADDR        1
#define MODBUS_DEFAULT_BAUD        115200UL
#define MODBUS_DEFAULT_DATABITS     8
#define MODBUS_DEFAULT_PARITY      MB_PAR_NONE

/* ----------------------- 私有函数声明 ------------------------------------*/
static void Modbus_LoadDefaultParams(void);

/* ----------------------- 参数初始化 --------------------------------------*/
void SystemParam_init(void)
{
    /* 清零所有寄存器结构体 */
    memset(&system_status,    0, sizeof(system_status));
    memset(&comm_settings,    0, sizeof(comm_settings));
    memset(&measure_settings, 0, sizeof(measure_settings));
    memset(&calib_settings,   0, sizeof(calib_settings));
    memset(&filter_settings,  0, sizeof(filter_settings));
    memset(&measure_values,   0, sizeof(measure_values));
    memset(&rsvd_param,       0, sizeof(rsvd_param));

    /* 加载默认通信参数 */
    Modbus_LoadDefaultParams();
}

static void Modbus_LoadDefaultParams(void)
{
    comm_settings.modbusAddr    = MODBUS_DEFAULT_ADDR;
    comm_settings.modbusDatabits = MODBUS_DEFAULT_DATABITS;
    comm_settings.modbusParity   = (uint16_t)MODBUS_DEFAULT_PARITY;
    comm_settings.modbusBaud     = (uint32_t)MODBUS_DEFAULT_BAUD;
}

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

    /* 初始化寄存器参数 */
    SystemParam_init();

    /* 初始化事件层 */
    xMBPortEventInit();

    /*
     * eMBInit 参数：
     * - ucMode: MB_RTU
     * - ucSlaveAddr: 从站地址（后续可从 comm_settings 读取）
     * - ucPort: 0（Port 层忽略，双串口统一处理）
     * - ulBaudRate: 波特率
     * - eParity: 校验方式
     */
    eStatus = eMBInit(MB_RTU,
                      (UCHAR)comm_settings.modbusAddr,
                      0,
                      (ULONG)comm_settings.modbusBaud,
                      (eMBParity)comm_settings.modbusParity);

    if(eStatus != MB_ENOERR)
    {
        /* 初始化失败，可在此添加错误处理 */
        while(1);  /* 调试用断点 */
    }

    /* 使能协议栈 - 内部会调用 vMBPortSerialEnable(TRUE, FALSE) 开启 RXNE 中断 */
    eStatus = eMBEnable();
    if(eStatus != MB_ENOERR)
    {
        while(1);  /* 调试用断点 */
    }

    /*
     * 安全保障：显式确保 NVIC 和 RXNE 中断已开启
     * (Modbus_USART_Init 内部 DeInit/Init 可能影响中断状态)
     */
    HAL_NVIC_EnableIRQ(USART1_IRQn);
    HAL_NVIC_EnableIRQ(USART2_IRQn);
}

/* ----------------------- Poll 后事务释放 ----------------------------------*/
void MB_PortAfterPoll(void)
{
    /*
     * 阶段5：修正事务释放逻辑
     *
     * 在 eMBPoll() 返回后检查：
     * - 如果没有响应需要发送（广播、CRC 错误、地址不匹配等），
     *   且当前仍处于 RX 忙状态，说明帧已被协议栈消费但无响应，
     *   可以安全释放事务。
     *
     * 注意：有正常响应时，事务由 TC 中断释放，不能在这里提前释放。
     */

    /* 检查是否有待处理事件（表示有响应正在发送） */
    if(!xMBPortEventPending())
    {
        /*
         * 无待处理事件，且如果 g_mb_rx_busy 仍为 TRUE，
         * 说明是无需响应的帧（如广播或错误帧），安全释放。
         *
         * 如果 TC 中断已释放，g_mb_rx_busy 已为 FALSE，这里无害。
         */
        if(g_mb_rx_busy)
        {
            /* 仅当确认没有发送操作时才释放 */
            /* 这里保守处理：让 TC 中断或下一帧超时自然释放 */
        }
    }
}


