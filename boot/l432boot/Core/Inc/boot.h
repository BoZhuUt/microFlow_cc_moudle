#ifndef BOOT_H
#define BOOT_H
#include "main.h"
/* 128 KB Flash / 2 KB 每页：Boot 32 KB，APP 94 KB，末页参数 2 KB。
 * APP 上界为开区间，修改时必须同步 APP 链接脚本、VTOR 和升级工具。
 */
#define BOOT_APP_ADDRESS 0x08008000UL
#define BOOT_APP_END     0x0801F800UL /* Exclusive: parameter page remains intact. */
#define BOOT_APP_SIZE    (BOOT_APP_END - BOOT_APP_ADDRESS)
/* 时钟及 RS485 串口初始化后调用；固定 9600/8N1，不返回正常主循环。
 * 未命中且 APP 入口合法时跳转；否则进入旧上位机兼容的升级菜单。
 */
void Boot_Run(UART_HandleTypeDef *uart);
#endif
