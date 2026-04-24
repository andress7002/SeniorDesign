#pragma once
#include <stdint.h>
#include <stddef.h>

// Register addresses (Datasheet Rev. May 2022 p. 21)
#define INA228_REG_CONFIG        0x00U
#define INA228_REG_ADC_CONFIG    0x01U
#define INA228_REG_SHUNT_CAL     0x02U
#define INA228_REG_SHUNT_TEMPCO  0x03U
#define INA228_REG_VSHUNT        0x04U
#define INA228_REG_VBUS          0x05U
#define INA228_REG_DIETEMP       0x06U
#define INA228_REG_CURRENT       0x07U
#define INA228_REG_POWER         0x08U
#define INA228_REG_ENERGY        0x09U
#define INA228_REG_CHARGE        0x0AU
#define INA228_REG_DIAG_ALRT     0x0BU
#define INA228_REG_SOVL          0x0CU
#define INA228_REG_SUVL          0x0DU
#define INA228_REG_BOVL          0x0EU
#define INA228_REG_BUVL          0x0FU
#define INA228_REG_TEMP_LIMIT    0x10U
#define INA228_REG_PWR_LIMIT     0x11U
#define INA228_REG_MANUF_ID      0x3EU
#define INA228_REG_DEVICE_ID     0x3FU

//Bit masks CONFIG (Datasheet Rev. May 2022 p. 23-24) (CONFIG 0h)
#define INA228_REG_RST_SYSTEMRESET          0x8000U
#define INA228_REG_CONFIG_RSTACC            0x4000U
#define INA228_REG_CONFIG_TEMPCOMP_EN       0x0020U
#define INA228_REG_CONFIG_TEMPCOMP_DIS      0x0000U
#define INA228_REG_CONFIG_ADCRANGE_16384MV  0x0000U
#define INA228_REG_CONFIG_ADCRANGE_4096MV   0x0010U

// ============================================================
//  ADC_CONFIG register fields  (ADC_CONFIG 1h, p. 23-24)
// ============================================================
// Operating mode
#define INA228_REG_MODE_SHUTDOWN           0x0000U
#define INA228_REG_MODE_TRIG_BUS           0x1000U
#define INA228_REG_MODE_TRIG_SHUNT         0x2000U
#define INA228_REG_MODE_TRIG_BUS_SHUNT     0x3000U
#define INA228_REG_MODE_TRIG_TEMP          0x4000U
#define INA228_REG_MODE_TRIG_TEMP_BUS      0x5000U
#define INA228_REG_MODE_TRIG_TEMP_SHUNT    0x6000U
#define INA228_REG_MODE_TRIG_ALL           0x7000U
#define INA228_REG_MODE_CONT_BUS           0x9000U
#define INA228_REG_MODE_CONT_SHUNT         0xA000U
#define INA228_REG_MODE_CONT_BUS_SHUNT     0xB000U
#define INA228_REG_MODE_CONT_TEMP          0xC000U
#define INA228_REG_MODE_CONT_TEMP_BUS      0xD000U
#define INA228_REG_MODE_CONT_TEMP_SHUNT    0xE000U
#define INA228_REG_MODE_CONT_ALL           0xF000U   // Continuous shunt+bus+temp (recommended)

// VBUS conversion time
#define INA228_REG_VBUSCT_50US             0x0000U
#define INA228_REG_VBUSCT_84US             0x0200U
#define INA228_REG_VBUSCT_150US            0x0400U
#define INA228_REG_VBUSCT_280US            0x0600U
#define INA228_REG_VBUSCT_540US            0x0800U
#define INA228_REG_VBUSCT_1052US           0x0A00U
#define INA228_REG_VBUSCT_2074US           0x0C00U
#define INA228_REG_VBUSCT_4120US           0x0E00U
 
// VSHUNT conversion time
#define INA228_REG_VSHCT_50US              0x0000U
#define INA228_REG_VSHCT_84US              0x0040U
#define INA228_REG_VSHCT_150US             0x0080U
#define INA228_REG_VSHCT_280US             0x00C0U
#define INA228_REG_VSHCT_540US             0x0100U
#define INA228_REG_VSHCT_1052US            0x0140U
#define INA228_REG_VSHCT_2074US            0x0180U
#define INA228_REG_VSHCT_4120US            0x01C0U
 
// VTEMP conversion time
#define INA228_REG_VTCT_50US               0x0000U
#define INA228_REG_VTCT_84US               0x0008U
#define INA228_REG_VTCT_150US              0x0010U
#define INA228_REG_VTCT_280US              0x0018U
#define INA228_REG_VTCT_540US              0x0020U
#define INA228_REG_VTCT_1052US             0x0028U
#define INA228_REG_VTCT_2074US             0x0030U
#define INA228_REG_VTCT_4120US             0x0038U
 
// Averaging count
#define INA228_REG_AVG_1                   0x0000U
#define INA228_REG_AVG_4                   0x0001U
#define INA228_REG_AVG_16                  0x0002U
#define INA228_REG_AVG_64                  0x0003U
#define INA228_REG_AVG_128                 0x0004U
#define INA228_REG_AVG_256                 0x0005U
#define INA228_REG_AVG_512                 0x0006U
#define INA228_REG_AVG_1024                0x0007U
 
// ============================================================
//  DIAG_ALRT register bit masks  (DIAG_ALRT 0Bh)
// ============================================================
#define INA228_REG_DIAG_ALATCH             0x8000U   // Alert latch enable
#define INA228_REG_DIAG_CNVR               0x4000U   // Conversion-ready alert
#define INA228_REG_DIAG_SLOWALERT          0x2000U   // Slow alert
#define INA228_REG_DIAG_APOL               0x1000U   // Alert polarity (1=active-high)
#define INA228_REG_DIAG_ENERGYOF           0x0800U   // Energy accumulator overflow
#define INA228_REG_DIAG_CHARGEOF           0x0400U   // Charge accumulator overflow
#define INA228_REG_DIAG_MATHOF             0x0200U   // Math overflow
#define INA228_REG_DIAG_TMPOL              0x0080U   // Temp over-limit
#define INA228_REG_DIAG_SHNTOL             0x0040U   // Shunt over-limit
#define INA228_REG_DIAG_SHNTUL             0x0020U   // Shunt under-limit
#define INA228_REG_DIAG_BUSOL              0x0010U   // Bus over-voltage
#define INA228_REG_DIAG_BUSUL              0x0008U   // Bus under-voltage
#define INA228_REG_DIAG_POL                0x0004U   // Power over-limit
#define INA228_REG_DIAG_CNVRF              0x0002U   // Conversion ready flag (read-only)
#define INA228_REG_DIAG_MEMSTAT            0x0001U   // Memory checksum status (1=OK)
 
// ============================================================
//  Known ID values
// ============================================================
#define INA228_MANUF_ID_TI             0x5449U   // 'TI'
#define INA228_DEVICE_ID_INA228        0x2281U   // INA228 device ID (upper 12 bits = 0x228)