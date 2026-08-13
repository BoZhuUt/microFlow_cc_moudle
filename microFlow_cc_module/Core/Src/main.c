/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "cmsis_os.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "modbus.h"
#include "app.h"
#include "pid.h"
#include <string.h>
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

#define FLOW_ADC_VREF_V          3.3f
#define FLOW_ADC_FULL_SCALE      4095.0f
#define FLOW_ADC_ERROR_VALUE     (-1.0f)

/* ---- 恒流闭环参数(依据附录A实测整定) ---- */
#define FLOW_TASK_PERIOD_MS      100U                            /* 控制周期 ms */
#define FLOW_TASK_DT_S           (FLOW_TASK_PERIOD_MS / 1000.0f) /* 控制周期 s */
#define FLOW_ADC_SAMPLE_COUNT    20U     /* ADC中值滤波固定采样次数 */
#define FLOW_PID_OUT_MIN         35.0f   /* 阀门死区下沿(<35%不开) */
#define FLOW_PID_OUT_MAX         99.0f   /* PID输出上限%(放开, 原70基于主回路分流分析) */
#define FLOW_PID_DEADBAND        0.1f    /* 死区 mL/min, 抑制阀门抖动 */
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
ADC_HandleTypeDef hadc1;

TIM_HandleTypeDef htim1;
TIM_HandleTypeDef htim7;
TIM_HandleTypeDef htim16;

UART_HandleTypeDef huart1;
UART_HandleTypeDef huart2;

osThreadId defaultTaskHandle;
osThreadId atuoFlowTaskHandle;
osThreadId modbusTaskHandle;
/* USER CODE BEGIN PV */

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_USART1_UART_Init(void);
static void MX_USART2_UART_Init(void);
static void MX_TIM7_Init(void);
static void MX_TIM16_Init(void);
static void MX_ADC1_Init(void);
static void MX_TIM1_Init(void);
void StartDefaultTask(void const * argument);
void StartFlowTask(void const * argument);
void StartModbusTask(void const * argument);

/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{

  /* USER CODE BEGIN 1 */

  /* USER CODE END 1 */

  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_USART1_UART_Init();
  MX_USART2_UART_Init();
  MX_TIM7_Init();
  MX_TIM16_Init();
  MX_ADC1_Init();
  MX_TIM1_Init();
  /* USER CODE BEGIN 2 */
  if (HAL_ADCEx_Calibration_Start(&hadc1, ADC_SINGLE_ENDED) != HAL_OK)
  {
    Error_Handler();
  }

  __HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_1, 0U);
  if (HAL_TIM_PWM_Start(&htim1, TIM_CHANNEL_1) != HAL_OK)
  {
    Error_Handler();
  }

  Modbus_Init();
  /* USER CODE END 2 */

  /* USER CODE BEGIN RTOS_MUTEX */
  /* add mutexes, ... */
  /* USER CODE END RTOS_MUTEX */

  /* USER CODE BEGIN RTOS_SEMAPHORES */
  /* add semaphores, ... */
  /* USER CODE END RTOS_SEMAPHORES */

  /* USER CODE BEGIN RTOS_TIMERS */
  /* start timers, add new ones, ... */
  /* USER CODE END RTOS_TIMERS */

  /* USER CODE BEGIN RTOS_QUEUES */
  /* add queues, ... */
  /* USER CODE END RTOS_QUEUES */

  /* Create the thread(s) */
  /* definition and creation of defaultTask */
  osThreadDef(defaultTask, StartDefaultTask, osPriorityNormal, 0, 128);
  defaultTaskHandle = osThreadCreate(osThread(defaultTask), NULL);

  /* definition and creation of atuoFlowTask */
  osThreadDef(atuoFlowTask, StartFlowTask, osPriorityIdle, 0, 256);
  atuoFlowTaskHandle = osThreadCreate(osThread(atuoFlowTask), NULL);

  /* definition and creation of modbusTask */
  osThreadDef(modbusTask, StartModbusTask, osPriorityIdle, 0, 256);
  modbusTaskHandle = osThreadCreate(osThread(modbusTask), NULL);

  /* USER CODE BEGIN RTOS_THREADS */

  /* USER CODE END RTOS_THREADS */

  /* Start scheduler */
  osKernelStart();

  /* We should never get here as control is now taken by the scheduler */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
  }
  /* USER CODE END 3 */
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  /** Configure the main internal regulator output voltage
  */
  if (HAL_PWREx_ControlVoltageScaling(PWR_REGULATOR_VOLTAGE_SCALE1) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_BYPASS;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLM = 1;
  RCC_OscInitStruct.PLL.PLLN = 20;
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV7;
  RCC_OscInitStruct.PLL.PLLQ = RCC_PLLQ_DIV2;
  RCC_OscInitStruct.PLL.PLLR = RCC_PLLR_DIV2;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_4) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * @brief ADC1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_ADC1_Init(void)
{

  /* USER CODE BEGIN ADC1_Init 0 */

  /* USER CODE END ADC1_Init 0 */

  ADC_ChannelConfTypeDef sConfig = {0};

  /* USER CODE BEGIN ADC1_Init 1 */

  /* USER CODE END ADC1_Init 1 */

  /** Common config
  */
  hadc1.Instance = ADC1;
  hadc1.Init.ClockPrescaler = ADC_CLOCK_ASYNC_DIV1;
  hadc1.Init.Resolution = ADC_RESOLUTION_12B;
  hadc1.Init.DataAlign = ADC_DATAALIGN_RIGHT;
  hadc1.Init.ScanConvMode = ADC_SCAN_DISABLE;
  hadc1.Init.EOCSelection = ADC_EOC_SINGLE_CONV;
  hadc1.Init.LowPowerAutoWait = DISABLE;
  hadc1.Init.ContinuousConvMode = DISABLE;
  hadc1.Init.NbrOfConversion = 1;
  hadc1.Init.DiscontinuousConvMode = DISABLE;
  hadc1.Init.ExternalTrigConv = ADC_SOFTWARE_START;
  hadc1.Init.ExternalTrigConvEdge = ADC_EXTERNALTRIGCONVEDGE_NONE;
  hadc1.Init.DMAContinuousRequests = DISABLE;
  hadc1.Init.Overrun = ADC_OVR_DATA_PRESERVED;
  hadc1.Init.OversamplingMode = DISABLE;
  if (HAL_ADC_Init(&hadc1) != HAL_OK)
  {
    Error_Handler();
  }

  /** Configure Regular Channel
  */
  sConfig.Channel = ADC_CHANNEL_6;
  sConfig.Rank = ADC_REGULAR_RANK_1;
  sConfig.SamplingTime = ADC_SAMPLETIME_2CYCLES_5;
  sConfig.SingleDiff = ADC_SINGLE_ENDED;
  sConfig.OffsetNumber = ADC_OFFSET_NONE;
  sConfig.Offset = 0;
  if (HAL_ADC_ConfigChannel(&hadc1, &sConfig) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN ADC1_Init 2 */

  /* USER CODE END ADC1_Init 2 */

}

/**
  * @brief TIM1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_TIM1_Init(void)
{

  /* USER CODE BEGIN TIM1_Init 0 */

  /* USER CODE END TIM1_Init 0 */

  TIM_MasterConfigTypeDef sMasterConfig = {0};
  TIM_OC_InitTypeDef sConfigOC = {0};
  TIM_BreakDeadTimeConfigTypeDef sBreakDeadTimeConfig = {0};

  /* USER CODE BEGIN TIM1_Init 1 */

  /* USER CODE END TIM1_Init 1 */
  htim1.Instance = TIM1;
  htim1.Init.Prescaler = 1;
  htim1.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim1.Init.Period = 39999;
  htim1.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  htim1.Init.RepetitionCounter = 0;
  htim1.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_ENABLE;
  if (HAL_TIM_PWM_Init(&htim1) != HAL_OK)
  {
    Error_Handler();
  }
  sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
  sMasterConfig.MasterOutputTrigger2 = TIM_TRGO2_RESET;
  sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
  if (HAL_TIMEx_MasterConfigSynchronization(&htim1, &sMasterConfig) != HAL_OK)
  {
    Error_Handler();
  }
  sConfigOC.OCMode = TIM_OCMODE_PWM1;
  sConfigOC.Pulse = 0;
  sConfigOC.OCPolarity = TIM_OCPOLARITY_HIGH;
  sConfigOC.OCNPolarity = TIM_OCNPOLARITY_HIGH;
  sConfigOC.OCFastMode = TIM_OCFAST_DISABLE;
  sConfigOC.OCIdleState = TIM_OCIDLESTATE_RESET;
  sConfigOC.OCNIdleState = TIM_OCNIDLESTATE_RESET;
  if (HAL_TIM_PWM_ConfigChannel(&htim1, &sConfigOC, TIM_CHANNEL_1) != HAL_OK)
  {
    Error_Handler();
  }
  sBreakDeadTimeConfig.OffStateRunMode = TIM_OSSR_DISABLE;
  sBreakDeadTimeConfig.OffStateIDLEMode = TIM_OSSI_DISABLE;
  sBreakDeadTimeConfig.LockLevel = TIM_LOCKLEVEL_OFF;
  sBreakDeadTimeConfig.DeadTime = 0;
  sBreakDeadTimeConfig.BreakState = TIM_BREAK_DISABLE;
  sBreakDeadTimeConfig.BreakPolarity = TIM_BREAKPOLARITY_HIGH;
  sBreakDeadTimeConfig.BreakFilter = 0;
  sBreakDeadTimeConfig.Break2State = TIM_BREAK2_DISABLE;
  sBreakDeadTimeConfig.Break2Polarity = TIM_BREAK2POLARITY_HIGH;
  sBreakDeadTimeConfig.Break2Filter = 0;
  sBreakDeadTimeConfig.AutomaticOutput = TIM_AUTOMATICOUTPUT_DISABLE;
  if (HAL_TIMEx_ConfigBreakDeadTime(&htim1, &sBreakDeadTimeConfig) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN TIM1_Init 2 */

  /* USER CODE END TIM1_Init 2 */
  HAL_TIM_MspPostInit(&htim1);

}

/**
  * @brief TIM7 Initialization Function
  * @param None
  * @retval None
  */
static void MX_TIM7_Init(void)
{

  /* USER CODE BEGIN TIM7_Init 0 */

  /* USER CODE END TIM7_Init 0 */

  TIM_MasterConfigTypeDef sMasterConfig = {0};

  /* USER CODE BEGIN TIM7_Init 1 */

  /* USER CODE END TIM7_Init 1 */
  htim7.Instance = TIM7;
  htim7.Init.Prescaler = 4000-1;
  htim7.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim7.Init.Period = 80-1;
  htim7.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
  if (HAL_TIM_Base_Init(&htim7) != HAL_OK)
  {
    Error_Handler();
  }
  sMasterConfig.MasterOutputTrigger = TIM_TRGO_UPDATE;
  sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
  if (HAL_TIMEx_MasterConfigSynchronization(&htim7, &sMasterConfig) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN TIM7_Init 2 */

  /* USER CODE END TIM7_Init 2 */

}

/**
  * @brief TIM16 Initialization Function
  * @param None
  * @retval None
  */
static void MX_TIM16_Init(void)
{

  /* USER CODE BEGIN TIM16_Init 0 */

  /* USER CODE END TIM16_Init 0 */

  /* USER CODE BEGIN TIM16_Init 1 */

  /* USER CODE END TIM16_Init 1 */
  htim16.Instance = TIM16;
  htim16.Init.Prescaler = 8000-1;
  htim16.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim16.Init.Period = 1000;
  htim16.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  htim16.Init.RepetitionCounter = 0;
  htim16.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_ENABLE;
  if (HAL_TIM_Base_Init(&htim16) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN TIM16_Init 2 */

  /* USER CODE END TIM16_Init 2 */

}

/**
  * @brief USART1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_USART1_UART_Init(void)
{

  /* USER CODE BEGIN USART1_Init 0 */

  /* USER CODE END USART1_Init 0 */

  /* USER CODE BEGIN USART1_Init 1 */

  /* USER CODE END USART1_Init 1 */
  huart1.Instance = USART1;
  huart1.Init.BaudRate = 115200;
  huart1.Init.WordLength = UART_WORDLENGTH_8B;
  huart1.Init.StopBits = UART_STOPBITS_1;
  huart1.Init.Parity = UART_PARITY_NONE;
  huart1.Init.Mode = UART_MODE_TX_RX;
  huart1.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart1.Init.OverSampling = UART_OVERSAMPLING_16;
  huart1.Init.OneBitSampling = UART_ONE_BIT_SAMPLE_DISABLE;
  huart1.AdvancedInit.AdvFeatureInit = UART_ADVFEATURE_NO_INIT;
  if (HAL_RS485Ex_Init(&huart1, UART_DE_POLARITY_HIGH, 0, 0) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN USART1_Init 2 */

  /* USER CODE END USART1_Init 2 */

}

/**
  * @brief USART2 Initialization Function
  * @param None
  * @retval None
  */
static void MX_USART2_UART_Init(void)
{

  /* USER CODE BEGIN USART2_Init 0 */

  /* USER CODE END USART2_Init 0 */

  /* USER CODE BEGIN USART2_Init 1 */

  /* USER CODE END USART2_Init 1 */
  huart2.Instance = USART2;
  huart2.Init.BaudRate = 115200;
  huart2.Init.WordLength = UART_WORDLENGTH_8B;
  huart2.Init.StopBits = UART_STOPBITS_1;
  huart2.Init.Parity = UART_PARITY_NONE;
  huart2.Init.Mode = UART_MODE_TX_RX;
  huart2.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart2.Init.OverSampling = UART_OVERSAMPLING_16;
  huart2.Init.OneBitSampling = UART_ONE_BIT_SAMPLE_DISABLE;
  huart2.AdvancedInit.AdvFeatureInit = UART_ADVFEATURE_NO_INIT;
  if (HAL_UART_Init(&huart2) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN USART2_Init 2 */

  /* USER CODE END USART2_Init 2 */

}

/**
  * @brief GPIO Initialization Function
  * @param None
  * @retval None
  */
static void MX_GPIO_Init(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};
  /* USER CODE BEGIN MX_GPIO_Init_1 */

  /* USER CODE END MX_GPIO_Init_1 */

  /* GPIO Ports Clock Enable */
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOA, GPIO_PIN_6, GPIO_PIN_RESET);

  /*Configure GPIO pin : PA6 */
  GPIO_InitStruct.Pin = GPIO_PIN_6;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  /*Configure GPIO pins : PB6 PB7 */
  GPIO_InitStruct.Pin = GPIO_PIN_6|GPIO_PIN_7;
  GPIO_InitStruct.Mode = GPIO_MODE_ANALOG;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

  /* USER CODE BEGIN MX_GPIO_Init_2 */

  /* USER CODE END MX_GPIO_Init_2 */
}

/* USER CODE BEGIN 4 */

/**
  * @author Bo
  * @date   Created: 2026-08-10
  * @date   Modified: 2026-08-10
  * @brief  Read one sample from the analog flow-meter input.
  * @param  raw Pointer used to return the 12-bit ADC result.
  * @retval HAL status.
  */
HAL_StatusTypeDef FlowAdc_ReadRaw(uint16_t *raw)
{
  HAL_StatusTypeDef status;
  HAL_StatusTypeDef stopStatus;

  if (raw == NULL)
  {
    return HAL_ERROR;
  }

  status = HAL_ADC_Start(&hadc1);
  if (status != HAL_OK)
  {
    return status;
  }

  status = HAL_ADC_PollForConversion(&hadc1, 10U);
  if (status == HAL_OK)
  {
    *raw = (uint16_t)HAL_ADC_GetValue(&hadc1);
  }

  stopStatus = HAL_ADC_Stop(&hadc1);
  if (status == HAL_OK)
  {
    status = stopStatus;
  }

  return status;
}

/**
  * @author Bo
  * @date   Created: 2026-08-10
  * @date   Modified: 2026-08-10
  * @brief  Read and average multiple ADC samples.
  * @param  num Number of samples to average; must be greater than zero.
  * @retval Average ADC code, or FLOW_ADC_ERROR_VALUE on failure.
  */
float getAdcAverage(uint16_t num)
{
  uint32_t sum = 0U;
  uint16_t raw;
  uint16_t index;

  if (num == 0U)
  {
    return FLOW_ADC_ERROR_VALUE;
  }

  for (index = 0U; index < num; index++)
  {
    if (FlowAdc_ReadRaw(&raw) != HAL_OK)
    {
      return FLOW_ADC_ERROR_VALUE;
    }

    sum += raw;
  }

  return (float)sum / (float)num;
}

/**
  * @author Bo
  * @date   Created: 2026-08-10
  * @date   Modified: 2026-08-10
  * @brief  Read the averaged voltage of the analog flow-meter input.
  * @param  num Number of ADC samples to average.
  * @retval Input voltage in volts, or FLOW_ADC_ERROR_VALUE on failure.
  */
float getAdcVoltage(uint16_t num)
{
  float adcAverage = getAdcAverage(num);

  if (adcAverage < 0.0f)
  {
    return FLOW_ADC_ERROR_VALUE;
  }

  return adcAverage * FLOW_ADC_VREF_V / FLOW_ADC_FULL_SCALE;
}

/**
  * @author Bo
  * @date   Created: 2026-08-11
  * @brief  读取流量计电压(固定20次采样 + 修剪均值滤波)
  * @retval 输入电压(V), 或 FLOW_ADC_ERROR_VALUE
  * @note   去1个最大1个最小后求平均, 比单纯均值更抗脉冲干扰;
  *         采样次数由 FLOW_ADC_SAMPLE_COUNT 固定, 不受寄存器控制
  */
float getAdcVoltageMedian(void)
{
  uint32_t sum = 0U;
  uint16_t maxv = 0U;
  uint16_t minv = 0xFFFFU;
  uint16_t raw;
  uint16_t i;

  for (i = 0U; i < FLOW_ADC_SAMPLE_COUNT; i++)
  {
    if (FlowAdc_ReadRaw(&raw) != HAL_OK)
    {
      return FLOW_ADC_ERROR_VALUE;
    }
    sum += raw;
    if (raw > maxv)
    {
      maxv = raw;
    }
    if (raw < minv)
    {
      minv = raw;
    }
  }

  /* 修剪均值: 去最大最小后平均剩余 (FLOW_ADC_SAMPLE_COUNT-2) 个 */
  sum = sum - (uint32_t)maxv - (uint32_t)minv;
  return ((float)sum / (float)(FLOW_ADC_SAMPLE_COUNT - 2U)) *
         FLOW_ADC_VREF_V / FLOW_ADC_FULL_SCALE;
}

/**
  * @author Bo
  * @date   Created: 2026-08-10
  * @date   Modified: 2026-08-10
  * @brief  Convert the averaged ADC voltage to flow using two-point scaling.
  * @param  num Number of ADC samples to average.
  * @param  flowZero Flow value at the zero-point voltage.
  * @param  flowFullScale Flow value at the full-scale voltage.
  * @param  voltageZero Voltage corresponding to flowZero.
  * @param  voltageFullScale Voltage corresponding to flowFullScale.
  * @retval Converted flow, or FLOW_ADC_ERROR_VALUE on failure.
  * @note   The result is not clamped to the configured flow range.
  */
float getFlow(uint16_t num, float flowZero, float flowFullScale,
              float voltageZero, float voltageFullScale)
{
  float voltage;

  if (voltageFullScale == voltageZero)
  {
    return FLOW_ADC_ERROR_VALUE;
  }

  voltage = getAdcVoltage(num);
  if (voltage < 0.0f)
  {
    return FLOW_ADC_ERROR_VALUE;
  }

  return flowZero + (voltage - voltageZero) *
         (flowFullScale - flowZero) /
         (voltageFullScale - voltageZero);
}

/**
  * @author Bo
  * @date   Created: 2026-08-10
  * @date   Modified: 2026-08-10
  * @brief  Set the valve PWM duty cycle on PA8/TIM1_CH1.
  * @param  dutyPercent Requested duty cycle in percent.
  * @retval None.
  * @note   Values outside 0...100 percent are clamped to that range.
  */
void Valve_SetDuty(float dutyPercent)
{
  uint32_t period;
  uint32_t compare;

  if (dutyPercent < 0.0f)
  {
    dutyPercent = 0.0f;
  }
  else if (dutyPercent > 100.0f)
  {
    dutyPercent = 100.0f;
  }

  period = __HAL_TIM_GET_AUTORELOAD(&htim1) + 1U;
  compare = (uint32_t)(((float)period * dutyPercent / 100.0f) + 0.5f);

  if (compare > period)
  {
    compare = period;
  }

  __HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_1, compare);
}

/* ======================== 恒流闭环(StartFlowTask拆分) ======================== */
/* 文件级状态: 避免向子函数传参, 任务体保持简洁 */
static PID_Controller_t s_flowPid;           /* PID实例(Kp/Ki/Kd在线传入) */
static uint16_t s_prevManualMode = 1U;       /* 模式切换边沿(上电默认手动) */
static float s_voltFiltered = 0.0f;          /* 一阶低通后的电压 */
static uint8_t s_voltFilterInit = 0U;        /* 电压滤波器首次初始化标志 */

/**
  * @brief  采样流量计: 固定20次中值滤波 + filterFactor一阶低通 + 两点标定
  * @retval 滤波后流量 PV; <0 表示 ADC 失败(调用方应保持上次输出)
  * @note   filterFactor(寄存器48005, 0~1) 关联电压/流量/自动模式滤波三者:
  *         电压经一阶低通->flowRateVoltageAve; 流量由滤波电压换算->flowRateAve(PV)
  */
static float FlowSensor_Sample(void)
{
  float voltageRaw;
  float alpha;

  /* 固定20次修剪均值滤波(去最大最小), 抗脉冲干扰 */
  voltageRaw = getAdcVoltageMedian();
  if (voltageRaw < 0.0f)
  {
    return FLOW_ADC_ERROR_VALUE;            /* ADC失败: 通知调用方保持上次输出 */
  }

  /* filterFactor 一阶低通系数(寄存器48005, 范围0~1); 越界保护为无滤波 */
  alpha = rsvd_param.filterFactor;
  if (alpha <= 0.0f || alpha >= 1.0f)
  {
    alpha = 1.0f;
  }

  /* 电压一阶低通 -> flowRateVoltageAve (首次直接赋值, 避免从0慢爬) */
  if (s_voltFilterInit == 0U)
  {
    s_voltFiltered = voltageRaw;
    s_voltFilterInit = 1U;
  }
  else
  {
    s_voltFiltered = alpha * voltageRaw + (1.0f - alpha) * s_voltFiltered;
  }
  rsvd_param.flowRateVoltageAve = s_voltFiltered;

  /* 流量由滤波电压换算(与电压同源, 保证一致) -> flowRateAve (PID的PV) */
  if (rsvd_param.highFlowVoltage != rsvd_param.lowFlowVoltage)
  {
    rsvd_param.flowRateAve = rsvd_param.lowFlow +
              (s_voltFiltered - rsvd_param.lowFlowVoltage) *
              (rsvd_param.highFlow - rsvd_param.lowFlow) /
              (rsvd_param.highFlowVoltage - rsvd_param.lowFlowVoltage);
  }
  else
  {
    rsvd_param.flowRateAve = 0.0f;
  }
  return rsvd_param.flowRateAve;
}

/**
  * @brief  流量闭环控制: 模式切换无扰 + 手动/PID控制律 + PWM输出
  * @param  pv 滤波后流量; <0 时保持上次开度后返回
  */
static void FlowControl_Run(float pv)
{
  float duty;

  /* ADC失败: 保持上次开度, 不归零, 避免流量突冲 */
  if (pv < 0.0f)
  {
    Valve_SetDuty(rsvd_param.valveOpening);
    return;
  }

  /* 模式切换边沿 -> 无扰切换 */
  if (rsvd_param.manualMode != s_prevManualMode)
  {
    if (rsvd_param.manualMode == 0U)
    {
      PID_Reset(&s_flowPid, rsvd_param.valveOpening);  /* 手动->自动: 预置积分, 无跳变 */
    }
    s_prevManualMode = rsvd_param.manualMode;
  }

  /* 控制律 */
  if (rsvd_param.manualMode == 1U)
  {
    duty = rsvd_param.valveOpening;         /* 手动: 直接用寄存器设定开度 */
  }
  else
  {
    duty = PID_Compute(&s_flowPid,          /* 自动: PID闭环, 参数在线整定 */
                       rsvd_param.PID_P, rsvd_param.PID_I, rsvd_param.PID_D,
                       rsvd_param.flowRateSet, pv, FLOW_TASK_DT_S);
    rsvd_param.valveOpening = duty;         /* 回写, 便于监视/手动衔接 */
  }

  Valve_SetDuty(duty);
}

/* USER CODE END 4 */

/* USER CODE BEGIN Header_StartDefaultTask */
/**
  * @brief  Function implementing the defaultTask thread.
  * @param  argument: Not used
  * @retval None
  */
/* USER CODE END Header_StartDefaultTask */
void StartDefaultTask(void const * argument)
{
  /* USER CODE BEGIN 5 */
  /* Infinite loop */
  for(;;)
  {
    osDelay(100);
  }
  /* USER CODE END 5 */
}

/* USER CODE BEGIN Header_StartFlowTask */
/**
* @brief Function implementing the atuoFlowTask thread.
* @param argument: Not used
* @retval None
*/
/* USER CODE END Header_StartFlowTask */
void StartFlowTask(void const * argument)
{
  /* USER CODE BEGIN StartFlowTask */
  float pv;

  /* 初始化PID: 仅限幅/死区配置; Kp/Ki/Kd每次从寄存器读取 */
  PID_Init(&s_flowPid, FLOW_PID_OUT_MIN, FLOW_PID_OUT_MAX, FLOW_PID_DEADBAND);

  /* Infinite loop */
  for(;;)
  {
    pv = FlowSensor_Sample();    /* 采样+标定+低通, 更新 flowRateAve/VoltageAve */
    FlowControl_Run(pv);         /* 模式切换无扰 + 手动/PID控制律 + PWM输出 */
    osDelay(FLOW_TASK_PERIOD_MS);
  }
  /* USER CODE END StartFlowTask */
}

/* USER CODE BEGIN Header_StartModbusTask */
/**
* @brief Function implementing the modbusTask thread.
* @param argument: Not used
* @retval None
*/
/* USER CODE END Header_StartModbusTask */
void StartModbusTask(void const * argument)
{
  /* USER CODE BEGIN StartModbusTask */
  (void)argument;

  /* Infinite loop - Modbus RTU 协议轮询 + 命令处理 */
  for(;;)
  {
    eMBPoll();
    MB_PortAfterPoll();
    MeasureFunc();        /* autoSavePending 检测 + 命令分发 */
    osDelay(1);
  }
  /* USER CODE END StartModbusTask */
}

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */
  __disable_irq();
  while (1)
  {
  }
  /* USER CODE END Error_Handler_Debug */
}
#ifdef USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
