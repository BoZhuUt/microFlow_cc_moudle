/* RSVD 参数的新旧布局转换；保留配置，保存副本及加载结果均清零运行进度。
 * Flash 擦写和布局标记由 flash.c 负责，本模块不访问硬件。
 */
#include "parameter_storage.h"
#include <stddef.h>
#include <string.h>

_Static_assert(sizeof(RSVD_PARAM_T) == 208, "RSVD must contain 104 registers");
_Static_assert(offsetof(RSVD_PARAM_T, rampVersion) == 60, "Keep existing calibration offsets");
_Static_assert(offsetof(RSVD_PARAM_T, rampState) == 164, "Keep public telemetry addresses");
_Static_assert(offsetof(RSVD_PARAM_T, rampState) - offsetof(RSVD_PARAM_T, rampVersion)
               == sizeof(FLOW_RAMP_CONFIG_T), "Ramp snapshot must match register members");

void RsvdParam_GetRamp(const RSVD_PARAM_T *p, FLOW_RAMP_CONFIG_T *config)
{
    memcpy(config, &p->rampVersion, sizeof(*config));
}

void RsvdParam_DefaultRamp(RSVD_PARAM_T *p)
{
    FLOW_RAMP_CONFIG_T config;
    FlowRamp_Defaults(&config);
    memcpy(&p->rampVersion, &config, sizeof(config));
}

static void clearRuntime(RSVD_PARAM_T *p)
{
    memset(&p->rampState, 0, offsetof(RSVD_PARAM_T, reserved) - offsetof(RSVD_PARAM_T, rampState));
}

void RsvdParam_Decode(RSVD_PARAM_T *p, const void *stored, int extended, const void *legacyRamp)
{
    if (extended) {
        memcpy(p, stored, sizeof(*p));
    } else {
        FLOW_RAMP_CONFIG_T config;
        memset(p, 0, sizeof(*p));
        /* Old 48001..48030 includes all flow calibration/model/PID fields. */
        memcpy(p, stored, offsetof(RSVD_PARAM_T, rampVersion));
        memcpy(&config, legacyRamp, sizeof(config));
        if (config.rampVersion != FLOW_RAMP_VERSION) FlowRamp_Defaults(&config);
        memcpy(&p->rampVersion, &config, sizeof(config));
    }
    clearRuntime(p);
}

void RsvdParam_PrepareFlash(RSVD_PARAM_T *stored, const RSVD_PARAM_T *p)
{
    memcpy(stored, p, sizeof(*stored));
    clearRuntime(stored);
}
