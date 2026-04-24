#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <math.h>

#include "ina228.h"

/////////////

/* Inline peripheral reset — call after any I2C timeout */
static void _i2c_reset(I2C_HandleTypeDef *hi2c)
{
    HAL_I2C_DeInit(hi2c);
    __HAL_RCC_I2C1_FORCE_RESET();
    HAL_Delay(5);
    __HAL_RCC_I2C1_RELEASE_RESET();
    HAL_I2C_Init(hi2c);
}

/**
 * Write a single register-pointer byte then read back N bytes.
 *         Used by all read helpers to avoid duplicating the two-phase
 *         transmit→receive pattern.
 */
static ina228_status_t _read_reg(ina228_t *dev, uint8_t reg,
                                  uint8_t *buf, uint8_t len)
{
    if (HAL_I2C_Master_Transmit(dev->hi2c, (uint16_t)(dev->addr << 1),
                                &reg, 1,
                                INA228_I2C_TIMEOUT_MS) != HAL_OK)
     {
        _i2c_reset(dev->hi2c);   
        return INA228_ERR_I2C;
     }

    if (HAL_I2C_Master_Receive(dev->hi2c, (uint16_t)(dev->addr << 1),
                               buf, len,
                               INA228_I2C_TIMEOUT_MS) != HAL_OK)
     {
        _i2c_reset(dev->hi2c);   
        return INA228_ERR_I2C;
     }

 return INA228_OK;
}


ina228_status_t ina228_write_u16(ina228_t *dev, uint8_t reg, uint16_t val)
{
    uint8_t tx[3];
    tx[0] = reg;
    tx[1] = (val >> 8) & 0xFF;
    tx[2] =  val       & 0xFF;
 
    if (HAL_I2C_Master_Transmit(dev->hi2c, dev->addr << 1,
                                tx, 3, INA228_I2C_TIMEOUT_MS) != HAL_OK)
        return INA228_ERR_I2C;
 
    return INA228_OK;
}
 
ina228_status_t ina228_read_u16(ina228_t *dev, uint8_t reg, uint16_t *out)
{
    uint8_t rx[2];
    ina228_status_t s = _read_reg(dev, reg, rx, 2);
    if (s != INA228_OK) return s;
 
    *out = ((uint16_t)rx[0] << 8) | rx[1];
    return INA228_OK;
}

//////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////READ AND WRITE//////////////////////////////////////////////


ina228_status_t ina228_read_s20(ina228_t *dev, uint8_t reg, int32_t *out)
{
    uint8_t rx[3];
    ina228_status_t s = _read_reg(dev, reg, rx, 3);
    if (s != INA228_OK) return s;
 
    // Assemble raw 24-bit value
    int32_t raw = ((int32_t)rx[0] << 16) |
                  ((int32_t)rx[1] <<  8) |
                            rx[2];
 
    // Shift right 4 to get 20-bit integer
    raw >>= 4;
 
    // Sign-extend from bit 19
    if (raw & 0x00080000)
        raw |= 0xFFF00000;
 
    *out = raw;
    return INA228_OK;
}
 
/**
 * @brief  Read a 24-bit unsigned register (POWER).
 */
ina228_status_t ina228_read_u24(ina228_t *dev, uint8_t reg, uint32_t *out)
{
    uint8_t rx[3];
    ina228_status_t s = _read_reg(dev, reg, rx, 3);
    if (s != INA228_OK) return s;
 
    *out = ((uint32_t)rx[0] << 16) |
           ((uint32_t)rx[1] <<  8) |
                      rx[2];
    return INA228_OK;
}
 
/**
 * @brief  Read a 40-bit signed register (ENERGY or CHARGE).
 *         Byte order: MSB first, 5 bytes total.
 */
ina228_status_t ina228_read_s40(ina228_t *dev, uint8_t reg, int64_t *out)
{
    uint8_t rx[5];
    ina228_status_t s = _read_reg(dev, reg, rx, 5);
    if (s != INA228_OK) return s;
 
    int64_t val = ((int64_t)rx[0] << 32) |
                  ((int64_t)rx[1] << 24) |
                  ((int64_t)rx[2] << 16) |
                  ((int64_t)rx[3] <<  8) |
                             rx[4];
 
    // Sign-extend from bit 39
    if (val & ((int64_t)1 << 39))
        val |= ~(((int64_t)1 << 40) - 1);
 
    *out = val;
    return INA228_OK;
}




/* ================= INITIALIZATION AND CONFIG================= */

ina228_status_t ina228_init(ina228_t *dev,
                            I2C_HandleTypeDef *hi2c,
                            uint8_t  addr,
                            float    shunt_ohms,
                            float    max_expected_A)
{
    if (!dev || !hi2c || shunt_ohms <= 0.0f || max_expected_A <= 0.0f)
        return INA228_ERR_PARAM;
 
    dev->hi2c      = hi2c;
    dev->addr      = addr;
    dev->shunt_ohms = shunt_ohms;
 
    // current_lsb = Imax / 2^19  (INA228 datasheet eq. 1)
    dev->current_lsb = max_expected_A / 524288.0f;
 
    // power_lsb  = 3.2 × current_lsb  (INA228 datasheet p. 29)
    dev->power_lsb = 3.2f * dev->current_lsb;
 
    // SHUNT_CAL  = 0.00512 / (current_lsb × Rshunt)  (datasheet eq. 2)
    float    cal_f   = 0.00512f / (dev->current_lsb * shunt_ohms);
    uint16_t cal_reg = (uint16_t)cal_f;
    dev->shunt_cal_reg = cal_reg;
 
    return ina228_write_u16(dev, INA228_REG_SHUNT_CAL, cal_reg);
}


ina228_status_t ina228_set_current_lsb(ina228_t *dev, float current_lsb)
{
    if (!dev || current_lsb <= 0.0f) return INA228_ERR_PARAM;
 
    dev->current_lsb = current_lsb;
    dev->power_lsb   = 3.2f * current_lsb;
 
    float    cal_f   = 0.00512f / (current_lsb * dev->shunt_ohms);
    uint16_t cal_reg = (uint16_t)cal_f;
    dev->shunt_cal_reg = cal_reg;
 
    return ina228_write_u16(dev, INA228_REG_SHUNT_CAL, cal_reg);
}
 
ina228_status_t ina228_set_adc_config(ina228_t *dev, uint16_t adc_cfg)
{
    if (!dev) return INA228_ERR_PARAM;
    dev->adc_config_reg = adc_cfg;
    return ina228_write_u16(dev, INA228_REG_ADC_CONFIG, adc_cfg);
}
 
ina228_status_t ina228_check_id(ina228_t *dev)
{
    uint16_t manuf = 0, device = 0;
    ina228_status_t s;
 
    s = ina228_read_u16(dev, INA228_REG_MANUF_ID,  &manuf);
    if (s != INA228_OK) return s;
 
    s = ina228_read_u16(dev, INA228_REG_DEVICE_ID, &device);
    if (s != INA228_OK) return s;
 
    // MANUF_ID must be 0x5449 ('TI'), DEVICE_ID upper 12 bits must be 0x228
    if (manuf != INA228_MANUF_ID_TI) return INA228_ERR_ID;
    if ((device >> 4) != 0x228)      return INA228_ERR_ID;
 
    return INA228_OK;
}
 
ina228_status_t ina228_reset_accumulators(ina228_t *dev)
{
    // Read current CONFIG, set RSTACC bit, write back
    uint16_t cfg = 0;
    ina228_status_t s = ina228_read_u16(dev, INA228_REG_CONFIG, &cfg);
    if (s != INA228_OK) return s;
 
    cfg |= INA228_REG_CONFIG_RSTACC;
    return ina228_write_u16(dev, INA228_REG_CONFIG, cfg);
}
 
/* ================= ENGINEERING UNITS ================= */

ina228_status_t ina228_get_vbus_V(ina228_t *dev, float *V)
{
    // VBUS: 20-bit signed, LSB = 195.3125 µV
    int32_t raw;
    ina228_status_t s = ina228_read_s20(dev, INA228_REG_VBUS, &raw);
    if (s != INA228_OK) return s;
 
    *V = (float)raw * 195.3125e-6f;
    return INA228_OK;
}

ina228_status_t ina228_get_vshunt_V(ina228_t *dev, float *Vsh)
{
    // VSHUNT: 20-bit signed
    // LSB = 312.5 nV  when ADCRANGE=0 (±163.84 mV range, default)
    // LSB = 78.125 nV when ADCRANGE=1 (±40.96  mV range)
    // We always use the default range here; add ADCRANGE flag to struct if needed.
    int32_t raw;
    ina228_status_t s = ina228_read_s20(dev, INA228_REG_VSHUNT, &raw);
    if (s != INA228_OK) return s;
 
    *Vsh = (float)raw * 312.5e-9f;
    return INA228_OK;
}

ina228_status_t ina228_get_current_A(ina228_t *dev, float *I)
{
    // CURRENT: 20-bit signed, LSB = current_lsb
    int32_t raw;
    ina228_status_t s = ina228_read_s20(dev, INA228_REG_CURRENT, &raw);
    if (s != INA228_OK) return s;
 
    *I = (float)raw * dev->current_lsb;
    return INA228_OK;
}
 
ina228_status_t ina228_get_power_W(ina228_t *dev, float *P)
{
    // POWER: 24-bit UNSIGNED, LSB = 3.2 × current_lsb
    uint32_t raw;
    ina228_status_t s = ina228_read_u24(dev, INA228_REG_POWER, &raw);
    if (s != INA228_OK) return s;
 
    *P = (float)raw * dev->power_lsb;
    return INA228_OK;
}
 
ina228_status_t ina228_get_dietemp_C(ina228_t *dev, float *Tc)
{
    // DIETEMP: 16-bit signed, upper 12 bits are integer, lower 4 are fraction
    // LSB = 7.8125 m°C  (= 1/128 °C)
    uint16_t raw16;
    ina228_status_t s = ina228_read_u16(dev, INA228_REG_DIETEMP, &raw16);
    if (s != INA228_OK) return s;
 
    int16_t raw = (int16_t)raw16;
    *Tc = (float)raw * 7.8125e-3f;
    return INA228_OK;
}
 
ina228_status_t ina228_get_energy_J(ina228_t *dev, double *J)
{
    // ENERGY: 40-bit signed accumulator
    // LSB = 16 × power_lsb (datasheet p. 31)
    int64_t raw;
    ina228_status_t s = ina228_read_s40(dev, INA228_REG_ENERGY, &raw);
    if (s != INA228_OK) return s;
 
    *J = (double)raw * 16.0 * (double)dev->power_lsb;
    return INA228_OK;
}
 
ina228_status_t ina228_get_charge_C(ina228_t *dev, double *Coul)
{
    // CHARGE: 40-bit signed accumulator
    // LSB = current_lsb  (datasheet p. 31)
    int64_t raw;
    ina228_status_t s = ina228_read_s40(dev, INA228_REG_CHARGE, &raw);
    if (s != INA228_OK) return s;
    *Coul = (double)raw * (double)dev->current_lsb;
    return INA228_OK;
}