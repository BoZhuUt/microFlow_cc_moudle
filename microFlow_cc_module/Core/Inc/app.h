#ifndef _APP_H__
#define _APP_H__
#include "main.h"
#include "cmsis_os.h"

/* ======================== Modbus 命令码 ========================*/
/* 通过写 measure_settings.command 或 rsvd_param.command 触发 */
#define RestoreFlashCMD     4    /* 恢复出厂设置 */
#define SaveToFlahCMD       5    /* 保存当前参数到 Flash */
#define JumpBootloaderCMD   7    /* 跳转到 Bootloader 并复位 */
#define SaveParToFactory    100  /* 将当前参数保存为新的出厂默认值 */
#define LoadParFromFactory  103  /* 从出厂默认值区加载参数覆盖当前值 */

/* ======================== 函数声明 ========================*/
void MeasureFunc(void);
void saveParametersToFlash(void);
void updateParameters(void);

#endif /* _APP_H__ */
