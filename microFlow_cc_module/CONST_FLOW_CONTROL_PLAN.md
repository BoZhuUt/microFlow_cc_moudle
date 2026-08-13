# 微流量恒流控制（PWM 阀门闭环）规划

> 文档版本：v1.0
> 创建日期：2026-08-11
> 模块：microFlow_cc_module
> MCU：STM32L432KBU6 @ 80 MHz，FreeRTOS + FreeModbus RTU

---

## 1. 概述

当前 `StartFlowTask` 为**开环**控制：Modbus 写入 `rsvd_param.valveOpening`（默认 30%），任务直接经 `Valve_SetDuty()` 输出到 TIM1_CH1（PA8）PWM，`flowRateSet`（默认 5.0 mL/min）只是个设定值，**未参与控制**。

本规划目标：在保留手动模式的前提下，引入 **PID 闭环**，通过实时流量反馈自动调节阀门开度，使实际流量稳定跟踪 `flowRateSet`，达到 **5 mL/min ±0.5 mL/min** 的控制精度。

---

## 2. 目标指标

| 指标 | 目标值 | 说明 |
|------|--------|------|
| 控制精度 | 设定值 ±0.5 mL/min（5 mL/min 时为 ±10%） | 稳态；注：5 mL/min 需供压足够（见附录 A.6） |
| 控制周期 | 100 ms（可配置） | 与现有任务周期一致 |
| 阶跃响应 | < 3 s 进入 ±5% 带 | 流量阶跃 ±20% |
| 超调 | < 15% | 启动 / 阶跃 |
| 模式切换 | 手动⇄自动无扰动（bumpless transfer） | 切换瞬间输出不跳变 |
| 输出范围 | 0% ~ 100% PWM 开度 | 限幅 + 抗积分饱和 |

---

## 3. 现状分析

### 3.1 硬件资源

| 资源 | 配置 | 当前用途 | 闭环复用 |
|------|------|----------|----------|
| ADC1_CH6 | 12bit, 软件触发, 采样 2.5 cycles | 流量计电压采集 | 反馈量（保留） |
| TIM1_CH1 / PA8 | Prescaler=1, Period=39999 → **1 kHz, 40000 步** | 阀门 PWM | 控制输出（保留） |
| TIM7 | Modbus T3.5 | 协议帧定时 | 不动 |
| TIM16 | 10 Hz（未启动中断） | 预留 | 可选作控制节拍源（暂不用） |

- PWM 分辨率：开度步长 = 100% / 40000 = **0.0025%/步**，对阀门控制绰绰有余。
- PWM 频率 1 kHz：对比例电磁阀通常合适；若为步进电机阀需另行评估（见 §9 风险）。

### 3.2 软件任务

| 任务 | 优先级 | 栈 | 周期 | 职责 |
|------|--------|-----|------|------|
| `defaultTask` | Normal | 128 | 100 ms | 空循环（预留） |
| `atuoFlowTask` (`StartFlowTask`) | **Idle** | 256 | 100 ms | 流量采集 + 阀门输出 |
| `modbusTask` (`StartModbusTask`) | Idle | 256 | 1 ms | Modbus 轮询 + 命令分发 |

### 3.3 关键参数（RSVD_PARAM_T，自动生成，勿手改）

| 字段 | 默认值 | 当前含义 | 闭环用途 |
|------|--------|----------|----------|
| `flowRateVoltageAve` | 5.0 | 流量计电压（V） | 反馈电压（保留） |
| `flowRateAve` | 5.0 | 当前流量（mL/min） | **PID 反馈量（PV）** |
| `aveNum` | 5.0 | 平均数（未使用，硬编码 10） | 预留：可作滤波系数 |
| `flowRateSet` | 5.0 | 目标流量 | **PID 设定值（SP）** |
| `valveOpening` | 30.0 | 阀门开度 (%) | **PID 输出（MV）/ 手动输出** |
| `manualMode` | 1 | 手动模式标志 | **0=自动PID, 1=手动** |
| `modbusCmd2` | 0 | 未使用（0~1000） | **PID 命令字** |
| `lowFlow/highFlow/...Voltage` | 0/30/0.25/2.25 | 两点标定 | 流量换算（保留） |

> ⚠️ `regmap.h` 由 Probe Parameter Editor 自动生成，**不可手动编辑**。PID 参数（Kp/Ki/Kd）若要在线可调，必须用工具重新生成寄存器；第一版先用宏定义硬编码（见 §6.4）。

### 3.4 现有 StartFlowTask 逻辑（待改造）

```c
void StartFlowTask(void const * argument)
{
  float flow;
  for(;;)
  {
    flow = getFlow(FLOW_ADC_AVERAGE_COUNT, lowFlow, highFlow, ...);
    if (flow >= 0.0f) rsvd_param.flowRateAve = flow;
    rsvd_param.flowRateVoltageAve = getAdcVoltage(FLOW_ADC_AVERAGE_COUNT); // 重复采样
    Valve_SetDuty(rsvd_param.valveOpening);  // 开环
    osDelay(100);
  }
}
```

**问题**：
1. 开环，`flowRateSet` 不参与控制；
2. `getFlow` 与 `getAdcVoltage` 各采 10 次 ADC，共 20 次转换，**浪费时间且两次结果不一致**（应合并为一次采样）；
3. 无模式切换、无 PID、无抗饱和、无无扰切换。

---

## 4. 控制方案选型

### 4.1 PID 类型：位置式 + 抗积分饱和

- **选位置式**：输出直接对应阀门开度（0~100%），物理意义清晰，与手动模式 `valveOpening` 天然衔接。
- **抗积分饱和（Clamping）**：输出触限后停止积分累积，防止开环饱和导致超调。
- **微分项处理**：流量计有噪声，**对测量值 PV 取微分**（而非误差），并加一阶低通，避免设定值阶跃时微分冲击。第一版可先关微分（Kd=0），用 PI 控制。

### 4.2 模式切换

| `manualMode` | 模式 | 行为 |
|--------------|------|------|
| 1（默认） | 手动 | `Valve_SetDuty(rsvd_param.valveOpening)`（保持现有逻辑） |
| 0 | 自动（PID） | PID 计算 → 写 `valveOpening` → `Valve_SetDuty()` |

### 4.3 无扰切换（Bumpless Transfer）

- **手动 → 自动**：切换瞬间，将 PID 积分项预置为 `当前 valveOpening - Kp*e - Kd*de`，使首次 PID 输出 = 当前手动开度，无跳变。
- **自动 → 手动**：无需处理，手动模式直接读 `valveOpening`，而 PID 在自动模式已持续写入 `valveOpening`，天然衔接。

### 4.4 死区与限幅

- **死区**：|e| < 0.1 mL/min 时输出保持，避免阀门频繁抖动（机械磨损）。
- **限幅**：输出 0% ~ 100%。
- **变化率限幅**：单周期输出变化 ≤ 5%（可选，防止水锤），第一版可不开。

---

## 5. 系统架构

```mermaid
flowchart LR
    SP[flowRateSet<br/>设定值] --> Sum((+))
    PV[flowRateAve<br/>实测流量] --> Sum((-))
    Sum -->|e| PID[PID<br/>位置式+抗饱和]
    PID -->|MV| Lim[限幅/死区<br/>0~100%]
    Lim --> VO[valveOpening]
    VO --> PWM[Valve_SetDuty<br/>TIM1_CH1 PWM]
    PWM --> Valve[比例阀]
    Valve --> Meter[流量计]
    Meter --> ADC[ADC1_CH6]
    ADC --> Filt[低通滤波]
    Filt --> Calc[getFlow<br/>两点标定]
    Calc --> PV

    Mode{manualMode} -->|0| PID
    Mode -->|1| Manual[直接输出<br/>valveOpening]
    Manual --> VO
```

---

## 6. 详细设计

### 6.1 PID 模块（新增 `Core/Inc/pid.h` + `Core/Src/pid.c`）

**接口设计**：

```c
/* pid.h */
typedef struct {
    float Kp;           /* 比例 */
    float Ki;           /* 积分 */
    float Kd;           /* 微分 */
    float outMin;       /* 输出下限（0%） */
    float outMax;       /* 输出上限（100%） */
    float deadband;     /* 死区 */
    float integral;     /* 积分累积（内部） */
    float prevError;    /* 上次误差（内部） */
    float prevMeas;     /* 上次测量值，用于微分（内部） */
    float outPrev;      /* 上次输出（内部，用于无扰切换） */
} PID_Controller_t;

void  PID_Init(PID_Controller_t *pid, float kp, float ki, float kd,
               float outMin, float outMax, float deadband);
void  PID_Reset(PID_Controller_t *pid, float presetOutput);
float PID_Compute(PID_Controller_t *pid, float setpoint, float measurement, float dt);
```

**算法（位置式 + 抗饱和 + PV 微分）**：

```c
float PID_Compute(PID_Controller_t *pid, float sp, float pv, float dt)
{
    float error = sp - pv;

    /* 死区 */
    if (fabsf(error) < pid->deadband) {
        return pid->outPrev;          /* 维持上次输出 */
    }

    /* 比例 */
    float p = pid->Kp * error;

    /* 积分（前向欧拉） */
    float iCandidate = pid->integral + pid->Ki * error * dt;

    /* 微分（基于测量值，避免 SP 阶跃冲击） */
    float d = -pid->Kd * (pv - pid->prevMeas) / dt;

    /* 试探输出 */
    float out = p + iCandidate + d;

    /* 抗积分饱和：仅当输出未触限时才提交积分 */
    if (out > pid->outMax) {
        out = pid->outMax;
        /* 积分不更新（保留原值） */
    } else if (out < pid->outMin) {
        out = pid->outMin;
        /* 积分不更新 */
    } else {
        pid->integral = iCandidate;   /* 提交 */
    }

    pid->prevError = error;
    pid->prevMeas  = pv;
    pid->outPrev   = out;
    return out;
}
```

**无扰切换**：

```c
/* 手动→自动切换时调用，presetOutput = 切换瞬间的 valveOpening */
void PID_Reset(PID_Controller_t *pid, float presetOutput)
{
    pid->integral  = presetOutput;   /* 使首次 p+i+d ≈ presetOutput */
    pid->prevError = 0.0f;
    pid->prevMeas  = 0.0f;
    pid->outPrev   = presetOutput;
}
```

### 6.2 StartFlowTask 改造

```c
/* 文件顶部宏定义（第一版硬编码，见 §6.4；数值依据附录 A 实测修订） */
#define PID_KP              5.0f    /* 工作点增益~0.045, 开环增益0.225 偏安全 */
#define PID_KI              0.2f    /* Ti≈25s, 对象响应6s+过冲, Ki须小 */
#define PID_KD              0.0f    /* 对象欠阻尼, 禁用微分 */
#define PID_OUT_MIN         35.0f   /* 避阀门死区(<35%不开) */
#define PID_OUT_MAX         70.0f   /* >70%主回路分流,调节无效 */
#define PID_DEADBAND        0.1f    /* mL/min */
#define FLOW_TASK_PERIOD_MS 100U
#define FLOW_FILTER_ALPHA   0.2f    /* 一阶低通: y = a*x + (1-a)*y_prev */
#define FLOW_ADC_AVERAGE_COUNT 20U  /* ADC平均次数, 10→20 降噪声 */

static PID_Controller_t flowPid;
static uint16_t prevManualMode = 1U;
static float flowFiltered = 0.0f;

void StartFlowTask(void const * argument)
{
    float voltage;
    float flowRaw;
    float duty;

    PID_Init(&flowPid, PID_KP, PID_KI, PID_KD,
             PID_OUT_MIN, PID_OUT_MAX, PID_DEADBAND);

    for(;;)
    {
        /* === 1. 采样：一次 ADC 多次平均，电压与流量共用 === */
        voltage = getAdcVoltage(FLOW_ADC_AVERAGE_COUNT);
        if (voltage < 0.0f) { osDelay(FLOW_TASK_PERIOD_MS); continue; }
        rsvd_param.flowRateVoltageAve = voltage;

        flowRaw = rsvd_param.lowFlow +
                  (voltage - rsvd_param.lowFlowVoltage) *
                  (rsvd_param.highFlow - rsvd_param.lowFlow) /
                  (rsvd_param.highFlowVoltage - rsvd_param.lowFlowVoltage);

        /* === 2. 反馈低通滤波（降低 ADC 噪声） === */
        if (flowFiltered == 0.0f) flowFiltered = flowRaw;   /* 首次 */
        flowFiltered = FLOW_FILTER_ALPHA * flowRaw +
                       (1.0f - FLOW_FILTER_ALPHA) * flowFiltered;
        rsvd_param.flowRateAve = flowFiltered;

        /* === 3. 模式切换边沿检测 → 无扰切换 === */
        if (rsvd_param.manualMode != prevManualMode) {
            if (rsvd_param.manualMode == 0U) {
                /* 手动→自动：以当前开度预置积分 */
                PID_Reset(&flowPid, rsvd_param.valveOpening);
            }
            prevManualMode = rsvd_param.manualMode;
        }

        /* === 4. 控制律 === */
        if (rsvd_param.manualMode == 1U) {
            /* 手动：直接用寄存器设定开度 */
            duty = rsvd_param.valveOpening;
        } else {
            /* 自动：PID 闭环 */
            duty = PID_Compute(&flowPid,
                               rsvd_param.flowRateSet,
                               flowFiltered,
                               (float)FLOW_TASK_PERIOD_MS / 1000.0f);
            rsvd_param.valveOpening = duty;   /* 回写，便于监视/手动衔接 */
        }

        /* === 5. 输出 === */
        Valve_SetDuty(duty);

        osDelay(FLOW_TASK_PERIOD_MS);
    }
}
```

> 注：上面直接在任务里展开了两点标定公式，避免 `getFlow()` 与 `getAdcVoltage()` 重复采样。也可改造 `getFlow()` 增加一个"传入已采电压"的重载版本，保持函数复用。

### 6.3 modbusCmd2 命令协议

`modbusCmd2`（寄存器 48012，范围 0~1000）作为 PID 控制命令字，处理后在任务里清零：

| `modbusCmd2` 值 | 含义 | 处理位置 |
|-----------------|------|----------|
| 0 | 无命令 | — |
| 1 | PID 复位（积分清零到当前开度） | StartFlowTask |
| 2 | 切自动模式（`manualMode=0`） + PID 复位 | StartFlowTask |
| 3 | 切手动模式（`manualMode=1`） | StartFlowTask |
| 10 | 保存参数到 Flash（等价 `SaveToFlahCMD`） | MeasureFunc |
| 100~103 | 恢复出厂/加载出厂等（复用现有命令） | MeasureFunc |

**实现**（在 StartFlowTask 采样前检查）：

```c
switch (rsvd_param.modbusCmd2) {
    case 1:  PID_Reset(&flowPid, rsvd_param.valveOpening); break;
    case 2:  rsvd_param.manualMode = 0U;
             PID_Reset(&flowPid, rsvd_param.valveOpening); break;
    case 3:  rsvd_param.manualMode = 1U; break;
    default: break;
}
if (rsvd_param.modbusCmd2 != 0 && rsvd_param.modbusCmd2 < 10) {
    rsvd_param.modbusCmd2 = 0;   /* 命令类立即清零，10+ 交给 MeasureFunc */
}
```

### 6.4 PID 参数管理

**第一版**：宏定义在 `main.c`（或新建 `pid_config.h`）：
```c
#define PID_KP   8.0f
#define PID_KI   0.5f
#define PID_KD   0.0f
```

**第二版（可选）**：通过 Probe Parameter Editor 在 `RSVD_PARAM_T` 的 `reserved[32]` 区新增 3 个 float（Kp/Ki/Kd，占 12 个 uint16），重新生成 regmap，实现在线整定。需同步更新 `parameter.c` 默认值与 `CheckParamyDefaultSetting()` 范围校验。

---

## 7. 实施阶段

### Phase 1：PID 模块（独立，可单测）
- [ ] 新建 `Core/Inc/pid.h`、`Core/Src/pid.c`
- [ ] 实现 `PID_Init` / `PID_Reset` / `PID_Compute`
- [ ] 加入 Debug/makefile 编译（`Debug/Core/Src/subdir.mk` 自动扫描或手动添加）

### Phase 2：StartFlowTask 改造
- [ ] 合并 ADC 采样（消除 `getFlow`+`getAdcVoltage` 重复采样）
- [ ] 加入一阶低通滤波
- [ ] 加入模式切换边沿检测 + 无扰切换
- [ ] 自动模式接入 PID
- [ ] 手动模式保持原逻辑

### Phase 3：modbusCmd2 命令处理
- [ ] StartFlowTask 内处理 cmd 1/2/3
- [ ] MeasureFunc 内处理 cmd 10+（复用现有保存/恢复）

### Phase 4：任务优先级与节拍
- [ ] 评估提高 `atuoFlowTask` 优先级到 `osPriorityBelowNormal`（避免被 Modbus 长报文阻塞导致控制周期抖动）
- [ ] 确认 100ms 节拍稳定（或改用 TIM16 硬件定时中断触发采样）

### Phase 5：整定与验收
- [ ] 手动模式验证阀门线性度（开度 10%~90% 扫描，记录流量）
- [ ] 自动模式 P 整定 → PI 整定
- [ ] 阶跃响应测试（SP: 3→5→7 mL/min）
- [ ] 扰动测试（改变供压）
- [ ] 精度验收：稳态 ±0.5 mL/min

---

## 8. PID 参数整定指南

控制周期 T = 100 ms = 0.1 s。

### 8.1 初值估算（基于附录 A 实测）
- 实测工作点稳态增益 **Kplant ≈ 0.045 (mL/min)/%**（40~70% 段）。
- 期望开环增益 Kp·Kplant ≈ 0.2~0.3（对象有过冲，取保守值）→ **Kp ≈ 5**。
- 对象响应 6 s + 过冲 21%，积分时间 Ti 须 >2 倍主时间常数 → Ti ≈ 20~25 s → Ki = Kp/Ti ≈ 5/25 = **0.2**。
- **Kd = 0**（对象欠阻尼，禁用微分）。
- 输出限幅 35%~75%（避死区与饱和，见附录 A.4）。
- 以上即第一版初值，整定时按 §8.2 流程微调。

### 8.2 整定流程
1. **手动模式**：Modbus 写 `manualMode=1`，扫描 `valveOpening`，记录 `flowRateAve`，确认线性、无卡死、无迟滞。
2. **纯 P**：`manualMode=0`，Kp 从 2 起递增，观察响应速度与超调，至出现轻微振荡后回退 30%。
3. **加 I**：Ki 从 0.05 起递增，消除稳态误差，至出现低频振荡后回退。
4. **加 D（可选）**：若响应慢且噪声小，Kd 从 0.1 起小步加，配合 PV 微分。
5. **死区**：稳态抖动时把 `PID_DEADBAND` 从 0.1 加到 0.2~0.3。

### 8.3 采样与滤波
- ADC 平均 10 次（已有），100ms 周期内完成，OK。
- 一阶低通 `α=0.3`：截止 fc ≈ (α)/(2π(1-α)T) ≈ 0.68 Hz，对流量计噪声合适；过强会引入相位滞后，影响稳定性，慎调。

---

## 9. 风险与注意事项

### 9.1 硬件层（需先排查，否则 PID 调不出来）

| 风险 | 现状 | 建议 |
|------|------|------|
| **PA6 GPIO 配置冲突** | `MX_GPIO_Init()` 把 PA6 配为 `OUTPUT_PP`，但 ADC1_CH6 也是 PA6 | 确认 `HAL_ADC_MspInit()`（stm32l4xx_hal_msp.c）是否覆盖为模拟模式；若否，删除 `MX_GPIO_Init` 中 PA6 输出配置 |
| **ADC 采样时间过短** | `ADC_SAMPLETIME_2CYCLES_5`（约 31 ns） | 提高到 `ADC_SAMPLETIME_247CYCLES_5` 或更长，降低源阻抗误差，直接影响 ±0.5 mL/min 精度 |
| **PWM 频率适配性** | 1 kHz | 确认比例阀规格书；部分低频阀需 100~200 Hz，部分高频阀需 10 kHz+ |
| **供压稳定性** | 未知 | 恒流前提是供压恒定；供压波动会直接映射为流量扰动，PID 难以完全消除 |
| **主回路分流** | 70% 以上支路流量封顶（主回路分流，非供压不足） | PID 输出上限限 70%；5 mL/min 可达性取决于主回路工况，必要时调整主回路阻尼 |

### 9.2 软件层

| 风险 | 影响 | 对策 |
|------|------|------|
| 控制周期抖动 | atuoFlowTask 与 modbusTask 同为 Idle，Modbus 长报文可能拖延 | 提高 atuoFlowTask 到 BelowNormal；或用 TIM16 中断节拍 |
| float 在 Modbus 读写中的原子性 | `flowRateSet`/`valveOpening` 4 字节，被任务读时若被中断改写会读到半新半旧 | 关键读取用 `__disable_irq()/__enable_irq()` 包裹，或用 `__LDREX/__STREX`；第一版可先观察 |
| PID 参数硬编码 | 无法在线整定 | 第一版可接受；第二版用工具加寄存器 |
| 模式切换抖动 | manualMode 寄存器毛刺 | 边沿检测已处理；建议 Modbus 端写后回读确认 |

### 9.3 失效安全
- ADC 读失败（`voltage < 0`）：本次跳过控制，**保持上一次开度**（不归零，避免流量突冲）。
- 流量长时间偏离设定值（>5s 且 |e|>2 mL/min）：置 `system_status.runStatus` 报警位（后续实现）。
- 手动模式作为安全降级：异常时 Modbus 写 `manualMode=1` + `valveOpening=安全值`。

---

## 10. 验收标准

1. **手动模式**：`manualMode=1`，写 `valveOpening=50`，PWM 占空比 = 50% ±1%，行为与改造前一致。
2. **模式切换无扰动**：自动稳态 5 mL/min 时切手动，开度不跳变；手动 30% 时切自动，首次输出 ≈ 30%。
3. **自动模式稳态**：`flowRateSet=5`，稳定后 `flowRateAve` 在 **4.5 ~ 5.5 mL/min** 持续 60s。
4. **阶跃响应**：SP 3→5 mL/min，5 s 内进入 ±0.5 带，超调 < 15%。
5. **命令字**：写 `modbusCmd2=1` 立即复位 PID；`=2` 切自动；`=3` 切手动；执行后寄存器清零。
6. **保存恢复**：PID 相关默认参数（Kp/Ki/Kd 第二版）经 Flash 保存/上电恢复正常。

---

## 11. 待确认事项（需用户/硬件侧反馈）

- [ ] 比例阀型号、工作频率、PWM 驱动要求（电压/电流、是否需保持电流）
- [ ] 流量计型号、量程、输出电压范围、响应时间
- [ ] 供压方式（恒压源 / 泵 / 气压），压力稳定性
- [ ] PA6 是否确认由 ADC MspInit 接管（核实 stm32l4xx_hal_msp.c）
- [ ] 是否需要在线整定 PID（决定是否走第二版寄存器方案）
- [ ] 是否需要流量累计、报警上报等扩展功能

---

---

## 附录 A：手动调节测试数据分析（2026-08-11）

> 数据来源：`IQ_PSF_A手动调节测试(20260811).xlsx`
> 测试方式：开环手动模式，开度阶跃 30%→40%→50%→60%→70%→80%→90%，每档保持 45~170 s，采样周期约 1 s
> 当时标定参数：lowFlow=0, highFlow=30, lowFlowVoltage=0.25, highFlowVoltage=2.25（斜率 15 mL/min/V）

### A.1 各开度稳态汇总

| 开度 | 稳态电压 (V) | 稳态流量 (mL/min) | 段增量 | 段增益 (mL/min/%) | 响应时间 | 稳态噪声峰峰 |
|------|------|------|------|------|------|------|
| 30% | 0.251 | ≈0.01 | — | — | — | 0.025 |
| 40% | 0.458 | 3.13 | +3.13 | 0.31¹ | 7~8 s | 0.18 |
| 50% | 0.491 | 3.61 | +0.47 | 0.047 | 10 s（过冲 21%） | 0.04 |
| 60% | 0.515 | 3.98 | +0.38 | 0.038 | 4~5 s | 0.05（+尖峰 0.4） |
| 70% | 0.553 | 4.52 | +0.54 | 0.054 | 5 s | 0.11 |
| 80% | 0.559 | 4.62 | +0.10 | 0.010 | 7 s | 0.05 |
| 90% | 0.566 | 4.73 | +0.11 | 0.011 | 6 s | 0.04 |

¹ 30→40% 跨越阀门死区，非稳态增益

### A.2 观察点1：流量噪声（需滤波）

- 常规稳态噪声峰峰值 **0.04~0.18 mL/min**，60% 段偶发尖峰达 **0.4 mL/min**。
- 噪声来源：ADC 量化（采样时间过短 2.5 cycles）+ 流量计自身抖动 + 管路微扰。
- 在 ±0.5 mL/min 目标中占比 10~35%，**必须滤波**，但不算严重。
- **建议**：ADC 平均次数 10→20；软件一阶低通 α=0.2~0.3；ADC 采样时间提高到 247.5 cycles（见 §9.1）。

### A.3 观察点2：响应慢 + 过冲（对象为欠阻尼高阶特性）

- 阶跃响应 **5~10 s** 才进入稳态带，远慢于 100 ms 控制周期。
- 50% 段最典型：阶跃后瞬时峰值 4.37 mL/min，稳态 3.61，**过冲 21%**，再经 ~10 s 衰减到位。
- 结论：阀+管路+流量计是**欠阻尼高阶对象**，不是一阶惯性。
- **对 PID 的影响**：
  - Kd 基本不能用（对象本身已欠阻尼，加微分会激发振荡）→ **Kd=0**。
  - Ki 必须小，积分时间 Ti ≥ 15~20 s，否则在对象未响应时过积分→振荡。
  - 控制周期 100 ms 仍可用（dt 小，积分步长小），但不是越快越好；必要时可放慢到 200~500 ms 减少对噪声的敏感。

### A.4 观察点3：调节范围窄（最关键问题）

- **30% 以下死区**：流量≈0，阀门未有效开启。
- **40%~70% 有效调节区**：仅 3.1~4.5 mL/min（跨度 1.4 mL/min）。
- **70% 以上为主回路分流区**：80%→90% 流量仅增 0.11 mL/min，支路流量基本封顶。该区间调节无效——阀门虽可开到 99.99%，但对支路流量无贡献（用户确认：70% 以上看主回路，不考虑）。
- **支路流量上限约 4.7 mL/min** → 5 mL/min 目标是否可达取决于主回路工况。
- 工作点稳态增益约 **0.04~0.05 (mL/min)/%**（40~70% 段），70% 以上骤降至 0.01。
- 流量计量程 0~30 mL/min（0.25~2.25 V），实测仅用到 0.25~0.57 V，**仅用量程 1/6**，分辨率与信噪比严重浪费。

> **关于中间段调节度**：40%~70% 段内增益也有变化（0.038~0.054 (mL/min)/%），同样受主流道影响。第一版**按平均增益 0.045 整定 PID，不做增益调度（gain scheduling）**；整定时若发现不同工作点表现差异大，再考虑分段线性化或增益调度。

### A.5 对规划的影响与修订

| 项 | 原规划 | 修订后 | 依据 |
|----|--------|--------|------|
| 目标流量 | 5 mL/min ±0.5 | **当前硬件上限 4.7 mL/min**，需先提供压；过渡验收按 4 mL/min ±0.5 | A.4 |
| PID_OUT_MIN | 0% | **35%**（避死区，<35% 阀门不开） | A.4 |
| PID_OUT_MAX | 100% | **70%**（>70% 主回路分流，调节无效） | A.4 |
| PID_KP | 8.0 | **5.0** | 工作点增益 0.045，开环增益 Kp·Kplant=0.225 偏安全 |
| PID_KI | 0.5 | **0.2**（Ti≈25 s） | 对象响应 6 s+过冲，Ti 须 >15 s |
| PID_KD | 0.0 | 0.0（确认） | 对象欠阻尼，禁用 |
| FLOW_FILTER_ALPHA | 0.3 | **0.2** | 噪声不大，加强平滑 |
| ADC 平均次数 | 10 | **20** | 降噪声 |
| 两点标定 | 0~30mL/min @ 0.25~2.25V | **供压确定后重标**：highFlowVoltage≈0.57V, highFlow≈实测满量程 | A.4 量程浪费 |
| 阀门死区补偿 | 无 | 输出<35% 时直接跳到 35% | A.4 |
| 控制周期 | 100 ms | 100 ms 保留（dt 小利于积分稳定） | A.3 |

### A.6 待解决的硬件前提（优先级最高）

1. **主回路工况**：70% 以上支路流量封顶系**主回路分流**所致（非供压不足，阀门可开至 99.99% 但对支路无贡献）。需确认目标 5 mL/min 在当前主回路工况下是否可达，必要时调整主回路阻尼/分流，使支路在有效调节区（40%~70%）内能覆盖目标流量。
2. **流量计量程匹配**：当前 0~30 mL/min 量程只用到 1/6，建议换 0~10 mL/min 量程流量计，或重新标定缩放。
3. **阀门死区**：30% 以下无效，需在 PWM 输出侧加死区补偿。
4. **供压稳定性**：60% 段偶发 0.4 mL/min 尖峰，疑似供压波动，恒流前提是供压恒定。

> 若上述硬件问题不解决，PID 调得再好也无法在 5 mL/min 稳定工作。建议先做 A.6 的硬件排查，再进入 Phase 5 整定。

---

*本规划为实施前的设计基线，Phase 1~3 可直接落地，Phase 4~5 视整定结果迭代。附录 A 基于实测数据对规划参数做了第一轮修订。*
