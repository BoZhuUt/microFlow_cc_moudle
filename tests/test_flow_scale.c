#include "flow_scale.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>

static float thermal(float v, float ref, float v0, float vr, float full)
{
    float q = -999;
    assert(FlowScale_Convert(1,v,0,ref,v0,vr,full,&q));
    return q;
}
int main(void)
{
    const float qExample[] = {0,.1f,1.05f,2.39f,3.06f,4.24f,5.52f,7.06f,9,10};
    const float vExample[] = {.51f,1.05f,2.35f,2.89f,3.12f,3.38f,3.61f,3.85f,4.32f,4.52f};
    float result, previous = 0;
    unsigned i;
    assert(FlowScale_Convert(0,1.25f,0,30,.25f,2.25f,30,&result));
    assert(fabsf(result-15)<.0001f);
    assert(FlowScale_Convert(0,.399f,1.6f,8.57f,.433f,.540f,30,&result));
    assert(fabsf(result-(-.614766f))<.0001f); /* Linear mode is preserved. */
    /* Compress the example's voltages into ADC range, preserve its shape. */
    for (i=0;i<10;i++) {
        float v=.4f+(vExample[i]-.51f)*.3f;
        assert(fabsf(thermal(v,30,.4f,1.603f,30)-qExample[i]*3)<.0001f);
    }
    assert(thermal(.4f,10,.4f,.8f,30)==0);
    assert(thermal(.39f,10,.4f,.8f,30)==0);
    assert(fabsf(thermal(.8f,10,.4f,.8f,30)-10)<.0001f);
    /* Arbitrary interior reference points must all be reproduced. */
    for (i=1;i<=30;i++)
        assert(fabsf(thermal(.8f,(float)i,.4f,.8f,30)-(float)i)<.0001f);
    for (i=0;i<=10000;i++) {
        result=thermal(.3f+(float)i*.0001f,10,.4f,.8f,30);
        assert(result>=previous);
        previous=result;
    }
    assert(thermal(1.5f,10,.4f,.8f,30)>30); /* No silent top clipping. */
    result=123;
    assert(!FlowScale_Convert(1,.5f,1,10,.4f,.8f,30,&result));
    assert(!FlowScale_Convert(1,.5f,0,0,.4f,.8f,30,&result));
    assert(!FlowScale_Convert(1,.5f,0,31,.4f,.8f,30,&result));
    assert(!FlowScale_Convert(1,.5f,0,10,.8f,.4f,30,&result));
    assert(!FlowScale_Convert(1,.5f,0,10,.4f,.4001f,30,&result));
    assert(!FlowScale_Convert(0,.5f,0,10,.4f,.4f,30,&result));
    assert(!FlowScale_Convert(1,NAN,0,10,.4f,.8f,30,&result));
    assert(!FlowScale_Convert(1,.5f,0,10,.4f,.8f,NAN,&result));
    assert(!FlowScale_Convert(2,.5f,0,10,.4f,.8f,30,&result));
    assert(result==123);
    assert(FlowScale_ValidateCalibration(1,0,10,1.2f,2,30));
    assert(FlowScale_ValidateCalibration(1,0,30,0,3.3f,30));
    assert(FlowScale_ValidateCalibration(0,30,0,3.3f,0,30));
    assert(!FlowScale_ValidateCalibration(0,5,5,0,3,30));
    assert(!FlowScale_ValidateCalibration(0,0,30,-.01f,3,30));
    assert(!FlowScale_ValidateCalibration(0,0,30,0,3.31f,30));
    assert(!FlowScale_ValidateCalibration(0,-1,30,0,3,30));
    assert(!FlowScale_ValidateCalibration(0,0,31,0,3,30));
    puts("flow scale: calibration anchors, example shape, monotonicity and invalid inputs passed");
    return 0;
}
