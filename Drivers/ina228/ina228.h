#pragma once
#include "stm32f1xx_hal.h"
#include <stdint.h>
#include <stdbool.h>

typedef enum {
  INA228_OK = 0,
  INA228_ERR_PARAM = -1,
  INA228_ERR_I2C = -2,
  INA228_ERR_ID = -3
} ina228_status_t;

typedef struct {
  I2C_HandleTypeDef *hi2c;
  uint8_t addr7;          // 7-bit I2C address
  float shunt_ohms;

  // scaling chosen by you / computed
  float current_lsb_A;    // A per LSB of CURRENT register
  float power_lsb_W;      // W per LSB of POWER register

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
                            uint8_t addr7,
                            float shunt_ohms,
                            float max_expected_current_A);

// Optional: set/get current LSB explicitly if you want
ina228_status_t ina228_set_current_lsb(ina228_t *dev, float current_lsb_A);

// --- Raw register access ---
ina228_status_t ina228_write_u16(ina228_t *dev, uint8_t reg, uint16_t val);
ina228_status_t ina228_read_u16(ina228_t *dev, uint8_t reg, uint16_t *out);

// INA228 has 24-bit / 40-bit style registers for some quantities.
// We provide helpers for common ones:
ina228_status_t ina228_read_s24(ina228_t *dev, uint8_t reg, int32_t *out);   // signed 24-bit
ina228_status_t ina228_read_u24(ina228_t *dev, uint8_t reg, uint32_t *out); // unsigned 24-bit
ina228_status_t ina228_read_s40(ina228_t *dev, uint8_t reg, int64_t *out);  // signed 40-bit

// --- Engineering units ---
ina228_status_t ina228_get_vbus_V(ina228_t *dev, float *V);
ina228_status_t ina228_get_vshunt_V(ina228_t *dev, float *Vsh);
ina228_status_t ina228_get_current_A(ina228_t *dev, float *I);
ina228_status_t ina228_get_power_W(ina228_t *dev, float *P);
ina228_status_t ina228_get_dietemp_C(ina228_t *dev, float *Tc);

// Energy/charge are optional but very useful for SOC models
ina228_status_t ina228_get_energy_J(ina228_t *dev, double *J);
ina228_status_t ina228_get_charge_C(ina228_t *dev, double *C);