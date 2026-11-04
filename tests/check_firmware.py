"""Compile and link all firmware sources in a temporary directory.

Requires arm-none-eabi-gcc and arm-none-eabi-size on PATH. Does not alter IDE
build artifacts. Run from any directory: python tests/check_firmware.py
"""
from concurrent.futures import ThreadPoolExecutor
from pathlib import Path
import shutil
import subprocess
import tempfile

root = Path(__file__).resolve().parents[1] / "microFlow_cc_module"
compiler = shutil.which("arm-none-eabi-gcc")
if not compiler:
    raise SystemExit("arm-none-eabi-gcc is required on PATH")
output = Path(tempfile.mkdtemp(prefix="microflow-build-"))
includes = [
    "Core/Inc", "Drivers/STM32L4xx_HAL_Driver/Inc",
    "Drivers/STM32L4xx_HAL_Driver/Inc/Legacy",
    "Drivers/CMSIS/Device/ST/STM32L4xx/Include", "Drivers/CMSIS/Include",
    "Middlewares/Third_Party/FreeRTOS/Source/include",
    "Middlewares/Third_Party/FreeRTOS/Source/CMSIS_RTOS",
    "Middlewares/Third_Party/FreeRTOS/Source/portable/GCC/ARM_CM4F",
    "Middlewares/Modbus", "Middlewares/Modbus/port",
    "Middlewares/Modbus/modbus/include", "Middlewares/Modbus/modbus/rtu",
    "Middlewares/autoRegmap",
]
arch = ["-mcpu=cortex-m4", "-mfpu=fpv4-sp-d16", "-mfloat-abi=hard", "-mthumb"]
flags = arch + ["-std=gnu11", "-g", "-O0", "-DDEBUG", "-DUSE_HAL_DRIVER",
    "-DSTM32L432xx", "-ffunction-sections", "-fdata-sections", "-Wall",
    "--specs=nano.specs"] + ["-I" + str(root / p) for p in includes]
sources = []
for line in (root / "Debug/objects.list").read_text().splitlines():
    obj = Path(line.strip('"'))
    source = (root / obj).with_suffix(".c")
    if not source.exists():
        source = source.with_suffix(".s")
    if not source.exists():
        raise SystemExit(f"Missing source: {source}")
    sources.append(source)

# IDE-generated objects.list may still describe the older checked-in build.
# Include new application modules without requiring local Debug outputs in Git.
for source in sorted((root / "Core/Src").glob("*.c")):
    if source not in sources:
        sources.append(source)

def compile_source(source):
    obj = output / source.relative_to(root).with_suffix(".o")
    obj.parent.mkdir(parents=True, exist_ok=True)
    extra = ["-x", "assembler-with-cpp"] if source.suffix == ".s" else []
    run = subprocess.run([compiler, *flags, *extra, "-c", str(source), "-o", str(obj)],
                         capture_output=True, text=True)
    if run.stdout or run.stderr:
        print(run.stdout + run.stderr, end="")
    run.check_returncode()
    return str(obj)

with ThreadPoolExecutor(max_workers=4) as pool:
    objects = list(pool.map(compile_source, sources))
elf = output / "microFlow_cc_module.elf"
subprocess.run([compiler, *arch, *objects, "-T" + str(root / "STM32L432KBUX_FLASH.ld"),
    "--specs=nosys.specs", "--specs=nano.specs", "-Wl,--gc-sections", "-static",
    "-Wl,--start-group", "-lc", "-lm", "-Wl,--end-group", "-o", str(elf)], check=True)
subprocess.run([shutil.which("arm-none-eabi-size"), str(elf)], check=True)
print(f"Compiled and linked {len(sources)} sources. ELF: {elf}")
