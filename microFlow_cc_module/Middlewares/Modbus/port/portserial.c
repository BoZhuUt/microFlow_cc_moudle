/**
 * @file portserial.c
 * @brief FreeModbus 串口 Port 层 - 双串口仲裁实现
 *
 * 结构与 TEC 工程保持一致，适配 microFlow：
 * - USART1: RS485 (PA9/PA10, 硬件 DE PA12)
 * - USART2: CH340 USB-to-USART (PA2/PA3)
 *
 * 关键设计：
 * - g_mb_rx_busy: 一路 UART 正在收帧时，另一路字节直接丢弃
 * - g_mb_rx_owner: 记录当前帧来源，响应从原路返回
 * - 硬件 DE: 不手动操作 PA12，由 HAL_RS485Ex_Init 控制
 */

#include "port.h"

/* ----------------------- Modbus includes ----------------------------------*/
#include "mb.h"
#include "mbport.h"
#include "modbus.h"

/* ----------------------- extern uart handles ------------------------------*/
extern UART_HandleTypeDef huart1;
extern UART_HandleTypeDef huart2;

/* ----------------------- Modbus uart source -------------------------------*/
typedef enum
{
    MB_UART_RS485 = 0,
    MB_UART_USB   = 1,
} MB_UART_SRC_t;

volatile MB_UART_SRC_t g_mb_uart_src = MB_UART_RS485;

/* 接收忙标志：一路 UART 正在收帧时，另一路的字节直接丢弃 */
volatile MB_UART_SRC_t g_mb_rx_owner = MB_UART_RS485;
volatile BOOL g_mb_rx_busy = FALSE;

/* ----------------------- static functions ---------------------------------*/
static void prvvUARTTxReadyISR(void);
static void prvvUARTRxISR(void);

static UART_HandleTypeDef *prvGetModbusUart(void)
{
    if(g_mb_uart_src == MB_UART_USB)
    {
        return &huart2;
    }

    return &huart1;
}

/* ----------------------- Start implementation -----------------------------*/
void vMBPortSerialEnable(BOOL xRxEnable, BOOL xTxEnable)
{
    if(xRxEnable)
    {
        /* 硬件 DE 模式：接收时 DE 自动由硬件控制 */
        __HAL_UART_ENABLE_IT(&huart1, UART_IT_RXNE);
        __HAL_UART_ENABLE_IT(&huart2, UART_IT_RXNE);
    }
    else
    {
        __HAL_UART_DISABLE_IT(&huart1, UART_IT_RXNE);
        __HAL_UART_DISABLE_IT(&huart2, UART_IT_RXNE);
    }

    if(xTxEnable)
    {
        UART_HandleTypeDef *huart = prvGetModbusUart();

        /* 硬件 DE 模式：发送时 DE 自动由硬件拉高 */
        if(huart == &huart1)
        {
            __HAL_UART_ENABLE_IT(&huart1, UART_IT_TXE);
        }
        else
        {
            __HAL_UART_ENABLE_IT(&huart2, UART_IT_TXE);
        }
    }
    else
    {
        __HAL_UART_DISABLE_IT(&huart1, UART_IT_TXE);
        __HAL_UART_DISABLE_IT(&huart2, UART_IT_TXE);

        /* 硬件 DE 模式：发送结束 DE 自动回落 */
    }
}

BOOL xMBPortSerialInit(UCHAR ucPORT, ULONG ulBaudRate, UCHAR ucDataBits, eMBParity eParity)
{
    Modbus_USART_Init(ulBaudRate, ucDataBits, eParity);

    return TRUE;
}

BOOL xMBPortSerialPutByte(CHAR ucByte)
{
    UART_HandleTypeDef *huart = prvGetModbusUart();

    /*
     * 直接写 TDR，不使用阻塞式 HAL_UART_Transmit。
     * 必须在 TXE 中断上下文或确保 TDR 为空时调用。
     */
    huart->Instance->TDR = (uint8_t)ucByte;

    return TRUE;
}

BOOL xMBPortSerialGetByte(CHAR *pucByte)
{
    UART_HandleTypeDef *huart = prvGetModbusUart();

    /*
     * RXNE 中断进来后，直接读 RDR。
     * 这比在中断里再调用 HAL_UART_Receive 更稳。
     */
    *pucByte = (CHAR)(huart->Instance->RDR & 0xFF);

    return TRUE;
}

/* ----------------------- ISR callbacks ------------------------------------*/
static void prvvUARTTxReadyISR(void)
{
    pxMBFrameCBTransmitterEmpty();
}

static void prvvUARTRxISR(void)
{
    pxMBFrameCBByteReceived();
}

/* ----------------------- USART1 RS485 IRQ user handler --------------------*/
void USART1_IRQHandler_USER(void)
{
    if(__HAL_UART_GET_FLAG(&huart1, UART_FLAG_RXNE) != RESET)
    {
        if(__HAL_UART_GET_IT_SOURCE(&huart1, UART_IT_RXNE) != RESET)
        {
            if(!g_mb_rx_busy || g_mb_rx_owner == MB_UART_RS485)
            {
                g_mb_rx_busy = TRUE;
                g_mb_rx_owner = MB_UART_RS485;
                g_mb_uart_src = MB_UART_RS485;
                mb_diag.usart1_rx_frames++;
                prvvUARTRxISR();
            }
            else
            {
                /* 另一路正在收帧，丢弃本字节 */
                (void)(huart1.Instance->RDR & 0xFF);
                mb_diag.conflict_discards++;
            }
        }
    }

    if(__HAL_UART_GET_FLAG(&huart1, UART_FLAG_TXE) != RESET)
    {
        if(__HAL_UART_GET_IT_SOURCE(&huart1, UART_IT_TXE) != RESET)
        {
            g_mb_uart_src = MB_UART_RS485;
            prvvUARTTxReadyISR();
        }
    }

    /* TC 中断：最后一个字节发送完成，释放事务 */
    if(__HAL_UART_GET_FLAG(&huart1, UART_FLAG_TC) != RESET)
    {
        if(__HAL_UART_GET_IT_SOURCE(&huart1, UART_IT_TC) != RESET)
        {
            __HAL_UART_CLEAR_FLAG(&huart1, UART_FLAG_TC);
            __HAL_UART_DISABLE_IT(&huart1, UART_IT_TC);
            mb_diag.usart1_tx_frames++;
            /* 恢复接收，释放 owner */
            g_mb_rx_busy = FALSE;
            g_mb_rx_owner = MB_UART_RS485;
            __HAL_UART_ENABLE_IT(&huart1, UART_IT_RXNE);
        }
    }

    /* 错误标志清除 */
    if(__HAL_UART_GET_FLAG(&huart1, UART_FLAG_PE) != RESET)
    {
        __HAL_UART_CLEAR_PEFLAG(&huart1);
        mb_diag.usart1_pe_count++;
    }

    if(__HAL_UART_GET_FLAG(&huart1, UART_FLAG_ORE) != RESET)
    {
        __HAL_UART_CLEAR_OREFLAG(&huart1);
        mb_diag.usart1_ore_count++;
    }

    if(__HAL_UART_GET_FLAG(&huart1, UART_FLAG_FE) != RESET)
    {
        __HAL_UART_CLEAR_FEFLAG(&huart1);
        mb_diag.usart1_fe_count++;
    }

    if(__HAL_UART_GET_FLAG(&huart1, UART_FLAG_NE) != RESET)
    {
        __HAL_UART_CLEAR_NEFLAG(&huart1);
        mb_diag.usart1_ne_count++;
    }

    HAL_NVIC_ClearPendingIRQ(USART1_IRQn);
}

/* ----------------------- USART2 CH340 IRQ user handler --------------------*/
void USART2_IRQHandler_USER(void)
{
    if(__HAL_UART_GET_FLAG(&huart2, UART_FLAG_RXNE) != RESET)
    {
        if(__HAL_UART_GET_IT_SOURCE(&huart2, UART_IT_RXNE) != RESET)
        {
            if(!g_mb_rx_busy || g_mb_rx_owner == MB_UART_USB)
            {
                g_mb_rx_busy = TRUE;
                g_mb_rx_owner = MB_UART_USB;
                g_mb_uart_src = MB_UART_USB;
                mb_diag.usart2_rx_frames++;
                prvvUARTRxISR();
            }
            else
            {
                /* 另一路正在收帧，丢弃本字节 */
                (void)(huart2.Instance->RDR & 0xFF);
                mb_diag.conflict_discards++;
            }
        }
    }

    if(__HAL_UART_GET_FLAG(&huart2, UART_FLAG_TXE) != RESET)
    {
        if(__HAL_UART_GET_IT_SOURCE(&huart2, UART_IT_TXE) != RESET)
        {
            g_mb_uart_src = MB_UART_USB;
            prvvUARTTxReadyISR();
        }
    }

    /* TC 中断：最后一个字节发送完成，释放事务 */
    if(__HAL_UART_GET_FLAG(&huart2, UART_FLAG_TC) != RESET)
    {
        if(__HAL_UART_GET_IT_SOURCE(&huart2, UART_IT_TC) != RESET)
        {
            __HAL_UART_CLEAR_FLAG(&huart2, UART_FLAG_TC);
            __HAL_UART_DISABLE_IT(&huart2, UART_IT_TC);
            mb_diag.usart2_tx_frames++;
            /* 恢复接收，释放 owner */
            g_mb_rx_busy = FALSE;
            g_mb_rx_owner = MB_UART_USB;
            __HAL_UART_ENABLE_IT(&huart2, UART_IT_RXNE);
        }
    }

    /* 错误标志清除 */
    if(__HAL_UART_GET_FLAG(&huart2, UART_FLAG_PE) != RESET)
    {
        __HAL_UART_CLEAR_PEFLAG(&huart2);
        mb_diag.usart2_pe_count++;
    }

    if(__HAL_UART_GET_FLAG(&huart2, UART_FLAG_ORE) != RESET)
    {
        __HAL_UART_CLEAR_OREFLAG(&huart2);
        mb_diag.usart2_ore_count++;
    }

    if(__HAL_UART_GET_FLAG(&huart2, UART_FLAG_FE) != RESET)
    {
        __HAL_UART_CLEAR_FEFLAG(&huart2);
        mb_diag.usart2_fe_count++;
    }

    if(__HAL_UART_GET_FLAG(&huart2, UART_FLAG_NE) != RESET)
    {
        __HAL_UART_CLEAR_NEFLAG(&huart2);
        mb_diag.usart2_ne_count++;
    }

    HAL_NVIC_ClearPendingIRQ(USART2_IRQn);
}
