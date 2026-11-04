#include "modbus.h"
#include "pid.h"
#include "parameter_storage.h"
#include "task.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

#define FLOW_PID_OUT_MIN 20.0f
#define FLOW_PID_OUT_MAX 99.0f
#define FLOW_PID_DEADBAND .1f
#define FLOW_TASK_DT_S .1f
#define CHECK(condition) do { if (!(condition)) { \
    fprintf(stderr,"control check failed, line %d: %s\n",__LINE__,#condition); return 1; \
} } while (0)

RSVD_PARAM_T rsvd_param;
static PID_Controller_t s_flowPid;
static FlowRamp s_ramp;
static FLOW_RAMP_CONFIG_T s_rampConfig;
static uint16_t s_prevManualMode;
static uint32_t s_controlTick, tick;
static float s_lastDuty, applied;
uint32_t HAL_GetTick(void) { return tick; }
void Valve_SetDuty(float duty) { applied=duty; }
#include "flow_control_under_test.inc"

static void reset(uint16_t previousMode)
{
    memset(&rsvd_param,0,sizeof(rsvd_param));
    memset(&s_ramp,0,sizeof(s_ramp));
    RsvdParam_DefaultRamp(&rsvd_param);
    PID_Init(&s_flowPid,20,99,.1f);
    s_prevManualMode=previousMode;
    s_lastDuty=applied=80;
    tick=1000;
    s_controlTick=900;
    rsvd_param.manualMode=1;
    rsvd_param.valveOpening=0;
}

int main(void)
{
    const float invalid[] = {-1.0f, -.001f, NAN, INFINITY};
    unsigned i;
    for(i=0;i<sizeof(invalid)/sizeof(invalid[0]);i++) {
        reset(2); /* Manual takeover after a ramp fault must accept close. */
        s_ramp.state=RAMP_FAULT;
        s_ramp.fault=RAMP_INSUFFICIENT_FLOW;
        FlowControl_Run(invalid[i]);
        CHECK(applied==0 && s_lastDuty==0 && rsvd_param.valveOpening==0);
        CHECK(rsvd_param.rampFault==0 && rsvd_param.rampState==0);
        rsvd_param.valveOpening=25;
        FlowControl_Run(invalid[i]);
        CHECK(applied==25 && s_lastDuty==25); /* Already manual, sensor still failed. */
        reset(0);
        rsvd_param.manualMode=0;
        FlowControl_Run(invalid[i]);
        CHECK(applied==80); /* Automatic control still holds its last output. */
    }
    reset(1);
    rsvd_param.valveOpening=NAN;
    FlowControl_Run(-1);
    CHECK(applied==80 && isfinite(rsvd_param.valveOpening));
    rsvd_param.valveOpening=200;
    FlowControl_Run(-1);
    CHECK(applied==100);
    rsvd_param.valveOpening=-5;
    FlowControl_Run(-1);
    CHECK(applied==0);
    reset(0);
    rsvd_param.manualMode=2;
    FlowControl_Run(-1);
    CHECK(applied==80 && rsvd_param.rampState==RAMP_HOLD_LOW);
    tick+=rsvd_param.rampMaxLow*1000U;
    FlowControl_Run(-1);
    CHECK(rsvd_param.rampFault==RAMP_POINT_TIMEOUT && applied==80);
    puts("flow control: manual takeover with invalid samples, output guards and automatic hold passed");
    return 0;
}
