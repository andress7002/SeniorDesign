#pragma once
#include "stm32f1xx_hal.h"
#include "ina228_regs.h"
#include <stdint.h>
#include <stdbool.h>

// ============================================================
//  I2C timeout — finite value so HAL never hangs forever
//  when the INA228 is absent or the bus is floating.
//  500 ms is more than enough for any single register read.
// ============================================================
#define INA228_I2C_TIMEOUT_MS   500U
 

typedef enum {
  INA228_OK = 0,
  INA228_ERR_PARAM = -1,
  INA228_ERR_I2C = -2,
  INA228_ERR_ID = -3
} ina228_status_t;

typedef struct {
  I2C_HandleTypeDef *hi2c;
  uint8_t addr;          // 7-bit I2C address
  float shunt_ohms;

  // scaling chosen by you / computed
  float current_lsb;    // A per LSB of CURRENT register
  float power_lsb;      // W per LSB of POWER register

  // cached config (optional, useful for readback/debug)
  uint16_t config_reg;
  uint16_t adc_config_reg;
  uint16_t diag_alrt_reg;
  uint16_t shunt_cal_reg;
  uint16_t shunt_tempco_reg;
} ina228_t;

// --- Init/config ---
ina228_status_t ina228_init(ina228_t *dev,
                            I2C_HandleTypeDef *hi2c,
                            uint8_t addr,
                            float shunt_ohms,
                            float max_expected_current);
// Optional: set/get current LSB explicitly if you want
ina228_status_t ina228_set_current_lsb(ina228_t *dev, float current_lsb);

ina228_status_t ina228_set_adc_config(ina228_t *dev, uint16_t adc_cfg);

ina228_status_t ina228_check_id(ina228_t *dev);

// --- Raw register access ---
ina228_status_t ina228_write_u16(ina228_t *dev, uint8_t reg, uint16_t val);
ina228_status_t ina228_read_u16(ina228_t *dev, uint8_t reg, uint16_t *out);

ina228_status_t ina228_read_s20 (ina228_t *dev, uint8_t reg, int32_t  *out); // 20-bit signed (VBUS, CURRENT, VSHUNT)
ina228_status_t ina228_read_u24 (ina228_t *dev, uint8_t reg, uint32_t *out); // 24-bit unsigned (POWER)
ina228_status_t ina228_read_s40 (ina228_t *dev, uint8_t reg, int64_t  *out); // 40-bit signed  (ENERGY, CHARGE)

// ============================================================
//  Engineering-unit reads
// ============================================================
ina228_status_t ina228_get_vbus_V    (ina228_t *dev, float  *V);    // Bus voltage, Volts
ina228_status_t ina228_get_vshunt_V  (ina228_t *dev, float  *Vsh);  // Shunt voltage, Volts
ina228_status_t ina228_get_current_A (ina228_t *dev, float  *I);    // Current, Amperes
ina228_status_t ina228_get_power_W   (ina228_t *dev, float  *P);    // Power, Watts
ina228_status_t ina228_get_dietemp_C (ina228_t *dev, float  *Tc);   // Die temperature, °C
 
// Accumulators — useful for State-of-Charge models
ina228_status_t ina228_get_energy_J  (ina228_t *dev, double *J);    // Energy, Joules
ina228_status_t ina228_get_charge_C  (ina228_t *dev, double *Coul); // Charge, Coulombs
 
ina228_status_t ina228_reset_accumulators(ina228_t *dev);