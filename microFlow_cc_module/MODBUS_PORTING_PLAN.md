# microFlow 双串口 Modbus RTU 移植计划

## 1. 目标

在 STM32L432KBUx、STM32CubeIDE、FreeRTOS（CMSIS-RTOS v1）工程中移植 TEC 工程使用的 FreeModbus RTU 从站，实现：

- USART1：RS485 Modbus，PA9/PA10，PA12 使用 USART1 硬件 DE。
- USART2：CH340 USB-to-USART Modbus，PA2/PA3。
- 两个串口访问同一个从站地址和同一套寄存器。
- 请求从哪个串口进入，响应就从哪个串口返回。
- 同一时刻只处理一路完整事务，禁止两路数据混入同一 RTU 帧。
- SysTick 同时提供 HAL Tick 和 FreeRTOS Tick，TIM7 专用于 Modbus RTU 帧间隔。

参考工程：`D:\pyxis\software\gitee\miniTocTec\APP_RTOS`

目标工程：`D:\pyxis\software\github\micr0Flow_cc_moudle\microFlow_cc_module`

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
| 阶段 7 | 最小测试寄存器联调 | ⏳ 待测试 RS485 | - |
| 阶段 8 | 接入正式 autoRegmap | ⏳ 待开始 | - |
| 阶段 9 | 通信参数与 Flash 持久化 | ⏳ 待开始 | - |

**当前里程碑：USART2 (CH340 USB) Modbus 通信已验证通过**

## 3. 已创建/修改的文件清单

```
Core/
├── Inc/
│   ├── modbus.h              ← 新建：应用层接口、诊断计数器声明
│   ├── cv.h                  ← 新建：空桩头文件（解决 regmap.h 依赖）
│   └── main.h                ← 修改：添加 htim7/huart1/huart2 extern
├── Src/
│   ├── main.c                ← 修改：调用 Modbus_Init()、StartModbusTask 轮询
│   ├── stm32l4xx_it.c        ← 修改：USART1/USART2/TIM7 IRQHandler 接入 Modbus
│   └── modbus_app.c          ← 新建：具体实现函数

Middlewares/
├── Modbus/                   ← 从 TEC 复制并适配
│   ├── port/
│   │   ├── port.h            ← 修改：临界区改用 __disable_irq/__enable_irq
│   │   ├── portevent.c       ← 修改：volatile + 临界区 + 查询接口
│   │   ├── portevent.h       ← 新建：事件层头文件
│   │   ├── portserial.c      ← 重写：双串口仲裁 + 硬件 DE + TC 保护
│   │   └── porttimer.c       ← 适配 TIM7
│   ├── portrw.c              ← 寄存器读写（对接 autoRegmap）
│   └── modbus/
│       ├── include/
│       │   └── mbconfig.h    ← 修改：关闭不需要的功能码
│       └── rtu/mbrtu.c       ← 原版（临界区问题通过 port.h 解决）
└── autoRegmap/
    └── parameter.c           ← 修改：取消注释 include
```

## 4. 关键问题与解决方案记录

### 4.1 configASSERT 死循环（已解决）

**现象**：FreeRTOS 启动后所有任务卡死，调试器停在 `vPortEnterCritical()` 的 `configASSERT`。

**根因**：
```
USART2_IRQHandler (ISR上下文)
  → prvvUARTRxISR()
    → pxMBFrameCBByteReceived()
      → eMBRTUReceiveFSM()          ← mbrtu.c:155
        → ENTER_CRITICAL_SECTION()  ← 在 ISR 中调用！
          → taskENTER_CRITICAL()
            → vPortEnterCritical()
              → configASSERT( (portNVIC_INT_CTRL_REG & portVECTACTIVE_MASK) == 0 )
                → 断言失败！→ 死循环
```

FreeModbus 的 `mbrtu.c` 在 UART ISR 回调链路中调用了 `ENTER_CRITICAL_SECTION`，
而 FreeRTOS 的 `taskENTER_CRITICAL()` 内部会检测是否在中断上下文中调用，若在 ISR 中触发会导致死循环。

**解决方案**：将 `port.h` 中的临界区宏从 FreeRTOS 任务级改为原始中断屏蔽：

```c
// 之前（不兼容 ISR）
#define ENTER_CRITICAL_SECTION()   taskENTER_CRITICAL()
#define EXIT_CRITICAL_SECTION()    taskEXIT_CRITICAL()

// 之后（兼容任务和 ISR）
#define ENTER_CRITICAL_SECTION()   __disable_irq()
#define EXIT_CRITICAL_SECTION()    __enable_irq()
```

**注意**：TEC 工程使用相同的 `taskENTER_CRITICAL()` 但未暴露此问题，可能因编译优化或断言配置不同。

### 4.2 CubeMX 覆盖 IRQHandler（已解决）

**现象**：修改后的 `stm32l4xx_it.c` 被 CubeMX 重新生成，恢复为原始的 `HAL_UART_IRQHandler()` 调用。

**解决方案**：将 Modbus handler 放在 `/* USER CODE BEGIN xxx_IRQn 0 */` 区域，并在 `return` 后保留原始代码：

```c
void USART1_IRQHandler(void)
{
  /* USER CODE BEGIN USART1_IRQn 0 */
  extern void USART1_IRQHandler_USER(void);
  USART1_IRQHandler_USER();
  return;  // 直接返回，不执行 HAL_UART_IRQHandler
  /* USER CODE END USART1_IRQn 0 */
  HAL_UART_IRQHandler(&huart1);  // 不会执行到
  ...
}
```

### 4.3 StartModbusTask 空转（已解决）

**现象**：CubeMX 生成的 `StartModbusTask` 只有 `osDelay(1)`，从未调用 `eMBPoll()`。

**解决方案**：在 `/* USER CODE BEGIN StartModbusTask */` 中填入轮询逻辑：

```c
void StartModbusTask(void const * argument)
{
  /* USER CODE BEGIN StartModbusTask */
  for(;;) {
    eMBPoll();
    MB_PortAfterPoll();
    osDelay(1);
  }
  /* USER CODE END StartModbusTask */
}
```

### 4.4 HAL_UART_Transmit 只发一次（已解决）

**现象**：测试代码中 `HAL_UART_Transmit()` 只在第一次循环成功，后续卡住。

**原因**：与 Modbus 初始化中的 `HAL_UART_DeInit()/Init()` 以及中断状态冲突有关。此问题在恢复正常 `eMBPoll()` 循环后不再出现。

## 5. 当前工程基线

已确认：

- MCU 主频为 80 MHz。
- SysTick 为 1 kHz，同时维护 HAL Tick 和 FreeRTOS Tick。
- FreeRTOS `configTICK_RATE_HZ` 为 1000 Hz。
- TIM7 已启用，Prescaler=3999，计数频率 20kHz（50μs 单位）。
- USART1 已配置为 115200-8-N-1、RS485 Hardware DE (PA12)。
- USART2 已配置为 115200-8-N-1、普通异步 UART (PA2/PA3)。
- USART1/USART2/TIM7 全局中断均已启用，优先级为 5。
- FreeModbus 协议栈已加入工程并编译通过。
- **USART2 (CH340 USB) Modbus RTU 通信已验证通过**。

## 6. 时间基准分配

| 时间源 | 用途 | 周期/频率 | 中断优先级 |
| --- | --- | --- | --- |
| SysTick | HAL Tick + FreeRTOS Tick | 1 ms / 1 kHz | 15 |
| TIM7 | FreeModbus RTU T1.5/T3.5 | 50 us 计数单位 | 5 |

## 7. 总体结构

```text
USART1 / RS485 ─┐
                ├─ 双串口事务仲裁 ─ FreeModbus RTU ─ 寄存器回调 ─ autoRegmap
USART2 / CH340 ─┘
                         │
                         └─ TIM7 T1.5/T3.5
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
- USART1 使用硬件 DE，由 HAL_RS485Ex_Init 控制，不手动操作 PA12。
- USART2 不涉及 DE。

### 8.3 TC 中断保护

最后一个字节发送完成后，TC 中断才释放事务所有权，防止响应被截断：

```c
// TC 中断处理（以 USART2 为例）
if(__HAL_UART_GET_FLAG(&huart2, UART_FLAG_TC) != RESET) {
    __HAL_UART_CLEAR_FLAG(&huart2, UART_FLAG_TC);
    __HAL_UART_DISABLE_IT(&huart2, UART_IT_TC);
    g_mb_rx_busy = FALSE;
    __HAL_UART_ENABLE_IT(&huart2, UART_IT_RXNE);
}
```

## 9. 功能码配置 (mbconfig.h)

```c
// 已启用
MB_RTU_ENABLED                = 1
MB_FUNC_READ_INPUT            = 1   // 功能码 04
MB_FUNC_READ_HOLDING          = 1   // 功能码 03
MB_FUNC_WRITE_HOLDING         = 1   // 功能码 06
MB_FUNC_WRITE_MULTIPLE        = 1   // 功能码 16

// 已禁用（不需要或避免依赖缺失）
MB_ASCII_ENABLED              = 0
MB_FUNC_READ_COILS_ENABLED    = 0
MB_FUNC_WRITE_COIL_ENABLED    = 0
MB_FUNC_WRITE_MULTIPLE_COILS  = 0
MB_FUNC_READ_DISCRETE_INPUTS  = 0
MB_FUNC_READWRITE_HOLDING     = 0
MB_FUNC_OTHER_REP_SLAVEID     = 0
```

## 10. 诊断计数器 (mb_diag)

```c
typedef struct {
    uint32_t usart1_rx_frames;     // USART1 接收帧数
    uint32_t usart1_tx_frames;     // USART1 发送帧数
    uint32_t usart2_rx_frames;     // USART2 接收帧数
    uint32_t usart2_tx_frames;     // USART2 发送帧数
    uint32_t conflict_discards;    // 双串口冲突丢弃数
    uint32_t usart1_pe_count;      // USART1 奇偶错误
    uint32_t usart1_ore_count;     // USART1 溢出错误
    uint32_t usart1_fe_count;      // USART1 帧错误
    uint32_t usart1_ne_count;      // USART1 噪声错误
    uint32_t usart2_pe_count;      // USART2 奇偶错误
    uint32_t usart2_ore_count;     // USART2 溢出错误
    uint32_t usart2_fe_count;      // USART2 帧错误
    uint32_t usart2_ne_count;      // USART2 噪声错误
} MB_DiagCounters_t;
```

## 11. 后续步骤

### 阶段 7：RS485 测试与最小寄存器联调

- [ ] 通过 RS485 转 USB 模块连接 USART1，使用 Modbus Poll 测试
- [ ] 验证功能码 03/04/06/16 在 RS485 通道正常工作
- [ ] 验证双通道仲裁逻辑（同时从两路发请求）

### 阶段 8：接入正式 autoRegmap

- [ ] 创建完整的 `cv.h`（校准参数类型定义）
- [ ] 对接 `portrw.c` 到 autoRegmap 生成的寄存器表
- [ ] 验证所有寄存器区读写正确

### 阶段 9：Flash 持久化

- [ ] 实现通信参数保存到 Flash
- [ ] 实现参数变更后延迟重启机制

## 12. 关键约束

1. **USART1 使用硬件 DE**，不要手动操作 PA12。
2. **TXE 中断中直接写 TDR**，不要调用阻塞式 HAL 发送。
3. **TC 中断保护最后一个字节**，不能提前释放事务。
4. **临界区必须兼容 ISR**，使用 `__disable_irq()` 而非 `taskENTER_CRITICAL()`。
5. **所有手工代码放在 USER CODE 区域**，防止被 CubeMQ 覆盖。
6. **两个串口共享同一个 FreeModbus 实例**，必须先仲裁再喂给协议栈。
