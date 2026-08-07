/**
 * @file parameter.h
 * @brief 参数初始化与检查函数声明（autoRegmap 自动生成部分）
 */

#ifndef __PARAMETER_H
#define __PARAMETER_H

#include <stdint.h>

/**
 * @brief 用默认值初始化所有寄存器参数
 *
 * 来自 parameter.c (ProbeRegmapGenerator 生成)
 */
void ParametarsInitByDefaultValue(void);

/**
 * @brief 检查所有参数是否在合法范围内
 *
 * 越界的参数会被替换为默认值。
 * @return 被修正的参数数量（0 表示全部合法）
 */
uint8_t CheckParamyDefaultSetting(void);

#endif /* __PARAMETER_H */
