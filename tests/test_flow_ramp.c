#include "flow_ramp.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>

static FLOW_RAMP_CONFIG_T c;
static FlowRamp r;

static void reset(void)
{
    FlowRamp_Defaults(&c);
    c.rampSeconds = 4;
    c.rampHoldLow = c.rampHoldMid = c.rampHoldHigh = 2;
    c.rampStableSeconds = 1;
    c.rampMaxLow = c.rampMaxMid = c.rampMaxHigh = 10;
    FlowRamp_Start(&r, &c);
}
static void step(uint32_t ms, float pv)
{
    FlowRamp_Update(&r, ms, pv, 1, 50.0f);
}
static void finishHold(uint16_t next)
{
    step(0, r.target);
    step(1000, r.target); /* Stable confirmation only; no hold credit. */
    step(2000, r.target);
    assert(r.state == next && r.fault == RAMP_OK);
}

int main(void)
{
    assert(sizeof(FLOW_RAMP_CONFIG_T) == 104);
    reset();
    assert(r.state == RAMP_HOLD_LOW && r.target == 4.0f);
    finishHold(RAMP_UP_LOW_MID);
    step(1000, 4.0f); assert(fabsf(r.target - 4.5f) < 0.0001f);
    step(1000, 5.0f); assert(r.state == RAMP_HOLD_MID_UP && r.target == 5.0f);
    finishHold(RAMP_UP_MID_HIGH);
    step(2000, 6.0f); assert(r.state == RAMP_HOLD_HIGH && r.target == 6.0f);
    finishHold(RAMP_DOWN_HIGH_MID);
    step(1000, 6.0f); assert(fabsf(r.target - 5.5f) < 0.0001f);
    step(1000, 5.0f); assert(r.state == RAMP_HOLD_MID_DOWN);
    finishHold(RAMP_DOWN_MID_LOW);
    step(2000, 4.0f); assert(r.state == RAMP_HOLD_LOW && r.cycles == 1);
    finishHold(RAMP_UP_LOW_MID); /* Endpoint was held once, not twice. */

    reset(); step(0,4); step(1000,4); step(1000,4);
    assert(r.holdMs == 1000);
    step(1000,8); assert(r.holdMs == 1000 && r.stableMs == 0);
    step(0,4); step(1000,4); assert(r.holdMs == 1000);
    step(1000,4); assert(r.state == RAMP_UP_LOW_MID);

    reset(); step(10000,8);
    assert(r.fault == RAMP_POINT_TIMEOUT && r.state == RAMP_FAULT);
    step(1000,4); assert(r.fault == RAMP_POINT_TIMEOUT);
    reset(); FlowRamp_Update(&r,10000,NAN,0,50);
    assert(r.fault == RAMP_POINT_TIMEOUT); /* Sensor failure cannot stall timeout. */
    reset(); step(0,4); step(1000,4); step(9001,4);
    assert(r.fault == RAMP_POINT_TIMEOUT); /* No late completion. */

    reset(); c.rampLimitSeconds=1; FlowRamp_Start(&r,&c);
    FlowRamp_Update(&r,0,0,1,c.rampValveMax);
    FlowRamp_Update(&r,1000,0,1,c.rampValveMax);
    assert(r.fault == RAMP_INSUFFICIENT_FLOW);
    reset(); c.rampLimitSeconds=1; FlowRamp_Start(&r,&c);
    FlowRamp_Update(&r,0,8,1,c.rampValveMin);
    FlowRamp_Update(&r,1000,8,1,c.rampValveMin);
    assert(r.fault == RAMP_BELOW_RANGE);
    reset(); c.rampLimitSeconds=2; FlowRamp_Start(&r,&c);
    FlowRamp_Update(&r,0,0,1,c.rampValveMax);
    FlowRamp_Update(&r,1000,0,1,c.rampValveMax);
    FlowRamp_Update(&r,1000,4,1,c.rampValveMax);
    assert(r.limitMs==0 && r.fault==RAMP_OK);

    reset(); c.rampSeconds=604800; c.rampHoldLow=0; c.rampStableSeconds=0;
    FlowRamp_Start(&r,&c); step(0,4); assert(r.state==RAMP_UP_LOW_MID);
    step(151200000,4); assert(fabsf(r.target-4.5f)<0.0001f);
    step(151200000,5); assert(r.target==5 && r.state==RAMP_HOLD_MID_UP);
    reset(); c.rampSeconds=3; c.rampHoldLow=0; c.rampStableSeconds=0;
    FlowRamp_Start(&r,&c); step(0,4); step(750,4);
    assert(fabsf(r.target-4.5f)<0.0001f);

    reset(); c.rampSpan=0; c.rampHoldLow=0; c.rampStableSeconds=0;
    FlowRamp_Start(&r,&c); step(0,5); step(2000,5);
    assert(r.state==RAMP_HOLD_MID_UP && r.target==5);
    reset(); c.rampCenter=NAN; assert(!FlowRamp_Validate(&c));
    FlowRamp_Start(&r,&c); assert(r.fault==RAMP_BAD_CONFIG);
    reset(); c.rampSpan=10; assert(!FlowRamp_Validate(&c));
    reset(); c.rampValveMin=c.rampValveMax; assert(!FlowRamp_Validate(&c));
    reset(); c.rampSeconds=0; assert(!FlowRamp_Validate(&c));
    reset(); c.rampHoldLow=10; assert(!FlowRamp_Validate(&c));
    reset(); c.rampHoldLow=UINT32_MAX; assert(!FlowRamp_Validate(&c));
    reset(); c.rampValveMax=100; c.rampValveMin=30; assert(FlowRamp_Validate(&c));
    reset(); c.rampMaxLow=3; c.rampMaxMid=3; FlowRamp_Start(&r,&c);
    step(0,4); step(1000,4); step(2000,4);
    assert(r.state==RAMP_UP_LOW_MID); /* Completion at exact deadline. */
    step(2000,5); assert(r.state==RAMP_HOLD_MID_UP);
    step(1000,5); step(2000,5);
    assert(r.state==RAMP_UP_MID_HIGH); /* Arrival observation starts settling. */
    reset(); c.rampCenter=8; /* Active config is a snapshot, unchanged by edits. */
    assert(r.config.rampCenter==5);
    reset(); c.rampHoldLow=604799; c.rampMaxLow=604800;
    FlowRamp_Start(&r,&c); step(0,4); step(1000,4); step(604799000,4);
    assert(r.state==RAMP_UP_LOW_MID); /* Week-long effective hold. */
    { uint32_t previous=UINT32_MAX-49U, now=50U;
      assert(now-previous==100U); } /* Same tick subtraction used by control task. */
    puts("flow ramp: all scenarios passed");
    return 0;
}
