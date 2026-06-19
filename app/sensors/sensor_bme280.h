/******************************************************************************
 * @file sensor_bme280.h
 *
 * @par dependencies
 *      - board_init.h (I2C read helpers)
 *
 * @author Yuna-Celisse
 *
 * @brief BME280 environmental sensor interface (BOOSTXL-SENSORS).
 *
 * The BME280 is a combined humidity, pressure, and temperature sensor.
 * This driver implements environmental temperature measurement with
 * Bosch compensation formula. Humidity and pressure are not read
 * in this MVP to keep code size small.
 *
 * Temperature formula (Bosch compensation):
 *   var1 = ((raw/16384.0) - (dig_T1/1024.0)) * dig_T2
 *   var2 = ((raw/131072.0) - (dig_T1/8192.0))^2 * dig_T3
 *   t_fine = var1 + var2
 *   T(°C)  = t_fine / 5120.0
 *
 * @version V1.0 2026-6-19
 *
 * @note 1 tab == 4 spaces!
 *****************************************************************************/

#ifndef SENSOR_BME280_H
#define SENSOR_BME280_H

#include "board_init.h"

/*
 * BOOSTXL-SENSORS: BME280 I2C address.
 * 0x76 when SDO pin is tied to GND (most common).
 * Change to 0x77 if SDO is tied to VDDIO on your board.
 */
#define BME280_I2C_ADDR             0x76
#define BME280_I2C_ADDR_ALT         0x77

/* BME280 registers */
#define BME280_REG_CALIB_T1_LSB     0x88 /* calib block: 0x88-0x8D */
#define BME280_REG_CHIP_ID          0xD0
#define BME280_REG_CTRL_HUM         0xF2
#define BME280_REG_STATUS           0xF3
#define BME280_REG_CTRL_MEAS        0xF4
#define BME280_REG_CONFIG           0xF5
#define BME280_REG_TEMP_MSB         0xFA /* 3 bytes: 0xFA(MSB), 0xFB(LSB), 0xFC(XLSB) */

/* Expected chip ID */
#define BME280_CHIP_ID_VAL          0x60

/*
 * ctrl_meas (0xF4) bit layout:
 *   [7:5] osrs_t  — temperature oversampling
 *   [4:2] osrs_p  — pressure oversampling
 *   [1:0] mode    — sensor mode
 */
#define BME280_OSRS_T_1             (0x01 << 5) /* 1x oversampling */
#define BME280_OSRS_P_SKIP          (0x00 << 2) /* pressure skipped */
#define BME280_MODE_SLEEP           0x00
#define BME280_MODE_FORCED          0x01
#define BME280_MODE_NORMAL          0x03

/* config (0xF5) bit layout: t_sb[7:5], filter[4:2], spi3w_en[0] */
#define BME280_STANDBY_0_5MS        (0x00 << 5)
#define BME280_FILTER_OFF           (0x00 << 2)

/* ctrl_hum (0xF2): osrs_h[2:0] */
#define BME280_OSRS_H_1             0x01

/* Calibration coefficients — stored for compensation */
typedef struct {
    uint16_t dig_T1;
    int16_t  dig_T2;
    int16_t  dig_T3;
} Bme280Calib;

/**
 * @brief  Initialize BME280: verify chip ID, read calibration, set
 *         normal mode with 1x temp oversampling.
 * @return 0 on success, -1 on chip ID mismatch.
 */
int BME280_Init(void);

/**
 * @brief  Read compensated environmental temperature in degrees Celsius.
 *
 * Reads raw 20-bit temperature ADC value (3 bytes at 0xFA-0xFC)
 * and applies Bosch compensation formula using calibration data
 * loaded during BME280_Init().
 *
 * Range: -40 to +85 °C, resolution 0.01 °C.
 *
 * @return Temperature in °C.
 */
float BME280_ReadTemperature(void);

#endif /* SENSOR_BME280_H */
