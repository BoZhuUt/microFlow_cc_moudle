/* Run the actual Flash store/load/factory implementation against an emulated page. */
#include "flash.h"
#include "modbus.h"
#include "parameter.h"
#include "parameter_storage.h"
#include "flow_scale.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#ifdef _WIN32
#include <windows.h>
#else
#include <sys/mman.h>
#endif

SYS_STATUS_T system_status;
COMM_SETTINGS_T comm_settings;
MEASURE_SETTINGS_T measure_settings;
CALIB_SETTINGS_T calib_settings;
FILTER_SETTINGS_T filter_settings;
MEASURE_VALUES_T measure_values;
RSVD_PARAM_T rsvd_param;
extern RSVD_PARAM_T rsvd_paramf;

void HAL_FLASH_Unlock(void) {}
void HAL_FLASH_Lock(void) {}
HAL_StatusTypeDef HAL_FLASHEx_Erase(FLASH_EraseInitTypeDef *erase, uint32_t *error)
{
    assert(erase->Page==63 && erase->NbPages==1);
    memset((void *)REG_STORAGE_ADDR,0xff,PAGE_SIZE);
    *error=0;
    return HAL_OK;
}
HAL_StatusTypeDef HAL_FLASH_Program(uint32_t type, uint32_t address, uint64_t value)
{
    uint64_t existing;
    assert(type==FLASH_TYPEPROGRAM_DOUBLEWORD && address%8==0);
    assert(address>=REG_STORAGE_ADDR && address+8<=REG_STORAGE_ADDR+PAGE_SIZE);
    memcpy(&existing,(void *)(uintptr_t)address,8);
    assert(existing==UINT64_MAX); /* Detect any overlapping Flash slots. */
    memcpy((void *)(uintptr_t)address,&value,8);
    return HAL_OK;
}
static void put(uint32_t address,const void *data,unsigned bytes)
{
    memcpy((void *)(uintptr_t)address,data,bytes);
}
static void legacyPage(void)
{
    FLOW_RAMP_CONFIG_T config;
    unsigned char legacyRsvd[104]={0};
    ParametarsInitByDefaultValue();
    strcpy(system_status.softwareVer,"1.1.2");
    rsvd_param.lowFlowVoltage=.399f;
    rsvd_param.highFlowVoltage=.540f;
    rsvd_param.highFlow=8.57f;
    rsvd_param.flowScaleModel=1;
    rsvd_param.PID_P=7;
    memcpy(legacyRsvd,&rsvd_param,60);
    RsvdParam_GetRamp(&rsvd_param,&config);
    config.rampCenter=8;
    config.rampSeconds=7200;
    config.rampValveMin=35; /* Previously saved default must upgrade once. */
    memset((void *)REG_STORAGE_ADDR,0xff,PAGE_SIZE);
    put(SYS_STATUS_SADDR,&system_status,104);
    put(COM_SET_SADDR,&comm_settings,104);
    put(MEASURE_SET_SADDR,&measure_settings,104);
    put(CAL_SET_SADDR,&config,104);
    put(FILTER_SET_SADDR,&filter_settings,104);
    put(RSVD_PARA_LEGACY_SADDR,legacyRsvd,104);
    put(SYS_STATUS_SADDR_F,&system_status,104);
    put(COM_SET_SADDR_F,&comm_settings,104);
    put(MEASURE_SET_SADDR_F,&measure_settings,104);
    config.rampCenter=3;
    config.rampValveMin=25; /* An independently customized factory limit survives. */
    put(CAL_SET_SADDR_F,&config,104);
    put(FILTER_SET_SADDR_F,&filter_settings,104);
    put(RSVD_PARA_LEGACY_SADDR_F,legacyRsvd,104);
}
int main(void)
{
    void *mapping;
    uint64_t tag;
#ifdef _WIN32
    mapping=VirtualAlloc((void *)0x08010000,65536,MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE);
#else
    mapping=mmap((void *)0x08010000,65536,PROT_READ|PROT_WRITE,
                 MAP_PRIVATE|MAP_ANONYMOUS|MAP_FIXED,-1,0);
#endif
    assert(mapping==(void *)0x08010000);
    memset((void *)REG_STORAGE_ADDR,0xff,PAGE_SIZE);
    PowerOn_ReadModbusReg(); /* Fresh device defaults and first save. */
    assert(rsvd_param.rampVersion==FLOW_RAMP_VERSION && rsvd_param.rampSeconds==3600);
    assert(rsvd_param.rampValveMin==20);
    rsvd_param.rampCenter=8;
    rsvd_param.rampSeconds=7200;
    rsvd_param.rampState=RAMP_HOLD_HIGH;
    rsvd_param.rampHoldMs=123456;
    rsvd_param.reserved[7]=0xbeef; /* Final register 48104 must be saved. */
    rsvd_param.manualMode=2;
    assert(StoreModbusRegs()==HAL_OK);
    assert(rsvd_param.rampState==RAMP_HOLD_HIGH); /* Saving cannot stop a live run. */
    memset(&rsvd_param,0,sizeof(rsvd_param));
    PowerOn_ReadModbusReg();
    assert(rsvd_param.rampCenter==8 && rsvd_param.rampSeconds==7200);
    assert(rsvd_param.reserved[7]==0xbeef && rsvd_param.rampState==0);
    assert(rsvd_param.rampHoldMs==0 && rsvd_param.manualMode==2);
    {
        FLOW_RAMP_CONFIG_T config;
        FlowRamp run;
        unsigned mode;
        RsvdParam_GetRamp(&rsvd_param,&config);
        FlowRamp_Start(&run,&config);
        assert(run.state==RAMP_HOLD_LOW && run.target==7); /* A=8, B=1. */
        assert(run.elapsedMs==0 && run.holdMs==0 && run.cycles==0);
        for(mode=0;mode<=2;mode++) {
            rsvd_param.manualMode=(uint16_t)mode;
            assert(StoreModbusRegs()==HAL_OK);
            memset(&rsvd_param,0,sizeof(rsvd_param));
            PowerOn_ReadModbusReg();
            assert(rsvd_param.manualMode==mode && rsvd_param.rampState==0);
        }
    }
    SaveRegsToFactory();
    rsvd_param.rampCenter=11;
    rsvd_param.reserved[7]=0;
    LoadRegsFromFactory();
    assert(rsvd_param.rampCenter==8 && rsvd_param.reserved[7]==0xbeef);

    legacyPage();
    PowerOn_ReadModbusReg(); /* Actual migration + new-format save. */
    assert(rsvd_param.rampCenter==8 && rsvd_param.rampSeconds==7200);
    assert(rsvd_paramf.rampCenter==3 && rsvd_param.flowScaleModel==1);
    assert(rsvd_param.rampValveMin==20 && rsvd_paramf.rampValveMin==25);
    assert(fabsf(rsvd_param.lowFlowVoltage-.399f)<.000001f);
    assert(fabsf(rsvd_param.highFlowVoltage-.540f)<.000001f);
    assert(fabsf(rsvd_param.highFlow-8.57f)<.000001f && rsvd_param.PID_P==7);
    memcpy(&tag,(void *)RSVD_LAYOUT_TAG_ADDR,8);
    assert(tag==RSVD_LAYOUT_TAG);
    memset(&rsvd_param,0,sizeof(rsvd_param));
    PowerOn_ReadModbusReg(); /* New-format reboot must not re-import group 44. */
    assert(rsvd_param.rampCenter==8 && rsvd_param.rampSeconds==7200);
    rsvd_param.rampValveMin=35;
    assert(StoreModbusRegs()==HAL_OK);
    PowerOn_ReadModbusReg();
    assert(rsvd_param.rampValveMin==35); /* A new deliberate setting is preserved. */
    LoadRegsFromFactory();
    assert(rsvd_param.rampCenter==3);
    {
        /* Runtime-valid tuples must survive the actual save/boot validation path. */
        const float profiles[][6] = {
            {1,0,10,1.2f,2.0f,30},
            {1,0,10,0,3.3f,30},
            {1,0,5,.4f,.8f,10},
            {0,10,30,1.2f,3.3f,30},
            {0,30,0,3.3f,0,30},
        };
        unsigned i;
        for(i=0;i<sizeof(profiles)/sizeof(profiles[0]);i++) {
            RSVD_PARAM_T expected;
            float before,after,voltage;
            ParametarsInitByDefaultValue();
            rsvd_param.flowScaleModel=(uint16_t)profiles[i][0];
            rsvd_param.lowFlow=profiles[i][1];
            rsvd_param.highFlow=profiles[i][2];
            rsvd_param.lowFlowVoltage=profiles[i][3];
            rsvd_param.highFlowVoltage=profiles[i][4];
            rsvd_param.flowFullScale=profiles[i][5];
            expected=rsvd_param;
            voltage=(rsvd_param.lowFlowVoltage+rsvd_param.highFlowVoltage)/2;
            assert(FlowScale_Convert(rsvd_param.flowScaleModel,voltage,
                rsvd_param.lowFlow,rsvd_param.highFlow,rsvd_param.lowFlowVoltage,
                rsvd_param.highFlowVoltage,rsvd_param.flowFullScale,&before));
            assert(StoreModbusRegs()==HAL_OK);
            memset(&rsvd_param,0,sizeof(rsvd_param));
            PowerOn_ReadModbusReg();
            if(memcmp(&expected.lowFlow,&rsvd_param.lowFlow,4*sizeof(float))) {
                fprintf(stderr,"calibration profile %u changed after reboot\n",i);
                return 1;
            }
            assert(rsvd_param.flowScaleModel==expected.flowScaleModel);
            assert(FlowScale_Convert(rsvd_param.flowScaleModel,voltage,
                rsvd_param.lowFlow,rsvd_param.highFlow,rsvd_param.lowFlowVoltage,
                rsvd_param.highFlowVoltage,rsvd_param.flowFullScale,&after));
            assert(fabsf(before-after)<.00001f);
        }
        /* Repair an invalid tuple coherently for the configured thermal range. */
        rsvd_param.flowScaleModel=1;
        rsvd_param.flowFullScale=10;
        rsvd_param.lowFlowVoltage=NAN;
        assert(StoreModbusRegs()==HAL_OK);
        PowerOn_ReadModbusReg();
        assert(rsvd_param.lowFlow==0 && rsvd_param.highFlow==10);
        assert(FlowScale_ValidateCalibration(rsvd_param.flowScaleModel,
            rsvd_param.lowFlow,rsvd_param.highFlow,rsvd_param.lowFlowVoltage,
            rsvd_param.highFlowVoltage,rsvd_param.flowFullScale));
    }
    puts("Flash: actual save/reboot/factory/legacy migration passed; no slot overlap");
    puts("Flash calibration: thermal/linear zero/reference endpoints and conversion preserved after reboot");
    return 0;
}
