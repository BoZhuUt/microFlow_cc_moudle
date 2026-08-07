/**
 * @file cv.h
 * @brief 校准参数类型定义（空桩，待 autoRegmap 工具完善后补充）
 *
 * 本文件仅用于保证 regmap.h 编译通过。
 * 正式内容由 Middlewares/autoRegmap/regmap_define/sub_struct/ 下的 Excel 定义生成。
 */

#ifndef __CV_H
#define __CV_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* 校准初始化参数类型 - 待正式定义 */
typedef struct {
    uint32_t reserved;
} CV_InitTypeDef;

/* 测量参数类型 - 待正式定义 */
typedef struct {
    uint32_t reserved;
} It_MeasureParamTypeDef;

#ifdef __cplusplus
}
#endif

#endif /* __CV_H */
