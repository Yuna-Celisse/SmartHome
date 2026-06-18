#ifndef SENSOR_TMP116_H
#define SENSOR_TMP116_H

#include "board_init.h"

/* BOOSTXL-BASSENSORS: ADD0 tied to VCC → 0x49 */
#define TMP116_I2C_ADDR       0x49

/* TMP116 registers */
#define TMP116_REG_TEMP         0x00
#define TMP116_REG_CONFIG       0x01
#define TMP116_REG_T_LOW        0x02
#define TMP116_REG_T_HIGH       0x03
#define TMP116_REG_DEVICE_ID    0x07

/* Configuration bits */
#define TMP116_CONFIG_AVG_8     0x0000   /* 8 averaged conversions (default) */
#define TMP116_CONFIG_CONV_125MS 0x0080  /* 125ms conversion time */
#define TMP116_CONFIG_MODE_CONT 0x0000   /* continuous conversion */
#define TMP116_CONFIG_MODE_SD   0x0400   /* shutdown mode */

/* Expected device ID */
#define TMP116_DEVICE_ID_VAL    0x0116

int   TMP116_Init(void);
float TMP116_ReadTemperature(void);
uint16_t TMP116_ReadDeviceID(void);

#endif /* SENSOR_TMP116_H */
