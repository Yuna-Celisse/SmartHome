/******************************************************************************
 * @file sensor_bmi160.h
 *
 * @par dependencies
 *      - board_init.h (I2C read helpers)
 *
 * @author Yuna-Celisse
 *
 * @brief BMI160 IMU gyroscope interface (BOOSTXL-SENSORS).
 *
 * The BMI160 is a 6-axis IMU. This driver reads 3-axis gyroscope
 * angular rate from registers 0x0C-0x11 (16-bit, LSB first).
 * Gyro sensitivity: 16.384 LSB/°/s at default ±2000°/s range.
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
#define BMI160_REG_GYR_X_LSB    0x0C /* GYR X/Y/Z: 0x0C-0x11, LSB first */
#define BMI160_REG_GYR_Y_LSB    0x0E
#define BMI160_REG_GYR_Z_LSB    0x10
#define BMI160_REG_TEMP_MSB     0x20
#define BMI160_REG_TEMP_LSB     0x21
#define BMI160_REG_GYR_RANGE    0x43
#define BMI160_REG_CMD          0x7E

/* Expected chip ID */
#define BMI160_CHIP_ID_VAL      0xD1

/* Gyro range (0x43): default = ±2000°/s, sensitivity = 16.384 LSB/°/s */
#define BMI160_GYR_RANGE_2000   0x00
#define BMI160_GYR_SENSITIVITY  16.384f

/* Commands */
#define BMI160_CMD_ACC_NORMAL   0x11
#define BMI160_CMD_GYR_NORMAL   0x15
#define BMI160_CMD_SOFT_RESET   0xB6

/**
 * @brief  Initialize BMI160: verify chip ID, start gyroscope in normal mode.
 * @return 0 on success, -1 on failure.
 */
int BMI160_Init(void);

/**
 * @brief  Read 3-axis gyroscope angular rate in degrees per second.
 *
 * Reads GYR_X/Y/Z (0x0C-0x11, 6 bytes, LSB first) and converts to °/s
 * using sensitivity = 16.384 LSB/°/s (default range ±2000°/s).
 *
 * Range: ±2000 °/s, resolution ~0.061 °/s.
 *
 * @param[out] gx  X-axis angular rate (°/s).
 * @param[out] gy  Y-axis angular rate (°/s).
 * @param[out] gz  Z-axis angular rate (°/s).
 */
void BMI160_ReadGyro(float *gx, float *gy, float *gz);

#endif /* SENSOR_BMI160_H */
