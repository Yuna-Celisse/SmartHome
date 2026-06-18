/******************************************************************************
 * @file sensor_tmp007.h
 *
 * @par dependencies
 *      - board_init.h (I2C read/write helpers)
 *
 * @author Yuna-Celisse
 *
 * @brief TMP007 IR thermopile sensor driver interface.
 *
 * TMP007 is a contactless IR temperature sensor with integrated math
 * engine. Communication over I2C, 14-bit effective temperature data.
 *
 * @version V1.0 2026-6-19
 *
 * @note 1 tab == 4 spaces!
 *****************************************************************************/

#ifndef SENSOR_TMP007_H
#define SENSOR_TMP007_H

#include "board_init.h"

/* BOOSTXL-BASSENSORS: ADR1=V+, ADR0=SCL → 0x47 */
#define TMP007_I2C_ADDR         0x47

/* TMP007 registers */
#define TMP007_REG_V_SENSOR      0x00
#define TMP007_REG_T_DIE         0x01
#define TMP007_REG_CONFIG        0x02
#define TMP007_REG_T_OBJ         0x03
#define TMP007_REG_STATUS        0x04
#define TMP007_REG_MFR_ID        0x1E
#define TMP007_REG_DEVICE_ID     0x1F

/* Configuration bits */
#define TMP007_CONFIG_RESET      0x8000
#define TMP007_CONFIG_MODE_CONT  0x1800  /* MOD[12:11] = 11 = continuous */
#define TMP007_CONFIG_CR_1HZ     0x0200  /* CR[10:9] = 01 = 1 sample/sec */
#define TMP007_CONFIG_EN_DRDY    0x0040  /* Enable Data Ready */

/* Expected identification values */
#define TMP007_MFR_ID_VAL        0x5449  /* "TI" */
#define TMP007_DEVICE_ID_VAL     0x0078

/**
 * @brief  Initialize TMP007: verify device ID, configure continuous mode.
 * @return 0 on success, -1 if device ID mismatch.
 */
int TMP007_Init(void);

/**
 * @brief  Read object (IR target) temperature in degrees Celsius.
 * @return Temperature in °C, 0.03125°C resolution.
 */
float TMP007_ReadTemperature(void);

/**
 * @brief  Read local die temperature in degrees Celsius.
 * @return Die temperature in °C.
 */
float TMP007_ReadDieTemperature(void);

#endif /* SENSOR_TMP007_H */
