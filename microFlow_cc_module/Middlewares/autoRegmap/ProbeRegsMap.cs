using EasyModbus;

namespace ItCv
{
public class SYSTEM_STATUS_T
{
public ProbeReg runStatus;
public ProbeReg commStatus;
public ProbeReg calibStatus;
public ProbeReg configStatus;
public ProbeReg productNum;
public ProbeReg deviceName;
public ProbeReg serial;
public ProbeReg hardwareVer;
public ProbeReg softwareVer;
public ProbeReg newStructFlg;
public ProbeReg measureTarget;
}
public class COMM_SETTINGS_T
{
public ProbeReg modbusAddr;
public ProbeReg modbusDatabits;
public ProbeReg modbusParity;
public ProbeReg modbusBaud;
}
public class MEASURE_SETTINGS_T
{
public ProbeReg sampleCycle;
public ProbeReg measureRange;
public ProbeReg measureRange2;
public ProbeReg command;
}
public class CALIB_SETTINGS_T
{
}
public class FILTER_SETTINGS_T
{
}
public class MEASURE_VALUES_T
{
public ProbeReg tecPowerNow;
public ProbeReg temperature1;
public ProbeReg temperature2;
}
public class RSVD_PARAM_T
{
public ProbeReg flowRateVoltageAve;
public ProbeReg flowRateAve;
public ProbeReg filterFactor;
public ProbeReg flowRateSet;
public ProbeReg valveOpening;
public ProbeReg manualMode;
public ProbeReg modbusCmd2;
public ProbeReg lowFlow;
public ProbeReg highFlow;
public ProbeReg lowFlowVoltage;
public ProbeReg highFlowVoltage;
public ProbeReg PID_P;
public ProbeReg PID_I;
public ProbeReg PID_D;
}
    public class ProbeRegsMap
    {
public SYSTEM_STATUS_T system_status = new SYSTEM_STATUS_T();
public COMM_SETTINGS_T comm_settings = new COMM_SETTINGS_T();
public MEASURE_SETTINGS_T measure_settings = new MEASURE_SETTINGS_T();
public CALIB_SETTINGS_T calib_settings = new CALIB_SETTINGS_T();
public FILTER_SETTINGS_T filter_settings = new FILTER_SETTINGS_T();
public MEASURE_VALUES_T measure_values = new MEASURE_VALUES_T();
public RSVD_PARAM_T rsvd_param = new RSVD_PARAM_T();
        public ProbeRegsMap(ModbusClient mb)
        {
system_status .runStatus = new ProbeReg("runStatus", "uint16_t",41000, mb);
system_status .commStatus = new ProbeReg("commStatus", "uint16_t",41001, mb);
system_status .calibStatus = new ProbeReg("calibStatus", "uint16_t",41002, mb);
system_status .configStatus = new ProbeReg("configStatus", "uint16_t",41003, mb);
system_status .productNum = new ProbeReg("productNum", "uint32_t",41004, mb);
system_status .deviceName = new ProbeReg("deviceName", "char[16]",41006, mb);
system_status .serial = new ProbeReg("serial", "char[16]",41014, mb);
system_status .hardwareVer = new ProbeReg("hardwareVer", "char[16]",41022, mb);
system_status .softwareVer = new ProbeReg("softwareVer", "char[16]",41030, mb);
system_status .newStructFlg = new ProbeReg("newStructFlg", "uint16_t",41038, mb);
system_status .measureTarget = new ProbeReg("measureTarget", "uint16_t",41039, mb);
comm_settings .modbusAddr = new ProbeReg("modbusAddr", "uint16_t",42000, mb);
comm_settings .modbusDatabits = new ProbeReg("modbusDatabits", "uint16_t",42001, mb);
comm_settings .modbusParity = new ProbeReg("modbusParity", "uint16_t",42002, mb);
comm_settings .modbusBaud = new ProbeReg("modbusBaud", "uint32_t",42003, mb);
measure_settings .sampleCycle = new ProbeReg("sampleCycle", "uint16_t",43000, mb);
measure_settings .measureRange = new ProbeReg("measureRange", "float",43001, mb);
measure_settings .measureRange2 = new ProbeReg("measureRange2", "float",43003, mb);
measure_settings .command = new ProbeReg("command", "uint16_t",43005, mb);
measure_values .tecPowerNow = new ProbeReg("tecPowerNow", "float",46000, mb);
measure_values .temperature1 = new ProbeReg("temperature1", "float",46002, mb);
measure_values .temperature2 = new ProbeReg("temperature2", "float",46004, mb);
rsvd_param .flowRateVoltageAve = new ProbeReg("flowRateVoltageAve", "float",48000, mb);
rsvd_param .flowRateAve = new ProbeReg("flowRateAve", "float",48002, mb);
rsvd_param .filterFactor = new ProbeReg("filterFactor", "float",48004, mb);
rsvd_param .flowRateSet = new ProbeReg("flowRateSet", "float",48006, mb);
rsvd_param .valveOpening = new ProbeReg("valveOpening", "float",48008, mb);
rsvd_param .manualMode = new ProbeReg("manualMode", "uint16_t",48010, mb);
rsvd_param .modbusCmd2 = new ProbeReg("modbusCmd2", "uint16_t",48011, mb);
rsvd_param .lowFlow = new ProbeReg("lowFlow", "float",48012, mb);
rsvd_param .highFlow = new ProbeReg("highFlow", "float",48014, mb);
rsvd_param .lowFlowVoltage = new ProbeReg("lowFlowVoltage", "float",48016, mb);
rsvd_param .highFlowVoltage = new ProbeReg("highFlowVoltage", "float",48018, mb);
rsvd_param .PID_P = new ProbeReg("PID_P", "float",48020, mb);
rsvd_param .PID_I = new ProbeReg("PID_I", "float",48022, mb);
rsvd_param .PID_D = new ProbeReg("PID_D", "float",48024, mb);
        }
    }
}
