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
public ProbeReg flowScaleModel;
public ProbeReg flowScaleVersion;
public ProbeReg flowFullScale;
public ProbeReg rampVersion;
public ProbeReg rampCenter;
public ProbeReg rampSpan;
public ProbeReg rampTolerance;
public ProbeReg rampSeconds;
public ProbeReg rampHoldLow;
public ProbeReg rampHoldMid;
public ProbeReg rampHoldHigh;
public ProbeReg rampStableSeconds;
public ProbeReg rampMaxLow;
public ProbeReg rampMaxMid;
public ProbeReg rampMaxHigh;
public ProbeReg rampLimitSeconds;
public ProbeReg rampValveMin;
public ProbeReg rampValveMax;
public ProbeReg rampState;
public ProbeReg rampFault;
public ProbeReg rampTarget;
public ProbeReg rampElapsedMs;
public ProbeReg rampHoldMs;
public ProbeReg rampStableMs;
public ProbeReg rampLimitMs;
public ProbeReg rampCycles;


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
rsvd_param .rampVersion = new ProbeReg("rampVersion", "uint16_t",48030, mb);
rsvd_param .rampCenter = new ProbeReg("rampCenter", "float",48032, mb);
rsvd_param .rampSpan = new ProbeReg("rampSpan", "float",48034, mb);
rsvd_param .rampTolerance = new ProbeReg("rampTolerance", "float",48036, mb);
rsvd_param .rampSeconds = new ProbeReg("rampSeconds", "uint32_t",48038, mb);
rsvd_param .rampHoldLow = new ProbeReg("rampHoldLow", "uint32_t",48040, mb);
rsvd_param .rampHoldMid = new ProbeReg("rampHoldMid", "uint32_t",48042, mb);
rsvd_param .rampHoldHigh = new ProbeReg("rampHoldHigh", "uint32_t",48044, mb);
rsvd_param .rampStableSeconds = new ProbeReg("rampStableSeconds", "uint32_t",48046, mb);
rsvd_param .rampMaxLow = new ProbeReg("rampMaxLow", "uint32_t",48048, mb);
rsvd_param .rampMaxMid = new ProbeReg("rampMaxMid", "uint32_t",48050, mb);
rsvd_param .rampMaxHigh = new ProbeReg("rampMaxHigh", "uint32_t",48052, mb);
rsvd_param .rampLimitSeconds = new ProbeReg("rampLimitSeconds", "uint32_t",48054, mb);
rsvd_param .rampValveMin = new ProbeReg("rampValveMin", "float",48056, mb);
rsvd_param .rampValveMax = new ProbeReg("rampValveMax", "float",48058, mb);
rsvd_param .rampState = new ProbeReg("rampState", "uint16_t",48082, mb);
rsvd_param .rampFault = new ProbeReg("rampFault", "uint16_t",48083, mb);
rsvd_param .rampTarget = new ProbeReg("rampTarget", "float",48084, mb);
rsvd_param .rampElapsedMs = new ProbeReg("rampElapsedMs", "uint32_t",48086, mb);
rsvd_param .rampHoldMs = new ProbeReg("rampHoldMs", "uint32_t",48088, mb);
rsvd_param .rampStableMs = new ProbeReg("rampStableMs", "uint32_t",48090, mb);
rsvd_param .rampLimitMs = new ProbeReg("rampLimitMs", "uint32_t",48092, mb);
rsvd_param .rampCycles = new ProbeReg("rampCycles", "uint32_t",48094, mb);
rsvd_param .flowScaleModel = new ProbeReg("flowScaleModel", "uint16_t",48026, mb);
rsvd_param .flowScaleVersion = new ProbeReg("flowScaleVersion", "uint16_t",48027, mb);
rsvd_param .flowFullScale = new ProbeReg("flowFullScale", "float",48028, mb);
        }
    }
}
