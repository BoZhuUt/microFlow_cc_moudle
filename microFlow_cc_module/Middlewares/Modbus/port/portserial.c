/**
 * @file portserial.c
 * @brief FreeModbus 串口 Port 层 - 双串口事务仲裁
 *
 * - USART1: RS485（PA9/PA10，PA12 硬件 DE）
 * - USART2: CH340 USB-to-USART（PA2/PA3）
 *
 * 两个物理串口共用一套 FreeModbus RTU 状态机。先收到首字节的串口
 * 独占整个请求/响应事务；无响应请求在 Poll 后释放，有响应请求在
 * 最后一个停止位发送完成（TC）后释放。
 */

#include "port.h"

#include "mb.h"
#include "mbport.h"
#include "modbus.h"
#include "portevent.h"

extern UART_HandleTypeDef huart1;
extern UART_HandleTypeDef huart2;

typedef enum
{
    MB_UART_NONE = 0,
    MB_UART_RS485,
    MB_UART_USB,
} MB_UART_SOURCE_t;

typedef enum
{
    MB_TRANS_IDLE = 0,
    MB_TRANS_RX,
    MB_TRANS_FRAME_READY,
    MB_TRANS_TX,
    MB_TRANS_TX_DRAIN,
} MB_TRANSACTION_STATE_t;

static volatile MB_UART_SOURCE_t mb_owner = MB_UART_NONE;
static volatile MB_TRANSACTION_STATE_t mb_state = MB_TRANS_IDLE;

static void prvvUARTTxReadyISR(void);
static void prvvUARTRxISR(void);
static void prvUARTIRQHandler(UART_HandleTypeDef *huart,
                              MB_UART_SOURCE_t source);

static UART_HandleTypeDef *prvGetOwnerUart(void)
{
    return (mb_owner == MB_UART_USB) ? &huart2 : &huart1;
}

static void prvFlushPendingRx(UART_HandleTypeDef *huart)
{
    if(__HAL_UART_GET_FLAG(huart, UART_FLAG_RXNE) != RESET)
    {
        (void)(huart->Instance->RDR & 0xFFU);
    }

    if(__HAL_UART_GET_FLAG(huart, UART_FLAG_ORE) != RESET)
    {
        __HAL_UART_CLEAR_OREFLAG(huart);
    }
}

static void prvReleaseTransaction(void)
{
    uint32_t primask = __get_PRIMASK();

    __disable_irq();

    __HAL_UART_DISABLE_IT(&huart1, UART_IT_TXE);
    __HAL_UART_DISABLE_IT(&huart2, UART_IT_TXE);
    __HAL_UART_DISABLE_IT(&huart1, UART_IT_TC);
    __HAL_UART_DISABLE_IT(&huart2, UART_IT_TC);

    /* 事务锁定期间到达的数据不属于下一帧，释放前统一丢弃。 */
    prvFlushPendingRx(&huart1);
    prvFlushPendingRx(&huart2);

    mb_owner = MB_UART_NONE;
    mb_state = MB_TRANS_IDLE;

    __HAL_UART_ENABLE_IT(&huart1, UART_IT_RXNE);
    __HAL_UART_ENABLE_IT(&huart2, UART_IT_RXNE);

    __set_PRIMASK(primask);
}

void vMBPortSerialEnable(BOOL xRxEnable, BOOL xTxEnable)
{
    UART_HandleTypeDef *huart;

    if(xTxEnable)
    {
        huart = prvGetOwnerUart();

        __HAL_UART_DISABLE_IT(&huart1, UART_IT_RXNE);
        __HAL_UART_DISABLE_IT(&huart2, UART_IT_RXNE);
        __HAL_UART_DISABLE_IT(&huart1, UART_IT_TXE);
        __HAL_UART_DISABLE_IT(&huart2, UART_IT_TXE);

        mb_state = MB_TRANS_TX;
        __HAL_UART_ENABLE_IT(huart, UART_IT_TXE);
        return;
    }

    /* FreeModbus 在最后一个字节装入 TDR 后请求恢复接收。此时必须先等 TC。 */
    if(xRxEnable && mb_state == MB_TRANS_TX)
    {
        huart = prvGetOwnerUart();

        __HAL_UART_DISABLE_IT(huart, UART_IT_TXE);
        mb_state = MB_TRANS_TX_DRAIN;
        __HAL_UART_ENABLE_IT(huart, UART_IT_TC);
        return;
    }

    __HAL_UART_DISABLE_IT(&huart1, UART_IT_TXE);
    __HAL_UART_DISABLE_IT(&huart2, UART_IT_TXE);

    if(xRxEnable)
    {
        __HAL_UART_ENABLE_IT(&huart1, UART_IT_RXNE);
        __HAL_UART_ENABLE_IT(&huart2, UART_IT_RXNE);
    }
    else
    {
        __HAL_UART_DISABLE_IT(&huart1, UART_IT_RXNE);
        __HAL_UART_DISABLE_IT(&huart2, UART_IT_RXNE);
        __HAL_UART_DISABLE_IT(&huart1, UART_IT_TC);
        __HAL_UART_DISABLE_IT(&huart2, UART_IT_TC);
        mb_owner = MB_UART_NONE;
        mb_state = MB_TRANS_IDLE;
    }
}

BOOL xMBPortSerialInit(UCHAR ucPORT, ULONG ulBaudRate, UCHAR ucDataBits,
                       eMBParity eParity)
{
    (void)ucPORT;
    Modbus_USART_Init(ulBaudRate, ucDataBits, eParity);
    return TRUE;
}

BOOL xMBPortSerialPutByte(CHAR ucByte)
{
    UART_HandleTypeDef *huart = prvGetOwnerUart();

    huart->Instance->TDR = (uint8_t)ucByte;
    return TRUE;
}

BOOL xMBPortSerialGetByte(CHAR *pucByte)
{
    UART_HandleTypeDef *huart = prvGetOwnerUart();

    *pucByte = (CHAR)(huart->Instance->RDR & 0xFFU);
    return TRUE;
}

static void prvvUARTTxReadyISR(void)
{
    pxMBFrameCBTransmitterEmpty();
}

static void prvvUARTRxISR(void)
{
    pxMBFrameCBByteReceived();
}

void MB_PortOnT35Expired(void)
{
    if(mb_state == MB_TRANS_RX)
    {
        if(mb_owner == MB_UART_RS485)
        {
            mb_diag.usart1_rx_frames++;
        }
        else if(mb_owner == MB_UART_USB)
        {
            mb_diag.usart2_rx_frames++;
        }

        mb_state = MB_TRANS_FRAME_READY;
    }
}

void MB_PortAfterPoll(void)
{
    /*
     * 有效本站请求会留下 EV_EXECUTE，随后进入 TX；CRC 错误、非本站帧、
     * 接收错误以及已经执行完的广播请求没有后续事件，可在这里释放。
     */
    if((mb_state == MB_TRANS_FRAME_READY) && !xMBPortEventPending())
    {
        prvReleaseTransaction();
    }
}

static void prvUARTIRQHandler(UART_HandleTypeDef *huart,
                              MB_UART_SOURCE_t source)
{
    if((__HAL_UART_GET_FLAG(huart, UART_FLAG_RXNE) != RESET) &&
       (__HAL_UART_GET_IT_SOURCE(huart, UART_IT_RXNE) != RESET))
    {
        if(mb_state == MB_TRANS_IDLE)
        {
            mb_owner = source;
            mb_state = MB_TRANS_RX;
        }

        if((mb_state == MB_TRANS_RX) && (mb_owner == source))
        {
            prvvUARTRxISR();
        }
        else
        {
            (void)(huart->Instance->RDR & 0xFFU);
            mb_diag.conflict_discards++;
        }
    }

    if((__HAL_UART_GET_FLAG(huart, UART_FLAG_TXE) != RESET) &&
       (__HAL_UART_GET_IT_SOURCE(huart, UART_IT_TXE) != RESET))
    {
        if((mb_state == MB_TRANS_TX) && (mb_owner == source))
        {
            prvvUARTTxReadyISR();
        }
        else
        {
            __HAL_UART_DISABLE_IT(huart, UART_IT_TXE);
        }
    }

    if((__HAL_UART_GET_FLAG(huart, UART_FLAG_TC) != RESET) &&
       (__HAL_UART_GET_IT_SOURCE(huart, UART_IT_TC) != RESET))
    {
        __HAL_UART_CLEAR_FLAG(huart, UART_CLEAR_TCF);
        __HAL_UART_DISABLE_IT(huart, UART_IT_TC);

        if((mb_state == MB_TRANS_TX_DRAIN) && (mb_owner == source))
        {
            if(source == MB_UART_RS485)
            {
                mb_diag.usart1_tx_frames++;
            }
            else
            {
                mb_diag.usart2_tx_frames++;
            }

            prvReleaseTransaction();
        }
    }

    if(__HAL_UART_GET_FLAG(huart, UART_FLAG_PE) != RESET)
    {
        __HAL_UART_CLEAR_PEFLAG(huart);
        if(source == MB_UART_RS485)
            mb_diag.usart1_pe_count++;
        else
            mb_diag.usart2_pe_count++;
    }

    if(__HAL_UART_GET_FLAG(huart, UART_FLAG_ORE) != RESET)
    {
        __HAL_UART_CLEAR_OREFLAG(huart);
        if(source == MB_UART_RS485)
            mb_diag.usart1_ore_count++;
        else
            mb_diag.usart2_ore_count++;
    }

    if(__HAL_UART_GET_FLAG(huart, UART_FLAG_FE) != RESET)
    {
        __HAL_UART_CLEAR_FEFLAG(huart);
        if(source == MB_UART_RS485)
            mb_diag.usart1_fe_count++;
        else
            mb_diag.usart2_fe_count++;
    }

    if(__HAL_UART_GET_FLAG(huart, UART_FLAG_NE) != RESET)
    {
        __HAL_UART_CLEAR_NEFLAG(huart);
        if(source == MB_UART_RS485)
            mb_diag.usart1_ne_count++;
        else
            mb_diag.usart2_ne_count++;
    }
}

void USART1_IRQHandler_USER(void)
{
    prvUARTIRQHandler(&huart1, MB_UART_RS485);
}

void USART2_IRQHandler_USER(void)
{
    prvUARTIRQHandler(&huart2, MB_UART_USB);
}
