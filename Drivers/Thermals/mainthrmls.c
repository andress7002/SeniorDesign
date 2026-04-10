/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : 2-channel NTC thermistor temperature monitor
  *                   PA0 → ADC_CH0 → T1
  *                   PA1 → ADC_CH1 → T2
  *                   Voltage divider: 3.3V → 10k → PA_x → NTC(11k) → GND
  *                   Results displayed on SSD1306 OLED via I2C
  ******************************************************************************
  */
/* USER CODE END Header */

#include "main.h"
/* USER CODE BEGIN Includes */
#include "ssd1306.h"
#include "ssd1306_fonts.h"
#include <stdio.h>
#include <math.h>
/* USER CODE END Includes */

ADC_HandleTypeDef hadc1;
I2C_HandleTypeDef hi2c1;

/* USER CODE BEGIN PV */

/* --- NTC thermistor parameters --- */
#define R_FIXED        10000.0f   /* fixed pull-up resistor: 10k            */
#define R_NOMINAL      11000.0f   /* NTC resistance at 25C: 11k             */
#define T_NOMINAL      25.0f      /* nominal temperature in Celsius          */
#define B_COEFFICIENT  3950.0f    /* B constant — check your NTC datasheet! */
#define V_SUPPLY       3.3f       /* supply voltage                          */
#define ADC_RESOLUTION 4095.0f    /* 12-bit ADC                              */

static uint16_t raw_t1     = 0;
static uint16_t raw_t2     = 0;
static float    voltage_t1 = 0.0f;
static float    voltage_t2 = 0.0f;
static float    temp_t1    = 0.0f;  /* PA0 temperature */
static float    temp_t2    = 0.0f;  /* PA1 temperature */

static char display_buf[32];

/* USER CODE END PV */

void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_ADC1_Init(void);
static void MX_I2C1_Init(void);

/* USER CODE BEGIN PFP */
static uint8_t  ADC_SelectChannel(uint32_t channel);
static uint16_t ADC_ReadRaw(uint32_t channel);
static float    ADC_RawToVoltage(uint16_t raw_val);
static float    NTC_ToTemperature(float v);
/* USER CODE END PFP */

/* USER CODE BEGIN 0 */

static uint8_t ADC_SelectChannel(uint32_t channel)
{
  ADC_ChannelConfTypeDef sConfig = {0};
  sConfig.Channel      = channel;
  sConfig.Rank         = ADC_REGULAR_RANK_1;
  sConfig.SamplingTime = ADC_SAMPLETIME_239CYCLES_5;

  if (HAL_ADC_ConfigChannel(&hadc1, &sConfig) != HAL_OK)
    return 0;

  return 1;
}

static uint16_t ADC_ReadRaw(uint32_t channel)
{
  if (!ADC_SelectChannel(channel))
    return 0;

  if (HAL_ADC_Start(&hadc1) != HAL_OK)
    return 0;

  if (HAL_ADC_PollForConversion(&hadc1, 100) != HAL_OK)
  {
    HAL_ADC_Stop(&hadc1);
    return 0;
  }

  uint16_t val = (uint16_t)HAL_ADC_GetValue(&hadc1);
  HAL_ADC_Stop(&hadc1);
  return val;
}

static float ADC_RawToVoltage(uint16_t raw_val)
{
  return ((float)raw_val * V_SUPPLY) / ADC_RESOLUTION;
}

static float NTC_ToTemperature(float v)
{
  if (v >= V_SUPPLY) return -273.15f;
  if (v <= 0.0f)     return -273.15f;

  /* R_NTC = R_FIXED * V / (V_SUPPLY - V) */
  float r_ntc = R_FIXED * v / (V_SUPPLY - v);

  /* Steinhart-Hart B-parameter equation */
  float steinhart = logf(r_ntc / R_NOMINAL);
  steinhart /= B_COEFFICIENT;
  steinhart += 1.0f / (T_NOMINAL + 273.15f);
  steinhart  = 1.0f / steinhart;

  return steinhart - 273.15f;  /* Kelvin → Celsius */
}

/* USER CODE END 0 */

int main(void)
{
  HAL_Init();
  SystemClock_Config();

  MX_GPIO_Init();
  MX_ADC1_Init();
  MX_I2C1_Init();

  /* USER CODE BEGIN 2 */
  ssd1306_Init();

  ssd1306_Fill(Black);
  ssd1306_SetCursor(10, 20);
  ssd1306_WriteString("Initializing...", Font_7x10, White);
  ssd1306_UpdateScreen();
  HAL_Delay(1000);
  /* USER CODE END 2 */

  while (1)
  {
    /* USER CODE BEGIN 3 */

    /* 1. Read PA0 (T1) */
    raw_t1     = ADC_ReadRaw(ADC_CHANNEL_0);
    voltage_t1 = ADC_RawToVoltage(raw_t1);
    temp_t1    = NTC_ToTemperature(voltage_t1);

    /* 2. Read PA1 (T2) */
    raw_t2     = ADC_ReadRaw(ADC_CHANNEL_1);
    voltage_t2 = ADC_RawToVoltage(raw_t2);
    temp_t2    = NTC_ToTemperature(voltage_t2);

    /* 3. Update OLED */
    ssd1306_Fill(Black);

    /* Title */
    ssd1306_SetCursor(15, 0);
    ssd1306_WriteString("Temperatures", Font_7x10, White);

    /* T1 — PA0 */
    sprintf(display_buf, "T1: %.1f C", temp_t1);
    ssd1306_SetCursor(0, 20);
    ssd1306_WriteString(display_buf, Font_7x10, White);

    /* T2 — PA1 */
    sprintf(display_buf, "T2: %.1f C", temp_t2);
    ssd1306_SetCursor(0, 40);
    ssd1306_WriteString(display_buf, Font_7x10, White);

    ssd1306_UpdateScreen();

    HAL_Delay(500); /* <<< BREAKPOINT HERE — watch temp_t1 and temp_t2 */

    /* USER CODE END 3 */
  }
}

void SystemClock_Config(void)
{
  RCC_OscInitTypeDef       RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef       RCC_ClkInitStruct = {0};
  RCC_PeriphCLKInitTypeDef PeriphClkInit     = {0};

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

  /* Default to CH0 (PA0) — ADC_SelectChannel() overrides at runtime */
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
  __disable_irq();
  while (1) { }
}

#ifdef USE_FULL_ASSERT
void assert_failed(uint8_t *file, uint32_t line) { }
#endif
