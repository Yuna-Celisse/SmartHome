#ifndef SENSOR_OPT3001_H
#define SENSOR_OPT3001_H

#include "board_init.h"

#define OPT3001_I2C_ADDR      0x44

/* OPT3001 registers */
#define OPT3001_REG_RESULT         0x00
#define OPT3001_REG_CONFIG         0x01
#define OPT3001_REG_LOW_LIMIT      0x02
#define OPT3001_REG_HIGH_LIMIT     0x03
#define OPT3001_REG_MANUFACTURER   0x7E
#define OPT3001_REG_DEVICE_ID      0x7F

/* Configuration */
#define OPT3001_CONFIG_AUTO_RANGE  0x0C00  /* automatic full-scale range */
#define OPT3001_CONFIG_CONT_MODE   0x0600  /* continuous conversion */
#define OPT3001_CONFIG_CONV_800MS  0x0800  /* 800ms conversion time */

/* Expected IDs */
#define OPT3001_MANUFACTURER_ID    0x5449
#define OPT3001_DEVICE_ID_VAL      0x3001

int   OPT3001_Init(void);
float OPT3001_ReadLux(void);
uint16_t OPT3001_ReadManufacturerID(void);
uint16_t OPT3001_ReadDeviceID(void);

#endif /* SENSOR_OPT3001_H */
