#include "main.h"

/* Handles */
TIM_HandleTypeDef htim3; // Servo PWM
TIM_HandleTypeDef htim2; // us delay / echo timing

/* Prototypes */
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_TIM3_Init(void);
static void MX_TIM2_Init(void);

/* ===== Microsecond delay using TIM2 ===== */
static void delay_us(uint16_t us)
{
  __HAL_TIM_SET_COUNTER(&htim2, 0);
  while (__HAL_TIM_GET_COUNTER(&htim2) < us);
}

/* ===== Read distance (cm) from HC-SR04 ===== */
static float HCSR04_Read(void)
{
  uint32_t t = 0;

  /* Ensure TRIG low */
  HAL_GPIO_WritePin(GPIOB, GPIO_PIN_5, GPIO_PIN_RESET);
  delay_us(2);

  /* 10 us trigger pulse */
  HAL_GPIO_WritePin(GPIOB, GPIO_PIN_5, GPIO_PIN_SET);
  delay_us(10);
  HAL_GPIO_WritePin(GPIOB, GPIO_PIN_5, GPIO_PIN_RESET);

  /* Wait for ECHO to go HIGH (with timeout) */
  uint32_t timeout = 30000; // ~30 ms
  while (HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_8) == GPIO_PIN_RESET)
  {
    if (--timeout == 0) return -1.0f; // no echo
  }

  /* Start timing */
  __HAL_TIM_SET_COUNTER(&htim2, 0);

  /* Measure HIGH time (with timeout) */
  timeout = 60000; // safety
  while (HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_8) == GPIO_PIN_SET)
  {
    t = __HAL_TIM_GET_COUNTER(&htim2);
    if (--timeout == 0) break;
  }

  /* Convert to distance: cm = (time_us * 0.0343) / 2 */
  return (t * 0.0343f) / 2.0f;
}

/* ===== Set servo pulse in microseconds (1000–2000 us typical) ===== */
static void Servo_SetUS(uint16_t us)
{
  if (us < 500) us = 500;
  if (us > 2500) us = 2500;
  __HAL_TIM_SET_COMPARE(&htim3, TIM_CHANNEL_2, us);
}

int main(void)
{
  HAL_Init();
  SystemClock_Config();

  MX_GPIO_Init();
  MX_TIM2_Init();
  MX_TIM3_Init();

  HAL_TIM_Base_Start(&htim2);                    // start TIM2 for us timing
  HAL_TIM_PWM_Start(&htim3, TIM_CHANNEL_2);      // start servo PWM

  /* Initial: door closed */
  Servo_SetUS(1000);

  while (1)
  {
    float d = HCSR04_Read();

    if (d > 0 && d < 20.0f)   // object within 20 cm
    {
      /* Open door */
      Servo_SetUS(2000);

      /* Wait until object leaves (like real doors) */
      while (1)
      {
        float d2 = HCSR04_Read();
        if (d2 < 0) break;          // sensor timeout, break safe
        if (d2 > 25.0f) break;      // hysteresis (leave threshold)
        HAL_Delay(50);
      }

      HAL_Delay(800);               // small grace delay

      /* Close door */
      Servo_SetUS(1000);
    }

    HAL_Delay(100);
  }
}

/* ================= GPIO ================= */
static void MX_GPIO_Init(void)
{
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();
  __HAL_RCC_GPIOC_CLK_ENABLE();

  GPIO_InitTypeDef GPIO_InitStruct = {0};

  /* PB5 -> TRIG (Output) */
  GPIO_InitStruct.Pin = GPIO_PIN_5;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

  /* PA8 -> ECHO (Input) */
  GPIO_InitStruct.Pin = GPIO_PIN_8;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  /* PC7 -> TIM3 CH2 (Servo PWM) configured in TIM init */
}

/* ================= TIM3: Servo PWM (50 Hz) ================= */
static void MX_TIM3_Init(void)
{
  __HAL_RCC_TIM3_CLK_ENABLE();

  TIM_OC_InitTypeDef sConfigOC = {0};

  htim3.Instance = TIM3;
  htim3.Init.Prescaler = 83;       // 84 MHz / (83+1) = 1 MHz (1 us tick)
  htim3.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim3.Init.Period = 20000 - 1;   // 20 ms period (50 Hz)
  htim3.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;

  HAL_TIM_PWM_Init(&htim3);

  sConfigOC.OCMode = TIM_OCMODE_PWM1;
  sConfigOC.Pulse = 1000;          // start closed
  sConfigOC.OCPolarity = TIM_OCPOLARITY_HIGH;
  sConfigOC.OCFastMode = TIM_OCFAST_DISABLE;

  HAL_TIM_PWM_ConfigChannel(&htim3, &sConfigOC, TIM_CHANNEL_2);

  /* GPIO for PC7 (AF2 for TIM3) */
  GPIO_InitTypeDef GPIO_InitStruct = {0};
  GPIO_InitStruct.Pin = GPIO_PIN_7;
  GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  GPIO_InitStruct.Alternate = GPIO_AF2_TIM3;
  HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);
}

/* ================= TIM2: 1 MHz base for us timing ================= */
static void MX_TIM2_Init(void)
{
  __HAL_RCC_TIM2_CLK_ENABLE();

  htim2.Instance = TIM2;
  htim2.Init.Prescaler = 83;        // 1 MHz (1 us per tick)
  htim2.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim2.Init.Period = 0xFFFFFFFF;  // free-running
  htim2.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;

  HAL_TIM_Base_Init(&htim2);
}

/* ================= Clock (HSI -> PLL ~84 MHz) ================= */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  __HAL_RCC_PWR_CLK_ENABLE();
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE2);

  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSI;
  RCC_OscInitStruct.PLL.PLLM = 16;
  RCC_OscInitStruct.PLL.PLLN = 336;
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV4; // ~84 MHz
  RCC_OscInitStruct.PLL.PLLQ = 7;

  HAL_RCC_OscConfig(&RCC_OscInitStruct);

  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK
                             | RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2);
}
