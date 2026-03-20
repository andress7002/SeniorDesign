#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <math.h>

#include "ina228.h"

/////////////

ina228_status_t ina228_write_u16(ina228_t *dev, uint8_t reg, uint16_t val)
{
    uint8_t tx[3];
    tx[0] = reg;
    tx[1] = (val >> 8) & 0xFF;
    tx[2] = val & 0xFF;

    if (HAL_I2C_Master_Transmit(dev->hi2c,
                                dev->addr << 1,
                                tx,
                                3,
                                HAL_MAX_DELAY) != HAL_OK)
        return INA228_ERR_I2C;

    return INA228_OK;
}

ina228_status_t ina228_read_u16(ina228_t *dev, uint8_t reg, uint16_t *out)
{
    uint8_t rx[2];

    if (HAL_I2C_Master_Transmit(dev->hi2c,
                                dev->addr << 1,
                                &reg,
                                1,
                                HAL_MAX_DELAY) != HAL_OK)
        return INA228_ERR_I2C;

    if (HAL_I2C_Master_Receive(dev->hi2c,
                               dev->addr << 1,
                               rx,
                               2,
                               HAL_MAX_DELAY) != HAL_OK)
        return INA228_ERR_I2C;

    *out = ((uint16_t)rx[0] << 8) | rx[1];
    return INA228_OK;
}

ina228_status_t ina228_read_s24(ina228_t *dev, uint8_t reg, int32_t *out)
{
    uint8_t rx[3];

    if (HAL_I2C_Master_Transmit(dev->hi2c, dev->addr << 1, &reg, 1, HAL_MAX_DELAY) != HAL_OK)
        return INA228_ERR_I2C;

    if (HAL_I2C_Master_Receive(dev->hi2c, dev->addr << 1, rx, 3, HAL_MAX_DELAY) != HAL_OK)
        return INA228_ERR_I2C;

    int32_t val = ((int32_t)rx[0] << 16) |
                  ((int32_t)rx[1] << 8) |
                  rx[2];

    if (val & 0x800000)
        val |= 0xFF000000;

    *out = val;
    return INA228_OK;
}

/* ================= INITIALIZATION ================= */

ina228_status_t ina228_init (ina228_t *dev, I2C_HandleTypeDef *hi2c, uint8_t addr7, float shunt_ohms, float max_expected_current_A)
{
    dev->hi2c = hi2c;
    dev->addr = addr7;
    dev->shunt_ohms = shunt_ohms;

    dev->current_lsb = max_expected_current_A / 524288.0f; //current_lsb = Imax/2^19

    float cal = 0.00512f / (dev->current_lsb * shunt_ohms); //SHUNT_CAL = 0.00512 / (Current_LSB × RSHUNT)
    uint16_t cal_reg = (uint16_t)cal;

    return ina228_write_u16(dev, INA228_REG_SHUNT_CAL, cal_reg);
}

/* ================= ENGINEERING UNITS ================= */

ina228_status_t ina228_get_vbus_V(ina228_t *dev, float *V)
{
    int32_t raw;

    if (ina228_read_s24(dev, INA228_REG_VBUS, &raw) != INA228_OK)
        return INA228_ERR_I2C;

    *V = raw * 195.3125e-6f;  // 195.3125 µV per LSB
    return INA228_OK;
}

ina228_status_t ina228_get_current_A(ina228_t *dev, float *I)
{
    int32_t raw;

    if (ina228_read_s24(dev, INA228_REG_CURRENT, &raw) != INA228_OK)
        return INA228_ERR_I2C;

    *I = raw * dev->current_lsb;
    return INA228_OK;
}

ina228_status_t ina228_get_power_W(ina228_t *dev, float *P)
{
    int32_t raw;

    if (ina228_read_s24(dev, INA228_REG_POWER, &raw) != INA228_OK)
        return INA228_ERR_I2C;

    *P = raw * (3.2f * dev->current_lsb);
    return INA228_OK;
}