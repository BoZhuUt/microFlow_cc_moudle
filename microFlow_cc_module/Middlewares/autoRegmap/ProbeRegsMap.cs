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
public ProbeReg IN_FAN_RATE;
public ProbeReg OUT_FAN_RATE;
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
rsvd_param .IN_FAN_RATE = new ProbeReg("IN_FAN_RATE", "float",48000, mb);
rsvd_param .OUT_FAN_RATE = new ProbeReg("OUT_FAN_RATE", "float",48002, mb);
        }
    }
}
