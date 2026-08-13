/**
  ******************************************************************************
  * @file    pid.h
  * @brief   通用位置式 PID 控制器（抗积分饱和 + PV 微分 + 死区）
  ******************************************************************************
  * @attention
  *   设计要点：
  *   - Kp/Ki/Kd 不存于实例中，作为 PID_Compute 参数每次传入，
  *     支持 Modbus 在线整定（配合 rsvd_param.PID_P / PID_I / PID_D 寄存器）。
  *   - 实例仅存限幅/死区配置与内部状态（积分项、上次 PV、上次输出）。
  *
  *   参数语义（标准独立形式）：
  *     MV = Kp·e + Ki·∫e·dt + Kd·(-dPV/dt)
  *   其中 e = SP - PV，MV 为输出；D 项基于 PV 变化（避免 SP 阶跃冲击）。
  *   单位约定（本工程）：
  *     SP/PV: mL/min；MV: % 开度；dt: s
  *     Kp: %/(mL/min)；Ki: %/(mL/min)/s = Kp/Ti；Kd: %/(mL/min)·s
  ******************************************************************************
  */
#ifndef __PID_H
#define __PID_H

#include <stdint.h>

/** @brief PID 控制器实例 */
typedef struct {
    /* ---- 配置项（PID_Init 设定，运行中不变） ---- */
    float outMin;      /*!< 输出下限 (%) */
    float outMax;      /*!< 输出上限 (%) */
    float deadband;    /*!< 死区，与 SP/PV 同量纲 */
    /* ---- 内部状态 ---- */
    float integral;    /*!< 积分累积 */
    float prevMeas;    /*!< 上次 PV（PV 微分用） */
    float outPrev;     /*!< 上次输出（死区保持 + 无扰切换） */
    uint8_t primed;    /*!< 首次计算标志（防微分冲击） */
} PID_Controller_t;

/**
  * @brief  初始化 PID 实例
  * @param  pid       实例指针
  * @param  outMin    输出下限 (%)，如阀门死区下沿 35.0
  * @param  outMax    输出上限 (%)，如主回路分流临界 70.0
  * @param  deadband  死区（与 SP/PV 同量纲，如 0.1 mL/min）
  */
void PID_Init(PID_Controller_t *pid, float outMin, float outMax, float deadband);

/**
  * @brief  复位 PID 内部状态（用于手动→自动无扰切换）
  * @param  pid            实例指针
  * @param  presetOutput   预置输出（通常为切换瞬间的手动开度）
  * @note   将积分项置为 presetOutput，使切换后首次输出 ≈ presetOutput
  *         （当误差较小、PV 不突变时）。同时清除首次计算标志。
  */
void PID_Reset(PID_Controller_t *pid, float presetOutput);

/**
  * @brief  PID 单步计算
  * @param  pid          实例指针
  * @param  kp           比例增益（在线整定，来自 rsvd_param.PID_P）
  * @param  ki           积分增益（在线整定，来自 rsvd_param.PID_I）
  * @param  kd           微分增益（在线整定，来自 rsvd_param.PID_D）
  * @param  setpoint     设定值 SP
  * @param  measurement  测量值 PV
  * @param  dt           控制周期 (s)，必须 > 0
  * @retval 控制输出 MV（已限幅至 [outMin, outMax]）
  * @note   误差落入死区时维持上次输出，不更新内部状态，避免阀门抖动。
  */
float PID_Compute(PID_Controller_t *pid, float kp, float ki, float kd,
                  float setpoint, float measurement, float dt);

#endif /* __PID_H */
