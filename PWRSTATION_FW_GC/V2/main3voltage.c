/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Power-station monitor — Cell Voltage (3S) +
  *                   INA228 (I/P/E) + 2-channel NTC thermistor (T1/T2)
  *
  *  Cell voltage sensing (MCP6004 differential amp, gain = 2/3)
  *    PA2 -> ADC_CH2 -> Cell 1  (V_cell = ADC_V x 1.5)
  *    PA3 -> ADC_CH3 -> Cell 2
  *    PA4 -> ADC_CH4 -> Cell 3
  *
  *  INA228 (I2C1 — PB6/PB7)
  *    Measures current, power, energy, charge, die temp.
  *
  *  NTC thermistor channels (ADC1)
  *    PA0 -> ADC_CH0 -> T1   (10k pull-up to 3.3V)
  *    PA1 -> ADC_CH1 -> T2   (10k pull-up to 3.3V)
  *
  *  NOTE: PA0 (OLED_RST_Pin) is defined in main.h but oled.c never drives
  *        it — SSD1306 resets itself at power-on, so PA0/PA1 are free for ADC.
  *
  *  OLED layout  (SSD1306 128x64, 6x8 font, 21 cols x 8 rows)
  *    Row 0   C1:  3.712 V         Cell 1 voltage
  *    Row 1   C2:  3.708 V         Cell 2 voltage
  *    Row 2   C3:  3.715 V         Cell 3 voltage
  *    Row 3   I:   3.4560 A        INA228 current
  *    Row 4   P:  42.345 W         INA228 power
  *    Row 5   E: 1234.5  J         INA228 energy (slow task)
  *    Row 6   T1:  25.3 C          NTC thermistor 1
  *    Row 7   T2:  31.7 C          NTC thermistor 2
  ******************************************************************************
  */
/* USER CODE END Header */

/* Includes ------------------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "main.h"
#include <stdio.h>
#include <math.h>
#include "ina228.h"
#include "oled.h"
/* USER CODE END Includes */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* -- INA228 ----------------------------------------------------------------- */
#define INA228_I2C_ADDR    0x40
#define INA228_SHUNT_OHMS  0.001f
#define INA228_MAX_AMPS    40.0f

/* -- OLED ------------------------------------------------------------------- */
#define OLED_I2C_ADDR      0x3C

/* -- Scheduler intervals ---------------------------------------------------- */
#define INTERVAL_FAST_MS   500U
#define INTERVAL_SLOW_MS   5000U

/* -- NTC thermistor parameters ---------------------------------------------- */
#define R_FIXED        10000.0f
#define R_NOMINAL      11000.0f
#define T_NOMINAL      25.0f
#define B_COEFFICIENT  3950.0f
#define V_SUPPLY       3.3f
#define ADC_RESOLUTION 4095.0f

/* -- Cell voltage sensing (MCP6004 gain = 2/3, so inverse = x1.5) ---------- */
#define CELL_GAIN_INV  1.5f
#define CELL_VOLT_MIN  2.5f
#define CELL_VOLT_MAX  4.2f

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */
#define LED_ON()   HAL_GPIO_WritePin(GPIOC, GPIO_PIN_13, GPIO_PIN_RESET)
#define LED_OFF()  HAL_GPIO_WritePin(GPIOC, GPIO_PIN_13, GPIO_PIN_SET)
#define LED_TOG()  HAL_GPIO_TogglePin(GPIOC, GPIO_PIN_13)
/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
ADC_HandleTypeDef  hadc1;
I2C_HandleTypeDef  hi2c1;
TIM_HandleTypeDef  htim1;
UART_HandleTypeDef huart1;

/* USER CODE BEGIN PV */

/* -- INA228 ----------------------------------------------------------------- */
static ina228_t  g_ina;
static uint8_t   g_ina_ok   = 0U;

volatile float   g_current_A = 0.0f;
volatile float   g_power_W   = 0.0f;
volatile float   g_dietemp_C = 0.0f;
volatile double  g_energy_J  = 0.0;
volatile double  g_charge_C  = 0.0;
volatile uint8_t g_ina_err   = 0U;

/* -- Cell voltages (PA2=C1, PA3=C2, PA4=C3) -------------------------------- */
volatile float   g_vcell1    = 0.0f;
volatile float   g_vcell2    = 0.0f;
volatile float   g_vcell3    = 0.0f;

/* -- NTC thermistors (PA0=T1, PA1=T2) -------------------------------------- */
volatile float   temp_t1     = 0.0f;
volatile float   temp_t2     = 0.0f;

/* -- Scheduler timestamps --------------------------------------------------- */
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
static void     I2C1_BusRecovery(void);
static uint8_t  ADC_SelectChannel(uint32_t channel);
static uint16_t ADC_ReadRaw(uint32_t channel);
static float    NTC_ToTemperature(float v);
static float    CellADC_ToVoltage(uint32_t channel);
/* USER CODE END PFP */

/* USER CODE BEGIN 0 */

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
        LED_ON();  HAL_Delay(on_ms);
        LED_OFF(); HAL_Delay(off_ms);
    }
}

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
    if (!ADC_SelectChannel(channel)) return 0;
    if (HAL_ADC_Start(&hadc1) != HAL_OK) return 0;
    if (HAL_ADC_PollForConversion(&hadc1, 100U) != HAL_OK)
    {
        HAL_ADC_Stop(&hadc1);
        return 0;
    }
    uint16_t val = (uint16_t)HAL_ADC_GetValue(&hadc1);
    HAL_ADC_Stop(&hadc1);
    return val;
}

static float NTC_ToTemperature(float v)
{
    if (v >= V_SUPPLY) return -273.15f;
    if (v <= 0.0f)     return -273.15f;
    float r_ntc     = R_FIXED * v / (V_SUPPLY - v);
    float steinhart = logf(r_ntc / R_NOMINAL);
    steinhart /= B_COEFFICIENT;
    steinhart += 1.0f / (T_NOMINAL + 273.15f);
    steinhart  = 1.0f / steinhart;
    return steinhart - 273.15f;
}

static float CellADC_ToVoltage(uint32_t channel)
{
    uint16_t raw = ADC_ReadRaw(channel);
    float    v   = ((float)raw * V_SUPPLY) / ADC_RESOLUTION;
    return v * CELL_GAIN_INV;   /* undo MCP6004 2/3 gain */
}

/* ── Application tasks ──────────────────────────────────────────────────── */

static void app_init(void)
{
    led_blink(3, 80, 80);
    HAL_Delay(400);

    oled_status_t oled_s = oled_init(&hi2c1, OLED_I2C_ADDR);
    if (oled_s != OLED_OK)
    {
        led_blink(7, 200, 200);
    }
    else
    {
        oled_clear();
        oled_print_str(0, 0, "PWRSTATION FW");
        oled_print_str(0, 1, "Init...");
        oled_flush();
    }

    ina228_status_t ina_s = ina228_init(&g_ina, &hi2c1,
                                         INA228_I2C_ADDR,
                                         INA228_SHUNT_OHMS,
                                         INA228_MAX_AMPS);
    if (ina_s != INA228_OK)
    {
        led_blink(10, 200, 200);
        g_ina_ok = 0U;
        if (oled_s == OLED_OK)
        {
            oled_clear();
            oled_print_str(0, 0, "INA228 MISSING");
            oled_print_str(0, 1, "Cell+NTC only");
            oled_flush();
            HAL_Delay(2000);
        }
        return;
    }

    if (ina228_check_id(&g_ina) != INA228_OK)
    {
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
  * @brief  Fast task — 500 ms cadence.
  *
  *   Row 0  C1:  3.712 V       Cell 1 (PA2)   [!! CELL FAULT !! if out of range]
  *   Row 1  C2:  3.708 V       Cell 2 (PA3)
  *   Row 2  C3:  3.715 V       Cell 3 (PA4)
  *   Row 3  I:   3.4560 A      INA228 current
  *   Row 4  P:  42.345 W       INA228 power
  *   Row 5  E: 1234.5  J       INA228 energy  (written by slow task)
  *   Row 6  T1:  25.3 C        NTC thermistor 1 (PA0)
  *   Row 7  T2:  31.7 C        NTC thermistor 2 (PA1)
  */
static void app_fast_task(void)
{
    /* ── Cell voltages (PA2/PA3/PA4) -> rows 0-2 ─────────────────────────── */
    g_vcell1 = CellADC_ToVoltage(ADC_CHANNEL_2);
    g_vcell2 = CellADC_ToVoltage(ADC_CHANNEL_3);
    g_vcell3 = CellADC_ToVoltage(ADC_CHANNEL_4);

    uint8_t cell_fault = (g_vcell1 < CELL_VOLT_MIN || g_vcell1 > CELL_VOLT_MAX ||
                          g_vcell2 < CELL_VOLT_MIN || g_vcell2 > CELL_VOLT_MAX ||
                          g_vcell3 < CELL_VOLT_MIN || g_vcell3 > CELL_VOLT_MAX) ? 1U : 0U;

    if (cell_fault)
        oled_printf_row(0, "!! CELL FAULT !!");
    else
        oled_printf_row(0, "C1:%6.3fV", (double)g_vcell1);

    oled_printf_row(1, "C2:%6.3fV", (double)g_vcell2);
    oled_printf_row(2, "C3:%6.3fV", (double)g_vcell3);

    /* ── INA228 current + power -> rows 3-4 ─────────────────────────────── */
    if (g_ina_ok)
    {
        ina228_status_t s = INA228_OK;
        s |= ina228_get_current_A(&g_ina, (float *)&g_current_A);
        s |= ina228_get_power_W  (&g_ina, (float *)&g_power_W);
        s |= ina228_get_dietemp_C(&g_ina, (float *)&g_dietemp_C);

        if (s != INA228_OK)
        {
            g_ina_err++;
            oled_printf_row(3, "INA228 ERR");
            oled_printf_row(4, "");
        }
        else
        {
            g_ina_err = 0U;
            oled_printf_row(3, "I:%7.4fA", (double)g_current_A);
            oled_printf_row(4, "P:%7.3fW",  (double)g_power_W);
        }
    }
    else
    {
        oled_printf_row(3, "I:    --- A");
        oled_printf_row(4, "P:    --- W");
    }

    /* ── NTC thermistors (PA0=T1, PA1=T2) -> rows 6-7 ───────────────────── */
    temp_t1 = NTC_ToTemperature(((float)ADC_ReadRaw(ADC_CHANNEL_0) * V_SUPPLY) / ADC_RESOLUTION);
    temp_t2 = NTC_ToTemperature(((float)ADC_ReadRaw(ADC_CHANNEL_1) * V_SUPPLY) / ADC_RESOLUTION);

    oled_printf_row(6, "T1:%5.1fC", (double)temp_t1);
    oled_printf_row(7, "T2:%5.1fC", (double)temp_t2);

    oled_flush();
}

/**
  * @brief  Slow task — 5000 ms cadence.  Energy accumulator -> row 5.
  */
static void app_slow_task(void)
{
    if (!g_ina_ok) return;

    ina228_status_t s = INA228_OK;
    s |= ina228_get_energy_J(&g_ina, (double *)&g_energy_J);
    s |= ina228_get_charge_C(&g_ina, (double *)&g_charge_C);

    if (s != INA228_OK) { g_ina_err++; return; }

    oled_printf_row(5, "E:%7.1fJ", g_energy_J);
    oled_flush();
}

static void I2C1_BusRecovery(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    __HAL_RCC_GPIOB_CLK_ENABLE();
    __HAL_RCC_I2C1_FORCE_RESET();
    HAL_Delay(10);
    __HAL_RCC_I2C1_RELEASE_RESET();

    GPIO_InitStruct.Pin   = GPIO_PIN_6 | GPIO_PIN_7;
    GPIO_InitStruct.Mode  = GPIO_MODE_OUTPUT_OD;
    GPIO_InitStruct.Pull  = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_6 | GPIO_PIN_7, GPIO_PIN_SET);
    HAL_Delay(1);

    for (uint8_t i = 0; i < 9; i++)
    {
        HAL_GPIO_WritePin(GPIOB, GPIO_PIN_6, GPIO_PIN_RESET);
        HAL_Delay(1);
        HAL_GPIO_WritePin(GPIOB, GPIO_PIN_6, GPIO_PIN_SET);
        HAL_Delay(1);
        if (HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_7) == GPIO_PIN_SET)
            break;
    }

    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_7, GPIO_PIN_RESET);
    HAL_Delay(1);
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_6, GPIO_PIN_SET);
    HAL_Delay(1);
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_7, GPIO_PIN_SET);
    HAL_Delay(1);

    HAL_GPIO_DeInit(GPIOB, GPIO_PIN_6 | GPIO_PIN_7);
}

/* USER CODE END 0 */

int main(void)
{
    HAL_Init();
    SystemClock_Config();

    /* USER CODE BEGIN SysInit */
    I2C1_BusRecovery();
    /* USER CODE END SysInit */

    MX_GPIO_Init();
    MX_ADC1_Init();
    MX_I2C1_Init();
    MX_TIM1_Init();
    MX_USART1_UART_Init();

    /* USER CODE BEGIN 2 */
    app_init();
    s_t_fast = HAL_GetTick();
    s_t_slow = HAL_GetTick();
    /* USER CODE END 2 */

    while (1)
    {
        /* USER CODE BEGIN 3 */
        if (ms_elapsed(s_t_fast) >= INTERVAL_FAST_MS)
        {
            s_t_fast = HAL_GetTick();
            LED_TOG();
            app_fast_task();
        }

        if (ms_elapsed(s_t_slow) >= INTERVAL_SLOW_MS)
        {
            s_t_slow = HAL_GetTick();
            app_slow_task();
        }
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

    /* Default CH0 (PA0/T1) — ADC_SelectChannel() overrides at runtime
     * Channels in use:
     *   CH0 (PA0) T1 NTC
     *   CH1 (PA1) T2 NTC
     *   CH2 (PA2) Cell 1 op-amp output
     *   CH3 (PA3) Cell 2 op-amp output
     *   CH4 (PA4) Cell 3 op-amp output        */
    sConfig.Channel      = ADC_CHANNEL_0;
    sConfig.Rank         = ADC_REGULAR_RANK_1;
    sConfig.SamplingTime = ADC_SAMPLETIME_239CYCLES_5;
    if (HAL_ADC_ConfigChannel(&hadc1, &sConfig) != HAL_OK)
        Error_Handler();
}

static void MX_I2C1_Init(void)
{
    hi2c1.Instance             = I2C1;
    hi2c1.Init.ClockSpeed      = 400000;
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

static void MX_TIM1_Init(void)
{
    TIM_ClockConfigTypeDef  sClockSourceConfig = {0};
    TIM_MasterConfigTypeDef sMasterConfig      = {0};

    htim1.Instance               = TIM1;
    htim1.Init.Prescaler         = 7;
    htim1.Init.CounterMode       = TIM_COUNTERMODE_UP;
    htim1.Init.Period            = 65535;
    htim1.Init.ClockDivision     = TIM_CLOCKDIVISION_DIV1;
    htim1.Init.RepetitionCounter = 0;
    htim1.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
    if (HAL_TIM_Base_Init(&htim1) != HAL_OK)
        Error_Handler();

    sClockSourceConfig.ClockSource = TIM_CLOCKSOURCE_INTERNAL;
    if (HAL_TIM_ConfigClockSource(&htim1, &sClockSourceConfig) != HAL_OK)
        Error_Handler();

    sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
    sMasterConfig.MasterSlaveMode     = TIM_MASTERSLAVEMODE_DISABLE;
    if (HAL_TIMEx_MasterConfigSynchronization(&htim1, &sMasterConfig) != HAL_OK)
        Error_Handler();

    HAL_TIM_Base_Start(&htim1);
}

static void MX_USART1_UART_Init(void)
{
    huart1.Instance          = USART1;
    huart1.Init.BaudRate     = 115200;
    huart1.Init.WordLength   = UART_WORDLENGTH_8B;
    huart1.Init.StopBits     = UART_STOPBITS_1;
    huart1.Init.Parity       = UART_PARITY_NONE;
    huart1.Init.Mode         = UART_MODE_TX_RX;
    huart1.Init.HwFlowCtl    = UART_HWCONTROL_NONE;
    huart1.Init.OverSampling = UART_OVERSAMPLING_16;
    if (HAL_UART_Init(&huart1) != HAL_OK)
        Error_Handler();
}

static void MX_GPIO_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    __HAL_RCC_GPIOC_CLK_ENABLE();
    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();

    /* PC13 — onboard LED, active-LOW */
    HAL_GPIO_WritePin(GPIOC, GPIO_PIN_13, GPIO_PIN_SET);
    GPIO_InitStruct.Pin   = GPIO_PIN_13;
    GPIO_InitStruct.Mode  = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull  = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

    /* PA0-PA4 left as analog inputs — HAL_ADC_Init() configures them.
     * Do NOT drive these pins as digital outputs.                     */
}

void Error_Handler(void)
{
    __disable_irq();
    while (1) { }
}

#ifdef USE_FULL_ASSERT
void assert_failed(uint8_t *file, uint32_t line)
{
    (void)file; (void)line;
}
#endif
