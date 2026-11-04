#include "modbus.h"
#include "portrw.h"
#include <assert.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>

SYS_STATUS_T system_status;
COMM_SETTINGS_T comm_settings;
MEASURE_SETTINGS_T measure_settings;
CALIB_SETTINGS_T calib_settings;
FILTER_SETTINGS_T filter_settings;
MEASURE_VALUES_T measure_values;
RSVD_PARAM_T rsvd_param;
volatile uint8_t autoSavePending;
eMBErrorCode eMBRegHoldingCB(UCHAR *, USHORT, USHORT, eMBRegisterMode);

int main(void)
{
    UCHAR buf[208] = {0};
    assert(sizeof(SYS_STATUS_T)==104 && sizeof(COMM_SETTINGS_T)==104);
    assert(sizeof(MEASURE_SETTINGS_T)==104 && sizeof(CALIB_SETTINGS_T)==104);
    assert(sizeof(FILTER_SETTINGS_T)==104 && sizeof(RSVD_PARAM_T)==208);
    assert(sizeof(MEASURE_VALUES_T)==104);
    assert(offsetof(RSVD_PARAM_T,rampCenter)==64);
    assert(offsetof(RSVD_PARAM_T,rampSeconds)==76);
    assert(offsetof(RSVD_PARAM_T,rampValveMax)==116);
    assert(offsetof(RSVD_PARAM_T,rampState)==164);
    assert(offsetof(RSVD_PARAM_T,rampCycles)==188);
    assert(offsetof(RSVD_PARAM_T,manualMode)==20);
    assert(offsetof(RSVD_PARAM_T,flowScaleModel)==52);
    assert(offsetof(RSVD_PARAM_T,flowFullScale)==56);

    assert(eMBRegHoldingCB(buf,44000,1,MB_REG_READ)==MB_ENOREG);
    assert(eMBRegHoldingCB(buf,44001,0,MB_REG_READ)==MB_ENOREG);
    assert(eMBRegHoldingCB(buf,44052,2,MB_REG_READ)==MB_ENOREG);
    assert(eMBRegHoldingCB(buf,44052,1,MB_REG_READ)==MB_ENOERR);
    assert(eMBRegHoldingCB(buf,42001,7000,MB_REG_WRITE)==MB_ENOREG);
    assert(!autoSavePending);
    assert(eMBRegHoldingCB(buf,48028,1,MB_REG_WRITE)==MB_ENOREG);
    assert(eMBRegHoldingCB(buf,48000,1,MB_REG_READ)==MB_ENOREG);
    assert(eMBRegHoldingCB(buf,48001,0,MB_REG_READ)==MB_ENOREG);
    assert(eMBRegHoldingCB(buf,48104,2,MB_REG_READ)==MB_ENOREG);
    assert(eMBRegHoldingCB(buf,48104,1,MB_REG_READ)==MB_ENOERR);
    assert(eMBRegHoldingCB(buf,48031,1,MB_REG_WRITE)==MB_ENOREG);
    assert(eMBRegHoldingCB(buf,48032,2,MB_REG_WRITE)==MB_ENOREG);
    assert(eMBRegHoldingCB(buf,48082,2,MB_REG_WRITE)==MB_ENOREG);
    assert(eMBRegHoldingCB(buf,48083,14,MB_REG_WRITE)==MB_ENOREG);
    buf[0]=0; buf[1]=2;
    assert(eMBRegHoldingCB(buf,48027,1,MB_REG_WRITE)==MB_EINVAL);
    buf[1]=1;
    assert(eMBRegHoldingCB(buf,48027,1,MB_REG_WRITE)==MB_ENOERR);
    assert(rsvd_param.flowScaleModel==1);

    /* uint32_t 3600, low word first (CDAB). */
    buf[0]=0x0e; buf[1]=0x10; buf[2]=0; buf[3]=0;
    assert(eMBRegHoldingCB(buf,48039,2,MB_REG_WRITE)==MB_ENOERR);
    assert(rsvd_param.rampSeconds==3600 && autoSavePending);
    memset(buf,0,sizeof(buf));
    assert(eMBRegHoldingCB(buf,48039,2,MB_REG_READ)==MB_ENOERR);
    assert(buf[0]==0x0e && buf[1]==0x10 && buf[2]==0 && buf[3]==0);
    autoSavePending=0; buf[0]=0; buf[1]=3;
    assert(eMBRegHoldingCB(buf,48011,1,MB_REG_WRITE)==MB_EINVAL);
    assert(!autoSavePending && rsvd_param.manualMode==0);
    buf[1]=2;
    assert(eMBRegHoldingCB(buf,48011,1,MB_REG_WRITE)==MB_ENOERR);
    assert(rsvd_param.manualMode==2);
    memset(buf,0,sizeof(buf)); buf[5]=3;
    assert(eMBRegHoldingCB(buf,48009,3,MB_REG_WRITE)==MB_EINVAL);
    assert(rsvd_param.manualMode==2 && rsvd_param.valveOpening==0);
    rsvd_param.rampState=RAMP_HOLD_HIGH;
    rsvd_param.rampTarget=6.0f;
    assert(eMBRegHoldingCB(buf,48083,14,MB_REG_READ)==MB_ENOERR);
    assert(buf[0]==0 && buf[1]==RAMP_HOLD_HIGH);
    assert(buf[4]==0 && buf[5]==0 && buf[6]==0x40 && buf[7]==0xc0);
    autoSavePending=0;
    assert(eMBRegHoldingCB(buf,48001,104,MB_REG_READ)==MB_ENOERR);
    assert(buf[164]==0 && buf[165]==RAMP_HOLD_HIGH);
    assert(!autoSavePending); /* Polling never requests a Flash erase. */
    {
        unsigned i;
        /* Can write all 28 editable configuration words in one frame. */
        for(i=0;i<56;i++) buf[i]=(UCHAR)(i+1);
        assert(eMBRegHoldingCB(buf,48033,28,MB_REG_WRITE)==MB_ENOERR);
        assert(!memcmp(&rsvd_param.rampCenter, (UCHAR[]){2,1,4,3},4));
        assert(eMBRegHoldingCB(buf,48033,28,MB_REG_READ)==MB_ENOERR);
        for(i=0;i<56;i++) assert(buf[i]==i+1);
    }
    puts("Modbus ramp: layout, CDAB, permissions and mode validation passed");
    return 0;
}
