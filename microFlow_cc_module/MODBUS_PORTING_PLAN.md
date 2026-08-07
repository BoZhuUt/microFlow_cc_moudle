# microFlow 双串口 Modbus RTU 移植计划

## 1. 目标

在 STM32L432KBUx、STM32CubeIDE、FreeRTOS（CMSIS-RTOS v1）工程中移植 TEC 工程使用的 FreeModbus RTU 从站，实现：

- USART1：RS485 Modbus，PA9/PA10，PA12 使用 USART1 硬件 DE。
- USART2：CH340 USB-to-USART Modbus，PA2/PA3。
- 两个串口访问同一个从站地址和同一套寄存器。
- 请求从哪个串口进入，响应就从哪个串口返回。
- 同一时刻只处理一路完整事务，禁止两路数据混入同一 RTU 帧。
- SysTick 同时提供 HAL Tick 和 FreeRTOS Tick，TIM7 专用于 Modbus RTU 帧间隔。

参考工程：`D:\pyxis\software\gitee\miniTocTec\APP_RTOS` (STM32L431CCT6)

目标工程：`D:\pyxis\software\github\micr0Flow_cc_moudle\microFlow_cc_module` (STM32L432KBU6)

## 2. 进度总览（2026-08-07 更新）

| 阶段 | 内容 | 状态 | 完成日期 |
|------|------|------|----------|
| 阶段 0 | 保存基线 | ✅ 完成 | - |
| 阶段 1 | CubeMX 补齐外设（TIM7、UART 中断） | ✅ 完成 | - |
| 阶段 2 | 复制并裁剪 FreeModbus 协议栈 | ✅ 完成 | - |
| 阶段 3 | 实现 TIM7 Port 层 | ✅ 完成 | - |
| 阶段 4 | 实现双串口 Port 层 | ✅ 完成 | - |
| 阶段 5 | 修正事件层与事务释放 | ✅ 完成 | - |
| 阶段 6 | 增加 Modbus 应用层和任务 | ✅ 完成 | - |
| 阶段 7 | 双串口通信联调（RS485 + USB） | ✅ 完成 | - |
| 阶段 8 | 接入 autoRegmap 寄存器表 | ✅ 完成 | - |
| 阶段 9 | Flash 参数持久化 | ✅ 完成 | - |

**当前里程碑：全部核心功能已完成，双串口 Modbus RTU + Flash 存储正常运行**

## 3. 已创建/修改的文件清单

```
Core/
├── Inc/
│   ├── modbus.h              ← 新建：应用层接口、诊断计数器声明
│   ├── cv.h                  ← 新建：空桩头文件（解决 regmap.h 依赖）
│   ├── flash.h               ← 新建：Flash 地址定义、函数声明
│   ├── version.h             ← 新建：版本号定义 (SW/HW_VERSION_NUM)
│   ├── app.h                 ← 新建：命令码定义、函数声明
│   └── main.h                ← 修改：添加 extern 声明
├── Src/
│   ├── main.c                ← 修改：调用 Modbus_Init()、StartModbusTask 轮询
│   ├── stm32l4xx_it.c        ← 修改：USART1/USART2/TIM7 IRQHandler 接入 Modbus
│   ├── modbus_app.c          ← 新建：Modbus 初始化、参数初始化、任务入口
│   ├── app.c                 ← 新建：命令分发、autoSavePending 处理
│   └── flash.c               ← 新建：Flash 擦除/写入/保存/加载（从 TEC 复制）

Middlewares/
├── Modbus/                   ← 从 TEC 复制并适配
│   ├── port/
│   │   ├── port.h            ← 修改：临界区改用 __disable_irq/__enable_irq
│   │   ├── portevent.c       ← 修改：volatile + 临界区 + 查询接口
│   │   ├── portevent.h       ← 新建：事件层头文件
│   │   ├── portserial.c      ← 重写：双串口仲裁 + 硬件 DE + TC 保护
│   │   └── porttimer.c       ← 适配 TIM7
│   ├── portrw.c              ← 寄存器读写（对接 autoRegmap）
│   ├── portrw.h              ← 寄存器映射宏定义
│   └── modbus/
│       └── include/
│           └── mbconfig.h    ← 修改：关闭不需要的功能码
└── autoRegmap/
    ├── regmap.h              ← 自动生成：寄存器结构体定义
    ├── parameter.c           ← 自动生成：默认值、参数检查
    └── parameter.h           ← 新建：参数函数声明
```

## 4. 关键问题与解决方案记录

### 4.1 configASSERT 死循环（已解决）

**现象**：FreeRTOS 启动后所有任务卡死，调试器停在 `vPortEnterCritical()` 的 `configASSERT`。

**根因**：`mbrtu.c` 在 UART ISR 回调链路中调用了 `ENTER_CRITICAL_SECTION`，而 FreeRTOS 的 `taskENTER_CRITICAL()` 在 ISR 中调用会触发 `configASSERT` 死循环。

**解决方案**：将 `port.h` 中的临界区宏改为原始中断屏蔽：

```c
#define ENTER_CRITICAL_SECTION()   __disable_irq()
#define EXIT_CRITICAL_SECTION()    __enable_irq()
```

### 4.2 CubeMX 覆盖 IRQHandler（已解决）

**现象**：修改后的 `stm32l4xx_it.c` 被 CubeMX 重新生成。

**解决方案**：将 Modbus handler 放在 `/* USER CODE BEGIN xxx_IRQn 0 */` 区域并 `return`。

### 4.3 StartModbusTask 空转（已解决）

**现象**：CubeMX 生成的任务体只有 `osDelay(1)`，从未调用 `eMBPoll()`。

**解决方案**：填入 `eMBPoll()` + `MB_PortAfterPoll()` 循环。

### 4.4 Flash 写入 i=1 失败（已解决）

**现象**：`StoreModbusRegs()` 中 i=0 (system_status) 成功，i=1 (comm_settings) 返回 HAL_ERROR。

**排查过程**：
1. 结构体地址确认 8 字节对齐 → 排除对齐问题
2. FLASH_SR = 0 → 非 Flash 硬件错误
3. 尝试 WORD 模式替代 DOUBLEWORD → 未解决
4. 尝试外层统一 Unlock/Lock → 未解决
5. **最终方案：从 TEC 原版完整复制 flash.c → 解决**

**结论**：多次编辑累积的代码差异导致问题。TEC 原版（L431CCT6）验证过的代码可直接用于 L432KBU6。

### 4.5 RSVD_PARAM_T 结构体大小不匹配（已解决）

**现象**：编译时 `sizeof(RSVD_PARAM_T) != REG_GROUP_BYTES` 导致死循环。

**原因**：autoRegmap 生成的 `RSVD_PARAM_T` 包含 10 个 float + 多个 uint16_t，总大小不是 104 字节。

**解决**：调整 regmap_define 模板使每组恰好为 52 × uint16_t = 104 字节。

## 5. 当前工程基线

已确认：

- MCU 主频为 80 MHz。
- SysTick 为 1 kHz，同时维护 HAL Tick 和 FreeRTOS Tick。
- TIM7 已启用，Prescaler=3999，计数频率 20kHz（50μs 单位）。
- USART1 已配置为 RS485 Hardware DE (PA12)，波特率可配置。
- USART2 已配置为普通异步 UART (PA2/PA3)，波特率可配置。
- **USART1 (RS485) + USART2 (USB) 双串口 Modbus RTU 通信均已验证通过**
- **Flash Page 70 参数存储/加载正常**

## 6. 时间基准分配

| 时间源 | 用途 | 周期/频率 | 中断优先级 |
| --- | --- | --- | --- |
| SysTick | HAL Tick + FreeRTOS Tick | 1 ms / 1 kHz | 15 |
| TIM7 | FreeModbus RTU T1.5/T3.5 | 50 us 计数单位 | 5 |

## 7. 总体结构

```text
USART1 / RS485 ─┐
                ├─ 双串口事务仲裁 ─ FreeModbus RTU ─ 寄存器回调 ─ autoRegmap
USART2 / CH340 ─┘                         │
                         │                │
                         └─ TIM7 T1.5/T3.5  └─ Flash Page 70 (参数持久化)
```

## 8. 双串口仲裁设计要点

### 8.1 接收仲裁规则

1. IDLE 状态下，先收到首字节的串口成为 `owner`（`g_mb_rx_owner`）。
2. `owner` 的后续字节调用 `pxMBFrameCBByteReceived()`。
3. 非 `owner` 串口的数据必须读取 RDR 后丢弃，避免 ORE，同时增加冲突计数。
4. TIM7 到期后释放接收忙标志（`g_mb_rx_busy = FALSE`）。

### 8.2 发送路径

- `g_mb_uart_src` 决定使用哪个 UART 发送响应。
- `xMBPortSerialPutByte()` 直接写 TDR，不使用阻塞式 HAL 发送。
- USART1 使用硬件 DE，由 HAL_RS485Ex_Init 控制。
- USART2 不涉及 DE。

### 8.3 TC 中断保护

最后一个字节发送完成后，TC 中断才释放事务所有权，防止响应被截断。

## 9. 功能码配置 (mbconfig.h)

```c
// 已启用
MB_RTU_ENABLED                = 1
MB_FUNC_READ_INPUT            = 1   // 功能码 04
MB_FUNC_READ_HOLDING          = 1   // 功能码 03
MB_FUNC_WRITE_HOLDING         = 1   // 功能码 06
MB_FUNC_WRITE_MULTIPLE        = 1   // 功能码 16

// 已禁用
MB_ASCII_ENABLED              = 0
MB_FUNC_READ_COILS_ENABLED    = 0
MB_FUNC_WRITE_COIL_ENABLED    = 0
MB_FUNC_WRITE_MULTIPLE_COILS  = 0
MB_FUNC_READ_DISCRETE_INPUTS  = 0
MB_FUNC_READWRITE_HOLDING     = 0
MB_FUNC_OTHER_REP_SLAVEID     = 0
```

## 10. 寄存器表布局 (autoRegmap)

| Modbus 地址 | 区域 | 结构体 | 大小 |
|------------|------|--------|------|
| 41001~41052 | 系统状态 | `system_status` | 104B |
| 42001~42052 | 通信参数 | `comm_settings` | 104B |
| 43001~43052 | 测量设置 | `measure_settings` | 104B |
| 44001~44052 | 校准参数 | `calib_settings` | 104B |
| 45001~45052 | 滤波参数 | `filter_settings` | 104B |
| 46001~46052 | 测量数据 | `measure_values` | 104B |
| 48001~48052 | 保留/DO | `rsvd_param` | 104B |

## 11. Flash 存储设计

### 11.1 地址规划

```
Flash 基地址: 0x08000000
Page 大小:    2 KB (2048 字节)
存储页:      Page 70 = 0x08023000

用户参数区 (组 0~5):
  组0: 0x08023000  system_status
  组1: 0x08023068  comm_settings
  组2: 0x080230D0  measure_settings
  组3: 0x08023938  calib_settings
  组4: 0x080239A0  filter_settings
  组5: 0x08023A08  rsvd_param

出厂默认值区 (组 6~11):
  组6~11: 0x08023A70 ~ 0x08023CF4
```

### 11.2 命令码 (measure_settings.command)

| 值 | 功能 |
|----|------|
| 5 | 保存到 Flash |
| 4 | 恢复出厂设置 |
| 100 | 当前值存为出厂默认 |
| 103 | 从出厂默认值加载 |

### 11.3 上电加载流程

```
PowerOn_ReadModbusReg()
  → memcpy Flash → RAM (12 组)
  → newStructFlg != 0x2222? → ResetFlash() (首次启动)
  → CheckParam() (参数范围校验)
  → 版本号变更? → 自动更新并保存
```

## 12. 诊断计数器 (mb_diag)

可通过 Modbus 读取监控运行状态：

```c
typedef struct {
    uint32_t usart1_rx_frames;     // USART1 接收帧数
    uint32_t usart1_tx_frames;     // USART1 发送帧数
    uint32_t usart2_rx_frames;     // USART2 接收帧数
    uint32_t usart2_tx_frames;     // USART2 发送帧数
    uint32_t conflict_discards;    // 双串口冲突丢弃数
    // ... 各串口错误计数
} MB_DiagCounters_t;
```

## 13. 关键约束

1. **USART1 使用硬件 DE**，不要手动操作 PA12。
2. **TXE 中断中直接写 TDR**，不要调用阻塞式 HAL 发送。
3. **TC 中断保护最后一个字节**，不能提前释放事务。
4. **临界区必须兼容 ISR**，使用 `__disable_irq()` 而非 `taskENTER_CRITICAL()`。
5. **所有手工代码放在 USER CODE 区域**，防止被 CubeMQ 覆盖。
6. **flash.c 直接从 TEC 复制**，不要自行修改核心写入逻辑。
7. **regmap.h 重新生成后需确认结构体大小 = 104 字节**。
