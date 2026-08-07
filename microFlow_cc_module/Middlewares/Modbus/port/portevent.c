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

/* ----------------------- Modbus includes ----------------------------------*/
#include "mb.h"
#include "mbport.h"

/* ----------------------- Variables ----------------------------------------*/
static volatile eMBEventType eQueuedEvent;
static volatile BOOL     xEventInQueue;

/* ----------------------- Start implementation -----------------------------*/
BOOL
xMBPortEventInit( void )
{
    xEventInQueue = FALSE;
    eQueuedEvent = EV_READY;
    return TRUE;
}

BOOL
xMBPortEventPost( eMBEventType eEvent )
{
    ENTER_CRITICAL_SECTION();
    xEventInQueue = TRUE;
    eQueuedEvent = eEvent;
    EXIT_CRITICAL_SECTION();
    return TRUE;
}

BOOL
xMBPortEventGet( eMBEventType * eEvent )
{
    BOOL            xEventHappened = FALSE;

    ENTER_CRITICAL_SECTION();
    if( xEventInQueue )
    {
        *eEvent = eQueuedEvent;
        xEventInQueue = FALSE;
        xEventHappened = TRUE;
    }
    EXIT_CRITICAL_SECTION();

    return xEventHappened;
}

/* ----------------------- 查询接口 -----------------------------------------*/
BOOL
xMBPortEventPending( void )
{
    return xEventInQueue;
}
