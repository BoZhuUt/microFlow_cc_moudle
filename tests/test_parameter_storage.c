#include "parameter_storage.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

int main(void)
{
    RSVD_PARAM_T p, saved, loaded, factory;
    FLOW_RAMP_CONFIG_T ramp, snapshot;
    unsigned char legacy[104];
    memset(&p,0,sizeof(p));
    p.lowFlowVoltage=.399f;
    p.highFlowVoltage=.540f;
    p.highFlow=8.57f;
    p.flowScaleModel=1;
    p.flowScaleVersion=0x5301;
    p.flowFullScale=30;
    p.PID_P=7;
    p.manualMode=2;
    memset(legacy,0xff,sizeof(legacy));
    memcpy(legacy,&p,60);
    FlowRamp_Defaults(&ramp);
    ramp.rampCenter=8;
    ramp.rampSeconds=7200;
    ramp.rampHoldMid=3600;
    ramp.rampMaxMid=5400;
    RsvdParam_Decode(&loaded,legacy,0,&ramp);
    assert(!memcmp(&loaded,&p,60));
    assert(loaded.rampCenter==8 && loaded.rampSeconds==7200);
    assert(loaded.rampHoldMid==3600 && loaded.rampMaxMid==5400);
    assert(loaded.rampState==0 && loaded.rampFault==0);
    RsvdParam_GetRamp(&loaded,&snapshot);
    assert(!memcmp(&snapshot,&ramp,sizeof(ramp)));
    /* Factory image is decoded independently and keeps its own settings. */
    ramp.rampCenter=3;
    RsvdParam_Decode(&factory,legacy,0,&ramp);
    assert(factory.rampCenter==3 && loaded.rampCenter==8);
    loaded.rampState=RAMP_HOLD_HIGH;
    loaded.rampHoldMs=123456;
    loaded.rampCycles=42;
    loaded.rampFault=RAMP_POINT_TIMEOUT;
    loaded.reserved[7]=0x1234; /* Last word of the expanded 104-register group. */
    RsvdParam_PrepareFlash(&saved,&loaded);
    assert(loaded.rampState==RAMP_HOLD_HIGH); /* Save must not alter live state. */
    assert(saved.rampState==0 && saved.rampHoldMs==0 && saved.rampFault==0);
    assert(saved.rampCycles==0 && saved.reserved[7]==0x1234);
    RsvdParam_Decode(&p,&saved,1,NULL);
    assert(!memcmp(&p,&saved,sizeof(p)));
    assert(p.rampCenter==8 && fabsf(p.highFlowVoltage-.540f)<.000001f && p.PID_P==7);
    /* New-format images always clear stale runtime state when loaded. */
    saved.rampState=RAMP_FAULT;
    RsvdParam_Decode(&p,&saved,1,NULL);
    assert(p.rampState==0);
    memset(&ramp,0xff,sizeof(ramp));
    RsvdParam_Decode(&p,legacy,0,&ramp);
    assert(p.rampVersion==FLOW_RAMP_VERSION && p.rampSeconds==3600);
    assert(!memcmp(&p,legacy,60)); /* Invalid old ramp cannot erase calibration. */
    puts("parameter storage: legacy user/factory migration, full-size roundtrip, runtime exclusion passed");
    return 0;
}
