/* 流量缓变状态机：只生成目标和状态，阀门输出仍由 main.c 的 PID 控制。
 * 启动时保存配置快照；有效停留和阶段总时限分别计时，故障锁存至重新启动。
 */
#include "flow_ramp.h"
#include <math.h>
#include <string.h>

static uint32_t addMs(uint32_t value, uint32_t delta)
{
    return delta > UINT32_MAX - value ? UINT32_MAX : value + delta;
}

void FlowRamp_Defaults(FLOW_RAMP_CONFIG_T *c)
{
    memset(c, 0, sizeof(*c));
    c->rampVersion = FLOW_RAMP_VERSION;
    c->rampCenter = 5.0f;
    c->rampSpan = 1.0f;
    c->rampTolerance = 0.2f;
    c->rampSeconds = 3600U;
    c->rampHoldLow = c->rampHoldMid = c->rampHoldHigh = 600U;
    c->rampStableSeconds = 10U;
    c->rampMaxLow = c->rampMaxMid = c->rampMaxHigh = 1800U;
    c->rampLimitSeconds = 60U;
    c->rampValveMin = 20.0f;
    c->rampValveMax = 99.0f;
}

static int validPoint(uint32_t hold, uint32_t stable, uint32_t budget)
{
    return hold <= FLOW_RAMP_MAX_SECONDS && budget > 0U &&
           budget <= FLOW_RAMP_MAX_SECONDS && budget >= hold + stable;
}

int FlowRamp_Validate(const FLOW_RAMP_CONFIG_T *c)
{
    return c->rampVersion == FLOW_RAMP_VERSION &&
        isfinite(c->rampCenter) && isfinite(c->rampSpan) &&
        isfinite(c->rampTolerance) && isfinite(c->rampValveMin) &&
        isfinite(c->rampValveMax) && c->rampSpan >= 0.0f &&
        c->rampCenter - c->rampSpan >= 0.01f &&
        c->rampCenter + c->rampSpan <= 30.0f &&
        c->rampTolerance > 0.0f && c->rampTolerance <= 30.0f &&
        c->rampSeconds > 0U && c->rampSeconds <= FLOW_RAMP_MAX_SECONDS &&
        c->rampStableSeconds <= FLOW_RAMP_MAX_SECONDS &&
        c->rampLimitSeconds > 0U && c->rampLimitSeconds <= FLOW_RAMP_MAX_SECONDS &&
        c->rampValveMin >= 0.0f && c->rampValveMax <= 100.0f &&
        c->rampValveMin < c->rampValveMax &&
        validPoint(c->rampHoldLow, c->rampStableSeconds, c->rampMaxLow) &&
        validPoint(c->rampHoldMid, c->rampStableSeconds, c->rampMaxMid) &&
        validPoint(c->rampHoldHigh, c->rampStableSeconds, c->rampMaxHigh);
}

static void enterState(FlowRamp *r, uint16_t state)
{
    r->state = state;
    r->elapsedMs = r->holdMs = r->stableMs = 0U;
    r->previousInBand = r->previousQualified = 0U;
}

static void fault(FlowRamp *r, uint16_t reason)
{
    r->fault = reason;
    r->state = RAMP_FAULT;
}

void FlowRamp_Start(FlowRamp *r, const FLOW_RAMP_CONFIG_T *c)
{
    memset(r, 0, sizeof(*r));
    r->config = *c;
    if (!FlowRamp_Validate(c)) {
        fault(r, RAMP_BAD_CONFIG);
        return;
    }
    r->target = c->rampCenter - c->rampSpan;
    enterState(r, RAMP_HOLD_LOW);
}

void FlowRamp_Update(FlowRamp *r, uint32_t dtMs, float measured,
                     int sampleValid, float valveOpening)
{
    const FLOW_RAMP_CONFIG_T *c = &r->config;
    uint32_t hold, budget, segmentMs;
    uint16_t next;
    uint8_t kind = 0U;
    int inBand, qualified;
    float from, to, error;

    if (r->state == RAMP_IDLE || r->state == RAMP_FAULT) return;
    sampleValid = sampleValid && isfinite(measured);
    r->elapsedMs = addMs(r->elapsedMs, dtMs);
    segmentMs = c->rampSeconds * 500U; /* Supports odd second totals. */

    switch (r->state) {
    case RAMP_UP_LOW_MID: from = c->rampCenter - c->rampSpan; to = c->rampCenter; break;
    case RAMP_UP_MID_HIGH: from = c->rampCenter; to = c->rampCenter + c->rampSpan; break;
    case RAMP_DOWN_HIGH_MID: from = c->rampCenter + c->rampSpan; to = c->rampCenter; break;
    case RAMP_DOWN_MID_LOW: from = c->rampCenter; to = c->rampCenter - c->rampSpan; break;
    default: from = to = r->target; break;
    }
    if (r->state == RAMP_UP_LOW_MID || r->state == RAMP_UP_MID_HIGH ||
        r->state == RAMP_DOWN_HIGH_MID || r->state == RAMP_DOWN_MID_LOW) {
        /* Recompute from absolute segment elapsed time: no tiny-step accumulation. */
        if (r->elapsedMs >= segmentMs) {
            r->target = to;
            next = r->state == RAMP_DOWN_MID_LOW ? RAMP_HOLD_LOW : r->state + 1U;
            if (next == RAMP_HOLD_LOW && r->cycles != UINT32_MAX) r->cycles++;
            enterState(r, next);
            r->previousInBand = sampleValid &&
                fabsf(measured - r->target) <= c->rampTolerance;
            r->previousQualified = r->previousInBand && c->rampStableSeconds == 0U;
        } else {
            r->target = from + (to - from) * ((float)r->elapsedMs / (float)segmentMs);
        }
    } else {
        switch (r->state) {
        case RAMP_HOLD_LOW: hold = c->rampHoldLow; budget = c->rampMaxLow; next = RAMP_UP_LOW_MID; break;
        case RAMP_HOLD_HIGH: hold = c->rampHoldHigh; budget = c->rampMaxHigh; next = RAMP_DOWN_HIGH_MID; break;
        default:
            hold = c->rampHoldMid; budget = c->rampMaxMid; next = r->state + 1U; break;
        }
        inBand = sampleValid && fabsf(measured - r->target) <= c->rampTolerance;
        qualified = 0;
        if (inBand) {
            if (r->previousInBand) r->stableMs = addMs(r->stableMs, dtMs);
            qualified = r->stableMs >= c->rampStableSeconds * 1000U;
            if (qualified && r->previousQualified) r->holdMs = addMs(r->holdMs, dtMs);
        } else {
            r->stableMs = 0U;
        }
        r->previousInBand = (uint8_t)inBand;
        r->previousQualified = (uint8_t)qualified;
        /* Completion at the exact deadline is valid; late completion is not. */
        if (qualified && r->holdMs >= hold * 1000U && r->elapsedMs <= budget * 1000U) {
            enterState(r, next);
        } else if (r->elapsedMs >= budget * 1000U) {
            fault(r, RAMP_POINT_TIMEOUT);
            return;
        }
    }

    /* Saturation detection also runs during ramps; invalid samples never count. */
    error = r->target - measured;
    if (sampleValid && isfinite(valveOpening)) {
        if (error > c->rampTolerance && valveOpening >= c->rampValveMax - 0.01f)
            kind = RAMP_INSUFFICIENT_FLOW;
        else if (error < -c->rampTolerance && valveOpening <= c->rampValveMin + 0.01f)
            kind = RAMP_BELOW_RANGE;
    }
    if (kind != 0U) {
        r->limitMs = kind == r->limitKind ? addMs(r->limitMs, dtMs) : 0U;
        if (r->limitMs >= c->rampLimitSeconds * 1000U) fault(r, kind);
    } else {
        r->limitMs = 0U;
    }
    r->limitKind = kind;
}
