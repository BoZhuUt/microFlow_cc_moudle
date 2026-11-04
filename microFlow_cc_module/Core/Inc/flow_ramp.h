#ifndef FLOW_RAMP_H
#define FLOW_RAMP_H

#include <stdint.h>

#define FLOW_RAMP_VERSION 0x5201U
#define FLOW_RAMP_MAX_SECONDS 604800U /* Seven days per time parameter. */

/* Private control-algorithm snapshot. All register/persistent fields are direct
 * members of RSVD_PARAM_T; this type is not a Modbus or Flash parameter group. */
#pragma pack(push, 2)
typedef struct {
    uint16_t rampVersion;       /* 48031 */
    uint16_t rampReserved;      /* 48032 */
    float rampCenter;           /* 48033: A, mL/min */
    float rampSpan;             /* 48035: B, mL/min */
    float rampTolerance;        /* 48037: absolute flow tolerance */
    uint32_t rampSeconds;       /* 48039: one-way ramp, excludes holds */
    uint32_t rampHoldLow;       /* 48041: effective hold seconds */
    uint32_t rampHoldMid;       /* 48043 */
    uint32_t rampHoldHigh;      /* 48045 */
    uint32_t rampStableSeconds; /* 48047: continuous settling confirmation */
    uint32_t rampMaxLow;        /* 48049: total point-stage budget */
    uint32_t rampMaxMid;        /* 48051 */
    uint32_t rampMaxHigh;       /* 48053 */
    uint32_t rampLimitSeconds;  /* 48055: continuous unreachable confirmation */
    float rampValveMin;         /* 48057: effective valve lower limit, % */
    float rampValveMax;         /* 48059: valve upper limit, % */
    uint16_t reserved[22];
} FLOW_RAMP_CONFIG_T;
#pragma pack(pop)

enum {
    RAMP_IDLE, RAMP_HOLD_LOW, RAMP_UP_LOW_MID, RAMP_HOLD_MID_UP,
    RAMP_UP_MID_HIGH, RAMP_HOLD_HIGH, RAMP_DOWN_HIGH_MID,
    RAMP_HOLD_MID_DOWN, RAMP_DOWN_MID_LOW, RAMP_FAULT
};
enum {
    RAMP_OK, RAMP_BAD_CONFIG, RAMP_POINT_TIMEOUT,
    RAMP_INSUFFICIENT_FLOW, RAMP_BELOW_RANGE
};

typedef struct {
    FLOW_RAMP_CONFIG_T config; /* Snapshot taken on mode entry. */
    uint16_t state, fault;
    float target;
    uint32_t elapsedMs, holdMs, stableMs, limitMs, cycles;
    uint8_t previousInBand, previousQualified, limitKind;
} FlowRamp;

void FlowRamp_Defaults(FLOW_RAMP_CONFIG_T *config);
int FlowRamp_Validate(const FLOW_RAMP_CONFIG_T *config);
void FlowRamp_Start(FlowRamp *ramp, const FLOW_RAMP_CONFIG_T *config);
void FlowRamp_Update(FlowRamp *ramp, uint32_t dtMs, float measured,
                     int sampleValid, float valveOpening);

#endif
