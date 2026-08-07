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
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 * File: $Id$
 */

/* ----------------------- Platform includes --------------------------------*/
#include "port.h"

/* ----------------------- Modbus includes ----------------------------------*/
#include "mb.h"
#include "mbport.h"
#include "modbus.h"

/* ----------------------- static functions ---------------------------------*/
void prvvTIMERExpiredISR( void );

/* ----------------------- Start implementation -----------------------------*/
BOOL
xMBPortTimersInit( USHORT usTim1Timerout50us )
{
	Modbus_TIM7_Init(usTim1Timerout50us);
	return TRUE;
}


inline void
vMBPortTimersEnable(  )
{
	__HAL_TIM_CLEAR_IT(&htim7,TIM_IT_UPDATE);	//清除定时器中断
	__HAL_TIM_ENABLE_IT(&htim7,TIM_IT_UPDATE);//使能定时器中断
	__HAL_TIM_SetCounter(&htim7,0);						//计数器清0
	__HAL_TIM_ENABLE(&htim7);									//定时器使能
  /* Enable the timer with the timeout passed to xMBPortTimersInit( ) */
}

inline void
vMBPortTimersDisable(  )
{
	__HAL_TIM_DISABLE(&htim7);									//关闭定时器
	__HAL_TIM_SetCounter(&htim7,0);							//计数器清0
	__HAL_TIM_DISABLE_IT(&htim7,TIM_IT_UPDATE);
	__HAL_TIM_CLEAR_IT(&htim7,TIM_IT_UPDATE);
	/* Disable any pending timers. */
}

/* Create an ISR which is called whenever the timer has expired. This function
 * must then call pxMBPortCBTimerExpired( ) to notify the protocol stack that
 * the timer has expired.
 */
void prvvTIMERExpiredISR( void )
{
    extern volatile BOOL g_mb_rx_busy;
    g_mb_rx_busy = FALSE;
    ( void )pxMBPortCBTimerExpired(  );
}
