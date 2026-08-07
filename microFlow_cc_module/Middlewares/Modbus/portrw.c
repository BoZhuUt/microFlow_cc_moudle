#include "modbus.h"
#include "portrw.h"
#include "FreeRTOS.h"
#include "task.h"

//寄存器地址间距
#define INDEX_BASE	1000	

/**
  * @brief  读输入寄存器
  * @param  pucRegBuffer：返回数据指针
						usAddress		：输入寄存器起始地址
						usNRegs			：要读的寄存器数目		
  * @retval 错误代码
  */
eMBErrorCode eMBRegInputCB( UCHAR * pucRegBuffer, USHORT usAddress, USHORT usNRegs )
{
	volatile uint8_t test;
	uint16_t iRegIndex,uRegs;
	uint16_t *data_pointer;
	eMBErrorCode eStatus = MB_ENOERR;

	uRegs= usAddress/INDEX_BASE;

	//根据地址确定要访问的寄存器
	switch( uRegs )
	{
		//非法地址
		default:
			eStatus=MB_ENOREG;
			break;
	}
	
	if( eStatus != MB_ENOREG )
	{		
		iRegIndex= usAddress % INDEX_BASE;
		iRegIndex--;		
		
		//循环读数据
		while( usNRegs > 0 )
		{	
			//获取高位字节
			*pucRegBuffer++ = ( uint8_t )( *(data_pointer + iRegIndex) >> 8 );				
			
			//获取低位字节
			*pucRegBuffer++ = ( uint8_t )( *(data_pointer + iRegIndex) & 0xFF );				
			
			iRegIndex++;
			
			usNRegs--;
		}
	}	

	return eStatus;
}



/**
  * @brief  读/写保持寄存器
  * @param  
			pucRegBuffer：数据指针
			usAddress		：输入寄存器起始地址
			usNRegs			：要操作的寄存器数目	
			eMode				: 对保持寄存器的操作（读或写）
  * @retval 错误代码
  */
char MBAddr_w=0;
eMBErrorCode eMBRegHoldingCB( UCHAR * pucRegBuffer, USHORT usAddress, USHORT usNRegs,eMBRegisterMode eMode )
{	
	uint16_t iRegIndex,uRegs;
	uint16_t *data_pointer;
	eMBErrorCode eStatus = MB_ENOERR;		
	
	uRegs= usAddress/INDEX_BASE;
	
	//根据地址确定要访问的寄存器
	switch( uRegs )
	{
		//通信参数设置寄存器
		case COMSREG_FLAG:
			//判断地址和寄存器数目合法性
			if(usAddress + usNRegs <= COMSREG_SADDR + COMSREG_NREGS)
			{
				data_pointer= (void *)&comm_settings;
			}
			else
			{
				eStatus=MB_ENOREG;
			}
			break;
		
		//测量设置寄存器
		case MSREG_FLAG:
			//判断地址和寄存器数目合法性
			if(usAddress + usNRegs <= MSREG_SADDR + MSREG_NREGS)
			{
				data_pointer= (void *)&measure_settings;
			}
			else
			{
				eStatus=MB_ENOREG;
			}
			break;
		
		//校准设置寄存器
		case CALSREG_FLAG:
			//判断地址和寄存器数目合法性
			if(usAddress + usNRegs <= CALSREG_SADDR + CALSREG_NREGS)
			{
				data_pointer= (void *)&calib_settings;
			}
			else
			{
				eStatus=MB_ENOREG;
			}
			break;
			
		//滤波器参数设置寄存器
		case FSREG_FLAG:
			//判断地址和寄存器数目合法性
			if(usAddress + usNRegs <= FSREG_SADDR + FSREG_NREGS)
			{
				data_pointer= (void *)&filter_settings;
			}
			else
			{
				eStatus=MB_ENOREG;
			}
			break;
		//DO专有寄存器
		case RSVD_FLAG :
			if(usAddress + usNRegs <= RSVD_SADDR + RSVD_NREGS)
			{
				data_pointer= (void *)&rsvd_param;
			}
			else
			{
				eStatus=MB_ENOREG;
			}
			break;
		//系统状态寄存器
		case SYSREG_FLAG:
			//判断地址和寄存器数目合法性
			if(usAddress + usNRegs <= SYSREG_SADDR + SYSREG_NREGS)
			{
				data_pointer= (void *)&system_status;
			}
			else
			{
				eStatus=MB_ENOREG;
			}
			break;
		//测量数据寄存器
		case MVREG_FLAG:
			//判断地址和寄存器数目合法性
			if(usAddress + usNRegs <= MVREG_SADDR + MVREG_NREGS)
			{
				data_pointer= (void *)&measure_values;
			}
			else
			{
				eStatus=MB_ENOREG;			
			}
			break;			
		//非法地址
		default:
			eStatus=MB_ENOREG;
			break;

	}
	
	if(eStatus != MB_ENOREG)
	{		
		iRegIndex= usAddress % INDEX_BASE;
		iRegIndex--;
		
		switch(eMode)
		{
			//读保持寄存器
			case MB_REG_READ:
				taskENTER_CRITICAL();				
				while( usNRegs > 0 )
				{						
					//获取高位字节
					*pucRegBuffer++ = ( uint8_t )( *(data_pointer + iRegIndex) >> 8 );				
					
					//获取低位字节
					*pucRegBuffer++ = ( uint8_t )( *(data_pointer +iRegIndex) & 0xFF );				
					
					iRegIndex++;
					
					usNRegs--;
				}
				taskEXIT_CRITICAL();
				break;
				
			//写保持寄存器
			case MB_REG_WRITE:
				taskENTER_CRITICAL();
				while( usNRegs > 0 )
				{
					*(data_pointer + iRegIndex) = *pucRegBuffer++ << 8;
                                        *(data_pointer + iRegIndex) |=
                                            *pucRegBuffer++;
                    autoSavePending = 1; // set the flag to save the data to FLASH
					iRegIndex++;
					usNRegs--;
				}
				taskEXIT_CRITICAL();
				if(usAddress==42001)  //modbus address
				{
					MBAddr_w=1;
				}
				break;		

			default:
				break;
		}
	}

	return eStatus;
}

/**
  * @brief  读/写线圈寄存器
  * @param  pucRegBuffer：数据指针
			usAddress	：寄存器起始地址
			usNCoils	：要操作的线圈数目	
			eMode		：操作模式
  * @retval 错误代码
  */
eMBErrorCode eMBRegCoilsCB( UCHAR * pucRegBuffer, USHORT usAddress, USHORT usNCoils,eMBRegisterMode eMode )
{	
	return MB_ENOREG;
}

/**
  * @brief  读离散寄存器
  * @param  pucRegBuffer：返回数据指针
			usAddress	：寄存器起始地址
			usNDiscrete	：要读的寄存器数目		
  * @retval 错误代码
  */
eMBErrorCode eMBRegDiscreteCB( UCHAR * pucRegBuffer, USHORT usAddress, USHORT usNDiscrete )
{	
	return MB_ENOREG;
}

/******************* (C) COPYRIGHT 2015 AndyChen *******END OF FILE*************/
