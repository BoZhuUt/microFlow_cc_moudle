"""Run actual ramp and Modbus C sources with a simulated clock and no hardware."""
from pathlib import Path
import shutil
import subprocess
import tempfile

repo = Path(__file__).resolve().parents[1]
root = repo / "microFlow_cc_module"
compiler = shutil.which("gcc")
if not compiler:
    raise SystemExit("gcc is required on PATH")
output = Path(tempfile.mkdtemp(prefix="microflow-tests-"))
for name in ("stm32l4xx_hal.h", "stm32l4xx.h", "FreeRTOS.h"):
    (output / name).write_text("#include <stdint.h>\n")
(output / "stm32l4xx_hal.h").write_text('''#pragma once
#include <stdint.h>
typedef enum {HAL_OK, HAL_ERROR} HAL_StatusTypeDef;
typedef struct { unsigned unused; } TIM_HandleTypeDef;
typedef struct { unsigned unused; } UART_HandleTypeDef;
typedef struct { uint32_t Banks, NbPages, Page, TypeErase; } FLASH_EraseInitTypeDef;
#define FLASH_PAGE_SIZE 2048U
#define FLASH_BANK_1 1U
#define FLASH_TYPEERASE_PAGES 2U
#define FLASH_TYPEPROGRAM_DOUBLEWORD 3U
#define __disable_irq() ((void)0)
#define __enable_irq() ((void)0)
void HAL_FLASH_Unlock(void);
void HAL_FLASH_Lock(void);
HAL_StatusTypeDef HAL_FLASHEx_Erase(FLASH_EraseInitTypeDef *, uint32_t *);
HAL_StatusTypeDef HAL_FLASH_Program(uint32_t, uint32_t, uint64_t);
''')
(output / "task.h").write_text(
    "#define taskENTER_CRITICAL() ((void)0)\n#define taskEXIT_CRITICAL() ((void)0)\n")
(output / "modbus.h").write_text('''#include "regmap.h"
typedef unsigned char UCHAR;
typedef unsigned short USHORT;
typedef enum {MB_ENOERR, MB_ENOREG, MB_EINVAL} eMBErrorCode;
typedef enum {MB_REG_READ, MB_REG_WRITE} eMBRegisterMode;
extern SYS_STATUS_T system_status;
extern COMM_SETTINGS_T comm_settings;
extern MEASURE_SETTINGS_T measure_settings;
extern CALIB_SETTINGS_T calib_settings;
extern FILTER_SETTINGS_T filter_settings;
extern MEASURE_VALUES_T measure_values;
extern RSVD_PARAM_T rsvd_param;
extern volatile uint8_t autoSavePending;
''')
includes = [output, root / "Core/Inc", root / "Middlewares/autoRegmap",
            root / "Middlewares/Modbus"]
# Exercise the actual static control functions without linking MCU startup/HAL.
main_source = (root / "Core/Src/main.c").read_text(encoding="utf-8")
control_start = main_source.index("static void FlowRamp_Publish(void)")
control_end = main_source.index("/* USER CODE END 4 */", control_start)
(output / "flow_control_under_test.inc").write_text(
    main_source[control_start:control_end], encoding="utf-8")
for test, sources in [
    ("test_flow_control", [root / "Core/Src/pid.c", root / "Core/Src/flow_ramp.c",
                           root / "Core/Src/parameter_storage.c"]),
    ("test_flow_ramp", [root / "Core/Src/flow_ramp.c"]),
    ("test_flow_scale", [root / "Core/Src/flow_scale.c"]),
    ("test_parameter_storage", [root / "Core/Src/parameter_storage.c", root / "Core/Src/flow_ramp.c"]),
    ("test_flash", [root / "Core/Src/flash.c", root / "Core/Src/parameter_storage.c",
                    root / "Core/Src/flow_ramp.c", root / "Core/Src/flow_scale.c",
                    root / "Middlewares/autoRegmap/parameter.c"]),
    ("test_modbus_ramp", [root / "Middlewares/Modbus/portrw.c"]),
]:
    exe = output / (test + ".exe")
    subprocess.run([compiler, "-std=c11", "-Wall", "-Werror",
        *["-I" + str(p) for p in includes], str(repo / "tests" / (test + ".c")),
        *map(str, sources), "-lm", "-o", str(exe)], check=True)
    subprocess.run([str(exe)], check=True)
