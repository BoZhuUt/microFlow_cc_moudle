#ifndef PARAMETER_STORAGE_H
#define PARAMETER_STORAGE_H
#include "regmap.h"

/* FLOW_RAMP_CONFIG_T is only a control-algorithm snapshot, not a register group. */
void RsvdParam_GetRamp(const RSVD_PARAM_T *parameters, FLOW_RAMP_CONFIG_T *config);
void RsvdParam_DefaultRamp(RSVD_PARAM_T *parameters);
/* extended=0 imports the old 60-byte prefix plus old group-44 ramp config. */
void RsvdParam_Decode(RSVD_PARAM_T *parameters, const void *stored,
                       int extended, const void *legacyRamp);
void RsvdParam_PrepareFlash(RSVD_PARAM_T *stored, const RSVD_PARAM_T *parameters);
#endif
