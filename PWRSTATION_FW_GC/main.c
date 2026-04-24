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


/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "main.h"
#include "stdio.h"
#include "ina228.h"
#include "oled.h"


/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
/* ── INA228 ──────────────────────────────────────────────────────────────── */
#define INA228_I2C_ADDR    0x40U    // A0=GND, A1=GND
#define INA228_SHUNT_OHMS  0.0015f   // Shunt resistor (Ω) — adjust to yours
#define INA228_MAX_AMPS    40.0f    // Max expected current (A)
 
/* ── OLED ────────────────────────────────────────────────────────────────── */
#define OLED_I2C_ADDR      0x3DU    // 0x3C or 0x3D depending on SA0 pin
 
/* ── Scheduler intervals ─────────────────────────────────────────────────── */
#define INTERVAL_FAST_MS   500U     // V / I / P / T
#define INTERVAL_SLOW_MS   5000U    // Energy / Charge accumulators
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* LED helpers — PC13 is active-LOW on Blue Pill */
#define LED_ON()   HAL_GPIO_WritePin(GPIOC, GPIO_PIN_13, GPIO_PIN_RESET)
#define LED_OFF()  HAL_GPIO_WritePin(GPIOC, GPIO_PIN_13, GPIO_PIN_SET)
#define LED_TOG()  HAL_GPIO_TogglePin(GPIOC, GPIO_PIN_13)

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
ADC_HandleTypeDef hadc1;

I2C_HandleTypeDef hi2c1;

TIM_HandleTypeDef htim1;

UART_HandleTypeDef huart1;

/* USER CODE BEGIN PV */

/* ── INA228 driver handle ────────────────────────────────────────────────── */
static ina228_t g_ina;
static uint8_t   g_ina_ok  = 0U;   // 1 = INA228 initialised successfully
 
/* ── Live engineering values
 *    Declared volatile so the debugger Live Watch sees real-time updates
 *    even with optimisation enabled.                                        */
volatile float  g_vbus_V    = 0.0f;
volatile float  g_vshunt_V  = 0.0f;
volatile float  g_current_A = 0.0f;
volatile float  g_power_W   = 0.0f;
volatile float  g_dietemp_C = 0.0f;
volatile double g_energy_J  = 0.0;
volatile double g_charge_C  = 0.0;
 
/* ── Error counter — inspect in Watch panel to spot I2C issues ───────────── */
volatile uint8_t g_ina_err  = 0U;
 
/* ── Scheduler timestamps ────────────────────────────────────────────────── */
static uint32_t s_t_fast = 0U;
static uint32_t s_t_slow = 0U;

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_ADC1_Init(void);
static void MX_I2C1_Init(void);
static void MX_TIM1_Init(void);
static void MX_USART1_UART_Init(void);
/* USER CODE BEGIN PFP */

static void     led_blink(uint8_t count, uint32_t on_ms, uint32_t off_ms);
static void     app_init(void);
static void     app_fast_task(void);
static void     app_slow_task(void);
static uint32_t ms_elapsed(uint32_t since);

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

/**
  * @brief  Milliseconds elapsed since 'since', handles 32-bit Systick rollover.
  */
static uint32_t ms_elapsed(uint32_t since)
{
    uint32_t now = HAL_GetTick();
    return (now >= since) ? (now - since)
                          : (0xFFFFFFFFU - since + now + 1U);
}
 
static void led_blink(uint8_t count, uint32_t on_ms, uint32_t off_ms)
{
    for (uint8_t i = 0; i < count; i++)
    {
        LED_ON();
        HAL_Delay(on_ms);
        LED_OFF();
        HAL_Delay(off_ms);
    }
}

static void app_init(void)
{
    /* ── 3 quick blinks — MCU is alive, peripherals initialised ─────────── */
    led_blink(3, 80, 80);
    HAL_Delay(400);

    /* ── OLED init — capture return value to detect ACK failure ─────────── */
    oled_status_t oled_s = oled_init(&hi2c1, OLED_I2C_ADDR);

    if (oled_s != OLED_OK)
    {
        /*
         *  7 x 1000 ms blinks = OLED not responding on I2C.
         *  Causes: wrong address, missing pull-ups, RES not wired to PB0,
         *  CS not tied to GND, or SPI/I2C resistors not moved on module.
         *  Firmware continues — INA228 still initialises normally.
         */
        led_blink(7, 1000, 1000);
    }
    else
    {
        /* OLED responded — show splash screen */
        oled_clear();
        oled_print_str(0, 0, "PWRSTATION FW");
        oled_print_str(0, 1, "Init...");
        oled_flush();
    }

    /* ── INA228 init — capture return value separately ───────────────────── */
    ina228_status_t ina_s = ina228_init(&g_ina, &hi2c1,
                                         INA228_I2C_ADDR,
                                         INA228_SHUNT_OHMS,
                                         INA228_MAX_AMPS);

    if (ina_s != INA228_OK)
    {
        /*
         *  10 x 1000 ms blinks = INA228 not responding on I2C.
         *  Causes: not wired yet, wrong address (check A0/A1 pins),
         *  missing pull-ups, or device unpowered.
         *  Firmware continues in OLED-only mode.
         */
        led_blink(10, 1000, 1000);
        g_ina_ok = 0U;

        if (oled_s == OLED_OK)
        {
            oled_clear();
            oled_print_str(0, 0, "INA228 MISSING");
            oled_print_str(0, 1, "Check wiring");
            oled_print_str(0, 2, "OLED-only mode");
            oled_flush();
            HAL_Delay(2000);
        }
        return;
    }

    /* INA228 responded — verify silicon ID */
    if (ina228_check_id(&g_ina) != INA228_OK)
    {
        /* 3 x 200 ms blinks = device found but not an INA228 */
        led_blink(3, 200, 200);
        g_ina_ok = 0U;

        if (oled_s == OLED_OK)
        {
            oled_clear();
            oled_print_str(0, 0, "INA228 BAD ID");
            oled_flush();
            HAL_Delay(2000);
        }
        return;
    }

    /* ADC: continuous all channels, 1052 us conversion, 64-sample average */
    ina228_set_adc_config(&g_ina,
        INA228_REG_MODE_CONT_ALL |
        INA228_REG_VBUSCT_1052US |
        INA228_REG_VSHCT_1052US  |
        INA228_REG_VTCT_1052US   |
        INA228_REG_AVG_64);

    ina228_reset_accumulators(&g_ina);
    g_ina_ok = 1U;

    if (oled_s == OLED_OK)
    {
        oled_clear();
        oled_print_str(0, 0, "PWRSTATION FW");
        oled_print_str(0, 1, "INA228 OK");
        oled_flush();
        HAL_Delay(1000);
    }
}

/**
  * @brief  Fast task — runs every INTERVAL_FAST_MS (500 ms).
  *
  *         Reads V, I, P, T from INA228.
  *         Pushes formatted values to OLED rows 0-3.
  *
  *         OLED layout (128×64, font 6×8, 21 chars wide, 8 rows):
  *
  *           Row 0   V:  12.345 V
  *           Row 1   I:   3.456 A
  *           Row 2   P:  42.345 W
  *           Row 3   T:   27.5 C
  *           Row 4   E: 1234.5  J   ← slow task
  *           Row 5   Q:  567.8  C   ← slow task
  *           Row 6   (reserved — SoC %)
  *           Row 7   (reserved — status / alert flags)
  */
static void app_fast_task(void)
{
    if (!g_ina_ok) return;
    ina228_status_t s = INA228_OK;
 
    s |= ina228_get_vbus_V   (&g_ina, (float *)&g_vbus_V);
    s |= ina228_get_vshunt_V (&g_ina, (float *)&g_vshunt_V);
    s |= ina228_get_current_A(&g_ina, (float *)&g_current_A);
    s |= ina228_get_power_W  (&g_ina, (float *)&g_power_W);
    s |= ina228_get_dietemp_C(&g_ina, (float *)&g_dietemp_C);
 
    if (s != INA228_OK)
    {
        g_ina_err++;
        oled_clear();
        oled_print_str(0, 0, "INA228 ERR");
        oled_flush();
        return;
    }
 
    g_ina_err = 0U;
 
    /*
     *  oled_printf_row() is a stub in oled.c.
     *  Signature: void oled_printf_row(uint8_t row, const char *fmt, ...)
     *  It formats a string and writes it to the given text row.
     *  Implement using snprintf + your SSD1306 character write function.
     */
    oled_printf_row(0, "V:%7.3fV",  (double)g_vbus_V);
    oled_printf_row(1, "I:%7.4fA",  (double)g_current_A);
    oled_printf_row(2, "P:%7.3fW",  (double)g_power_W);
    oled_printf_row(3, "T:%5.1fC",  (double)g_dietemp_C);
    oled_flush();
}
 
/**
  * @brief  Slow task — runs every INTERVAL_SLOW_MS (5000 ms).
  *
  *         Reads energy and charge accumulators from INA228.
  *         Pushes values to OLED rows 4-5.
  */
static void app_slow_task(void)
{
    ina228_status_t s = INA228_OK;
 
    s |= ina228_get_energy_J(&g_ina, (double *)&g_energy_J);
    s |= ina228_get_charge_C(&g_ina, (double *)&g_charge_C);
 
    if (s != INA228_OK)
    {
        g_ina_err++;
        return;
    }
 
    oled_printf_row(4, "E:%7.1fJ",  g_energy_J);
    oled_printf_row(5, "Q:%7.3fC",  g_charge_C);
    oled_flush();
}

/**
  * @brief  STM32F1 I2C bus recovery.
  *
  *         Must be called BEFORE HAL_I2C_Init() / MX_I2C1_Init().
  *
  *         Procedure (per NXP I2C specification UM10204 section 3.1.16):
  *           1. Configure PB6 (SCL) and PB7 (SDA) as GPIO outputs.
  *           2. Toggle SCL 9 times while SDA is HIGH — this clocks out
  *              any stuck slave that was mid-transaction.
  *           3. Generate a STOP condition (SDA LOW→HIGH while SCL HIGH).
  *           4. Release both pins back to open-drain for the HAL to take over.
  *
  *         After this call, MX_I2C1_Init() will find a clean bus.
  */

  static void I2C1_BusRecovery(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    __HAL_RCC_GPIOB_CLK_ENABLE();
    __HAL_RCC_I2C1_FORCE_RESET();
    HAL_Delay(10);
    __HAL_RCC_I2C1_RELEASE_RESET();

    /* Configure PB6 (SCL) and PB7 (SDA) as open-drain GPIO outputs */
    GPIO_InitStruct.Pin   = GPIO_PIN_6 | GPIO_PIN_7;
    GPIO_InitStruct.Mode  = GPIO_MODE_OUTPUT_OD;   // open-drain — critical
    GPIO_InitStruct.Pull  = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

    /* Start with both lines HIGH (idle) */
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_6 | GPIO_PIN_7, GPIO_PIN_SET);
    HAL_Delay(1);

    /* Clock 9 pulses on SCL to release any stuck slave */
    for (uint8_t i = 0; i < 9; i++)
    {
        HAL_GPIO_WritePin(GPIOB, GPIO_PIN_6, GPIO_PIN_RESET); // SCL LOW
        HAL_Delay(1);
        HAL_GPIO_WritePin(GPIOB, GPIO_PIN_6, GPIO_PIN_SET);   // SCL HIGH
        HAL_Delay(1);

        /* If SDA is now HIGH the slave has released the bus — done early */
        if (HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_7) == GPIO_PIN_SET)
            break;
    }

    /* Generate STOP condition: SDA LOW→HIGH while SCL HIGH */
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_7, GPIO_PIN_RESET); // SDA LOW
    HAL_Delay(1);
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_6, GPIO_PIN_SET);   // SCL HIGH
    HAL_Delay(1);
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_7, GPIO_PIN_SET);   // SDA HIGH (STOP)
    HAL_Delay(1);

    /*
     *  Release PB6 and PB7 — HAL_I2C_Init() will reconfigure them
     *  as AF open-drain automatically when MX_I2C1_Init() runs.
     */
    HAL_GPIO_DeInit(GPIOB, GPIO_PIN_6 | GPIO_PIN_7);
}


/* USER CODE END 0 */


int main(void)
{

  /* USER CODE BEGIN 1 */

  /* USER CODE END 1 */

  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  //initialise_monitor_handles();
  HAL_Init();

  /* USER CODE BEGIN Init */
  
  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_ADC1_Init();
  I2C1_BusRecovery();
  MX_I2C1_Init();
  MX_TIM1_Init();
  MX_USART1_UART_Init();
  
  /* USER CODE BEGIN 2 */
   app_init();
    s_t_fast = HAL_GetTick();
    s_t_slow = HAL_GetTick();

  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
  /* USER CODE END 3 */

   /*
         *  Non-blocking cooperative scheduler.
         *  Each branch checks whether enough time has elapsed and,
         *  if so, runs the task and resets the timestamp.
         *
         *  Total worst-case blocking: one HAL_I2C transaction (~few ms).
         *  If you add an RTOS later, each task becomes its own thread.
         */
        if (ms_elapsed(s_t_fast) >= INTERVAL_FAST_MS)
        {
            s_t_fast = HAL_GetTick();
            app_fast_task();
        }
 
        if (ms_elapsed(s_t_slow) >= INTERVAL_SLOW_MS)
        {
            s_t_slow = HAL_GetTick();
            app_slow_task();
        }
 
        /*
         *  Uncomment __WFI() to sleep between scheduler ticks.
         *  Systick interrupt wakes the CPU every 1 ms.
         *  Saves ~10 mA on 3.3 V — useful if PWRSTATION is battery-powered.
         */
        /* __WFI(); */

   }
  /* USER CODE END 3 */

  /*
  
 PUTCHAR_PROTOTYPE
 {
  // Place your implementation of fputc here 
  // e.g. write a character to the USART1 and Loop until the end of transmission 
  HAL_UART_Transmit(&huart1, (uint8_t *)&ch, 1, 0xFFFF);

  return ch;
 }

  */ 

}


/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};
  RCC_PeriphCLKInitTypeDef PeriphClkInit = {0};

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_NONE;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_HSI;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_0) != HAL_OK)
  {
    Error_Handler();
  }
  PeriphClkInit.PeriphClockSelection = RCC_PERIPHCLK_ADC;
  PeriphClkInit.AdcClockSelection = RCC_ADCPCLK2_DIV2;
  if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInit) != HAL_OK)
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
  hadc1.Init.ScanConvMode = ADC_SCAN_DISABLE;
  hadc1.Init.ContinuousConvMode = DISABLE;
  hadc1.Init.DiscontinuousConvMode = DISABLE;
  hadc1.Init.ExternalTrigConv = ADC_SOFTWARE_START;
  hadc1.Init.DataAlign = ADC_DATAALIGN_RIGHT;
  hadc1.Init.NbrOfConversion = 1;
  if (HAL_ADC_Init(&hadc1) != HAL_OK)
  {
    Error_Handler();
  }

  /** Configure Regular Channel
  */
  sConfig.Channel = ADC_CHANNEL_0;
  sConfig.Rank = ADC_REGULAR_RANK_1;
  sConfig.SamplingTime = ADC_SAMPLETIME_1CYCLE_5;
  if (HAL_ADC_ConfigChannel(&hadc1, &sConfig) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN ADC1_Init 2 */

  /* USER CODE END ADC1_Init 2 */

}

/**
  * @brief I2C1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_I2C1_Init(void)
{

  /* USER CODE BEGIN I2C1_Init 0 */

  /* USER CODE END I2C1_Init 0 */

  /* USER CODE BEGIN I2C1_Init 1 */

  /* USER CODE END I2C1_Init 1 */
  hi2c1.Instance = I2C1;
  hi2c1.Init.ClockSpeed = 400000; // 400 kHz Fast-mode
  hi2c1.Init.DutyCycle = I2C_DUTYCYCLE_2;
  hi2c1.Init.OwnAddress1 = 0;
  hi2c1.Init.AddressingMode = I2C_ADDRESSINGMODE_7BIT;
  hi2c1.Init.DualAddressMode = I2C_DUALADDRESS_DISABLE;
  hi2c1.Init.OwnAddress2 = 0;
  hi2c1.Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
  hi2c1.Init.NoStretchMode = I2C_NOSTRETCH_DISABLE;
  if (HAL_I2C_Init(&hi2c1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN I2C1_Init 2 */

  /* USER CODE END I2C1_Init 2 */

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

  TIM_ClockConfigTypeDef sClockSourceConfig = {0};
  TIM_MasterConfigTypeDef sMasterConfig = {0};

  /* USER CODE BEGIN TIM1_Init 1 */

  /* USER CODE END TIM1_Init 1 */
  htim1.Instance = TIM1;
  htim1.Init.Prescaler = 7;
  htim1.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim1.Init.Period = 0xFFFF;
  htim1.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  htim1.Init.RepetitionCounter = 0;
  htim1.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
  if (HAL_TIM_Base_Init(&htim1) != HAL_OK)
  {
    Error_Handler();
  }
  sClockSourceConfig.ClockSource = TIM_CLOCKSOURCE_INTERNAL;
  if (HAL_TIM_ConfigClockSource(&htim1, &sClockSourceConfig) != HAL_OK)
  {
    Error_Handler();
  }
  sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
  sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
  if (HAL_TIMEx_MasterConfigSynchronization(&htim1, &sMasterConfig) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN TIM1_Init 2 */
  HAL_TIM_Base_Start(&htim1);  
  /* USER CODE END TIM1_Init 2 */

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
  if (HAL_UART_Init(&huart1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN USART1_Init 2 */

  /* USER CODE END USART1_Init 2 */

}

/**
  * @brief GPIO Initialization Function
  * @param None
  * @retval None
  */
static void MX_GPIO_Init(void)
{
  /* USER CODE BEGIN MX_GPIO_Init_1 */
  GPIO_InitTypeDef GPIO_InitStruct = {0};
  /* USER CODE END MX_GPIO_Init_1 */

  /* GPIO Ports Clock Enable */
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();
  __HAL_RCC_GPIOC_CLK_ENABLE();
  /* USER CODE BEGIN MX_GPIO_Init_2 */
  /* PC13 — onboard LED, active-LOW on Blue Pill.
     * Set HIGH at init → LED off.
     * Used as a fault indicator in app_init().                             */
    HAL_GPIO_WritePin(GPIOC, GPIO_PIN_13, GPIO_PIN_SET);
    GPIO_InitStruct.Pin   = GPIO_PIN_13;
    GPIO_InitStruct.Mode  = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull  = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);
  /* USER CODE END MX_GPIO_Init_2 */
}

/* USER CODE BEGIN 4 */

//
//int _write(int file, char *ptr, int len)
//{
//    HAL_UART_Transmit(&huart1, (uint8_t*)ptr, len, HAL_MAX_DELAY);
//    return len;
//}



/* USER CODE END 4 */

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
  (void)file;
  (void)line;
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
