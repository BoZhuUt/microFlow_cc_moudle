/**
  ******************************************************************************
  * @file    pid.c
  * @brief   通用位置式 PID 控制器实现
  ******************************************************************************
  * @attention
  *   特性：抗积分饱和(clamping) + PV 微分 + 死区 + 无扰切换。
  *   Kp/Ki/Kd 作为 PID_Compute 参数传入，支持在线整定。
  ******************************************************************************
  */
#include "pid.h"
#include <stddef.h>

/**
  * @brief  初始化 PID 实例
  */
void PID_Init(PID_Controller_t *pid, float outMin, float outMax, float deadband)
{
    if (pid == NULL)
    {
        return;
    }

    pid->outMin   = outMin;
    pid->outMax   = outMax;
    pid->deadband = deadband;
    pid->integral = 0.0f;
    pid->prevMeas = 0.0f;
    pid->outPrev  = outMin;     /* 默认从下限起步 */
    pid->primed   = 0U;
}

/**
  * @brief  复位 PID 内部状态（无扰切换用）
  */
void PID_Reset(PID_Controller_t *pid, float presetOutput)
{
    if (pid == NULL)
    {
        return;
    }

    /* 限幅到合法区间 */
    if (presetOutput > pid->outMax)
    {
        presetOutput = pid->outMax;
    }
    if (presetOutput < pid->outMin)
    {
        presetOutput = pid->outMin;
    }

    pid->integral = presetOutput;  /* 使首次 p+i+d ≈ presetOutput */
    pid->prevMeas = 0.0f;
    pid->outPrev  = presetOutput;
    pid->primed   = 0U;            /* 重新初始化 prevMeas */
}

/**
  * @brief  PID 单步计算（位置式 + 抗积分饱和 + PV 微分 + 死区）
  */
float PID_Compute(PID_Controller_t *pid, float kp, float ki, float kd,
                  float setpoint, float measurement, float dt)
{
    float error;
    float p;
    float iCandidate;
    float d;
    float out;

    if (pid == NULL)
    {
        return 0.0f;
    }

    /* dt 保护：非正则维持上次输出 */
    if (dt <= 0.0f)
    {
        return pid->outPrev;
    }

    error = setpoint - measurement;

    /* 死区：误差很小则维持上次输出，不更新内部状态 */
    if ((error < pid->deadband) && (error > -pid->deadband))
    {
        return pid->outPrev;
    }

    /* 首次计算：初始化 prevMeas，避免微分项冲击 */
    if (pid->primed == 0U)
    {
        pid->prevMeas = measurement;
        pid->primed   = 1U;
    }

    /* P 比例 */
    p = kp * error;

    /* I 积分（前向欧拉，先试探性累积，抗饱和判定决定是否提交） */
    iCandidate = pid->integral + ki * error * dt;

    /* D 微分（基于 PV 变化，符号取负：PV 上升时压低输出，抑制超调） */
    d = -kd * (measurement - pid->prevMeas) / dt;

    /* 试探输出 */
    out = p + iCandidate + d;

    /* 限幅 + 抗积分饱和：触限则不提交积分（保留原 integral），避免深饱和 */
    if (out > pid->outMax)
    {
        out = pid->outMax;
    }
    else if (out < pid->outMin)
    {
        out = pid->outMin;
    }
    else
    {
        pid->integral = iCandidate;   /* 未触限才提交 */
    }

    pid->prevMeas = measurement;
    pid->outPrev  = out;
    return out;
}
