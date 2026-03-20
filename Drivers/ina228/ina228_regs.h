#include <stddef.h>
#include <stdint.h>

#pragma once
#include <stdint.h>

// Register addresses (Datasheet Rev. May 2022 p. 21)
#define CONFIG        0x00U
#define ADC_CONFIG    0x01U
#define SHUNT_CAL     0x02U
#define SHUNT_TEMPCO  0x03U
#define VSHUNT        0x04U
#define VBUS          0x05U
#define DIETEMP       0x06U
#define CURRENT       0x07U
#define POWER         0x08U
#define ENERGY        0x09U
#define CHARGE        0x0AU
#define DIAG_ALRT     0x0BU
#define SOVL          0x0CU
#define SUVL          0x0DU
#define BOVL          0x0EU
#define BUVL          0x0FU
#define TEMP_LIMIT    0x10U
#define PWR_LIMIT     0x11U
#define MANUF_ID      0x3EU
#define DEVICE_ID     0x3FU

//Bit masks CONFIG (Datasheet Rev. May 2022 p. 23-24) (CONFIG 0h)
#define RST_SYSTEMRESET   0x8000U
#define CONFIG_RSTACC_CLEAR      0x4000U
#define CONFIG_TEMPCOMP_EN       0x0020U
#define CONFIG_TEMPCOMP_EN       0x0000U
#define CONFIG_ADCRANGE_16384MV  0x0000U
#define CONFIG_ADCRANGE_4096MV   0x0010U

//Fields ADC_CONFIG (Datasheet Rev. May 2022 p. 23-24) (ADC_CONFIG 1h)
#define MODE_CONT_SHUNT_BUS_TEMP 0xF000U
#define ADC_VBUSCT_1052US            0x0A00U
#define ADC_VSHCT_1052US             0x0140U
#define ADC_VTCT_1052US              0x0028U
#define ADC_AVG_64                   0x0003U
