/******************************************************************************
 * @file sensor_bmi160.h
 *
 * @par dependencies
 *      - board_init.h (I2C read helpers)
 *
 * @author Yuna-Celisse
 *
 * @brief BMI160 IMU temperature sensor interface (BOOSTXL-SENSORS).
 *
 * The BMI160 is a 6-axis IMU with an internal temperature sensor.
 * Temperature is a signed 16-bit value at registers 0x20-0x21.
 *
 * @version V1.0 2026-6-19
 *
 * @note 1 tab == 4 spaces!
 *****************************************************************************/

#ifndef SENSOR_BMI160_H
#define SENSOR_BMI160_H

#include "board_init.h"

/* BOOSTXL-SENSORS: SDO=VDD → 0x69 */
#define BMI160_I2C_ADDR         0x69

/* BMI160 registers */
#define BMI160_REG_CHIP_ID      0x00
#define BMI160_REG_TEMP_MSB     0x20
#define BMI160_REG_TEMP_LSB     0x21
#define BMI160_REG_CMD          0x7E

/* Expected chip ID */
#define BMI160_CHIP_ID_VAL      0xD1

/* Commands */
#define BMI160_CMD_ACC_NORMAL   0x11
#define BMI160_CMD_SOFT_RESET   0xB6

/**
 * @brief  Initialize BMI160: verify chip ID, set accelerometer to normal mode.
 * @return 0 on success, -1 on failure.
 */
int BMI160_Init(void);

/**
 * @brief  Read internal temperature in degrees Celsius.
 *
 * Temperature (°C) = 23.0 + raw / 512.0
 * Range: -40 to +85 °C, resolution ~0.002 °C.
 *
 * @return Temperature in °C.
 */
float BMI160_ReadTemperature(void);

#endif /* SENSOR_BMI160_H */
