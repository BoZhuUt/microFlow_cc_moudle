/*
 * FreeModbus Libary: BARE Port
 * Copyright (C) 2006 Christian Walter <wolti@sil.at>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1307  USA
 *
 * File: $Id$
 */

#ifndef _PORT_H
#define _PORT_H

#include <assert.h>
#include <inttypes.h>
#include "stm32l4xx.h"
#include "FreeRTOS.h"
#include "task.h"

#define	INLINE                      inline
#define PR_BEGIN_EXTERN_C           extern "C" {
#define	PR_END_EXTERN_C             }

/*
 * 临界区：使用原始中断屏蔽，兼容任务和 ISR 上下文
 *
 * FreeModbus 的 mbrtu.c 会在 UART ISR 回调链路中调用 ENTER_CRITICAL_SECTION：
 *   USARTx_IRQHandler → prvvUARTRxISR → pxMBFrameCBByteReceived
 *     → eMBRTUReceiveFSM → ENTER_CRITICAL_SECTION
 *
 * 而 taskENTER_CRITICAL() 内部调用 vPortEnterCritical()，其中包含：
 *   configASSERT( (portNVIC_INT_CTRL_REG & portVECTACTIVE_MASK) == 0 );
 * 该断言检测是否在中断上下文中调用，若在 ISR 中触发会导致死循环。
 *
 * TEC 工程使用相同的 taskENTER_CRITICAL() 但可能因编译优化或断言配置不同而未暴露此问题。
 * 此处改用 __disable_irq()/__enable_irq() 确保在两种上下文中均安全。
 */
#define ENTER_CRITICAL_SECTION( )   __disable_irq()
#define EXIT_CRITICAL_SECTION( )    __enable_irq()

typedef uint8_t BOOL;

typedef unsigned char UCHAR;
typedef char CHAR;

typedef uint16_t USHORT;
typedef int16_t SHORT;

typedef uint32_t ULONG;
typedef int32_t LONG;

#ifndef TRUE
#define TRUE            1
#endif

#ifndef FALSE
#define FALSE           0
#endif

#endif
