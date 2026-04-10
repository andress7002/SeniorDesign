/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body - DEBUG VERSION (no OLED)
  ******************************************************************************
  */
/* USER CODE END Header */

#include "main.h"
/* USER CODE BEGIN Includes */
#include <stdio.h>
/* USER CODE END Includes */

/* Private variables ---------------------------------------------------------*/
ADC_HandleTypeDef hadc1;
I2C_HandleTypeDef hi2c1;

/* USER CODE BEGIN PV */
static uint16_t raw_ch1    = 0;
static uint16_t raw_ch2    = 0;
static float    voltage_ch1 = 0.0f;
static float    voltage_ch2 = 0.0f;
static float    temp_ch1    = 0.0f;

#define STM32_SENSOR_1_CHANNEL  ADC_CHANNEL_TEMPSENSOR
#define STM32_SENSOR_2_CHANNEL  ADC_CHANNEL_1
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_ADC1_Init(void);
static void MX_I2C1_Init(void);

/* USER CODE BEGIN PFP */
static uint8_t  STM32_ADC_SelectChannel(uint32_t channel);
static uint16_t STM32_ADC_ReadRaw(uint32_t channel);
static float    STM32_ADC_RawToVoltage(uint16_t raw);
/* USER CODE END PFP */

/* USER CODE BEGIN 0 */
static uint8_t STM32_ADC_SelectChannel(uint32_t channel)
{
  ADC_ChannelConfTypeDef sConfig = {0};
  sConfig.Channel      = channel;
  sConfig.Rank         = ADC_REGULAR_RANK_1;
  sConfig.SamplingTime = ADC_SAMPLETIME_239CYCLES_5;

  if (HAL_ADC_ConfigChannel(&hadc1, &sConfig) != HAL_OK)
    return 0;

  return 1;
}

static uint16_t STM32_ADC_ReadRaw(uint32_t channel)
{
  if (!STM32_ADC_SelectChannel(channel))
    return 0;

  if (HAL_ADC_Start(&hadc1) != HAL_OK)
    return 0;

  /* Changed from HAL_MAX_DELAY to 100ms so it never hangs forever */
  if (HAL_ADC_PollForConversion(&hadc1, 100) != HAL_OK)
  {
    HAL_ADC_Stop(&hadc1);
    return 0;
  }

  uint16_t raw = (uint16_t)HAL_ADC_GetValue(&hadc1);
  HAL_ADC_Stop(&hadc1);
  return raw;
}

static float STM32_ADC_RawToVoltage(uint16_t raw)
{
  return ((float)raw * 3.3f) / 4095.0f;
}
/* USER CODE END 0 */

int main(void)
{
  /* USER CODE BEGIN 1 */
  /* USER CODE END 1 */

  HAL_Init();
  SystemClock_Config();

  MX_GPIO_Init();
  MX_ADC1_Init();
  MX_I2C1_Init();

  /* USER CODE BEGIN 2 */

  /* OLED init intentionally skipped — no display connected */
  // ssd1306_Init();
  // ssd1306_Fill(Black);
  // ssd1306_SetCursor(10, 20);
  // ssd1306_WriteString("Initializing...", Font_7x10, White);
  // ssd1306_UpdateScreen();
  // HAL_Delay(1000);

  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* USER CODE BEGIN 3 */

    /* Read ADC channels */
    raw_ch1 = STM32_ADC_ReadRaw(STM32_SENSOR_1_CHANNEL);
    raw_ch2 = STM32_ADC_ReadRaw(STM32_SENSOR_2_CHANNEL);

    /* Convert to voltage */
    voltage_ch1 = STM32_ADC_RawToVoltage(raw_ch1);
    voltage_ch2 = STM32_ADC_RawToVoltage(raw_ch2);

    /* Internal temp sensor formula for STM32F103 */
    temp_ch1 = ((1.43f - voltage_ch1) / 0.0043f) + 25.0f;

    /* OLED display update intentionally skipped — no display connected */
    // ssd1306_Fill(Black);
    // ssd1306_SetCursor(20, 0);
    // ssd1306_WriteString("STM32 Sensors", Font_7x10, White);
    // sprintf(display_buf, "Temp: %.1f C", temp_ch1);
    // ssd1306_SetCursor(0, 16);
    // ssd1306_WriteString(display_buf, Font_7x10, White);
    // sprintf(display_buf, "V_ch1: %.3f V", voltage_ch1);
    // ssd1306_SetCursor(0, 32);
    // ssd1306_WriteString(display_buf, Font_7x10, White);
    // sprintf(display_buf, "V_ch2: %.3f V", voltage_ch2);
    // ssd1306_SetCursor(0, 48);
    // ssd1306_WriteString(display_buf, Font_7x10, White);
    // ssd1306_UpdateScreen();

    HAL_Delay(200); /* <<< SET YOUR BREAKPOINT HERE */

    /* USER CODE END 3 */
  }
}

void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};
  RCC_PeriphCLKInitTypeDef PeriphClkInit = {0};

  RCC_OscInitStruct.OscillatorType      = RCC_OSCILLATORTYPE_HSI;
  RCC_OscInitStruct.HSIState            = RCC_HSI_ON;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.PLL.PLLState        = RCC_PLL_NONE;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
    Error_Handler();

  RCC_ClkInitStruct.ClockType      = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK
                                   | RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource   = RCC_SYSCLKSOURCE_HSI;
  RCC_ClkInitStruct.AHBCLKDivider  = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;
  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_0) != HAL_OK)
    Error_Handler();

  PeriphClkInit.PeriphClockSelection = RCC_PERIPHCLK_ADC;
  PeriphClkInit.AdcClockSelection    = RCC_ADCPCLK2_DIV2;
  if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInit) != HAL_OK)
    Error_Handler();
}

static void MX_ADC1_Init(void)
{
  ADC_ChannelConfTypeDef sConfig = {0};

  hadc1.Instance                   = ADC1;
  hadc1.Init.ScanConvMode          = ADC_SCAN_DISABLE;
  hadc1.Init.ContinuousConvMode    = DISABLE;
  hadc1.Init.DiscontinuousConvMode = DISABLE;
  hadc1.Init.ExternalTrigConv      = ADC_SOFTWARE_START;
  hadc1.Init.DataAlign             = ADC_DATAALIGN_RIGHT;
  hadc1.Init.NbrOfConversion       = 1;
  if (HAL_ADC_Init(&hadc1) != HAL_OK)
    Error_Handler();

  /* USER CODE BEGIN ADC1_Init 2 */
  /* Enable internal temp sensor — REQUIRED for ADC_CHANNEL_TEMPSENSOR */
  ADC1->CR2 |= ADC_CR2_TSVREFE;
  /* USER CODE END ADC1_Init 2 */

  sConfig.Channel      = ADC_CHANNEL_0;
  sConfig.Rank         = ADC_REGULAR_RANK_1;
  sConfig.SamplingTime = ADC_SAMPLETIME_239CYCLES_5;
  if (HAL_ADC_ConfigChannel(&hadc1, &sConfig) != HAL_OK)
    Error_Handler();
}

static void MX_I2C1_Init(void)
{
  hi2c1.Instance             = I2C1;
  hi2c1.Init.ClockSpeed      = 100000;
  hi2c1.Init.DutyCycle       = I2C_DUTYCYCLE_2;
  hi2c1.Init.OwnAddress1     = 0;
  hi2c1.Init.AddressingMode  = I2C_ADDRESSINGMODE_7BIT;
  hi2c1.Init.DualAddressMode = I2C_DUALADDRESS_DISABLE;
  hi2c1.Init.OwnAddress2     = 0;
  hi2c1.Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
  hi2c1.Init.NoStretchMode   = I2C_NOSTRETCH_DISABLE;
  if (HAL_I2C_Init(&hi2c1) != HAL_OK)
    Error_Handler();
}

static void MX_GPIO_Init(void)
{
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();
}

void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  __disable_irq();
  while (1) { }
  /* USER CODE END Error_Handler_Debug */
}

#ifdef USE_FULL_ASSERT
void assert_failed(uint8_t *file, uint32_t line) { }
#endif
