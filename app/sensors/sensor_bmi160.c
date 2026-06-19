/******************************************************************************
 * @file sensor_bmi160.c
 *
 * @par dependencies
 *      - board_init.h (Board_I2C_Write/ReadReg, delay_cycles)
 *      - sensor_bmi160.h (this module's interface)
 *
 * @author Yuna-Celisse
 *
 * @brief BMI160 IMU gyroscope driver for BOOSTXL-SENSORS.
 *
 * Reads 3-axis gyroscope angular rate (0x0C-0x11) and converts
 * to °/s using sensitivity = 16.384 LSB/°/s (default ±2000°/s range).
 * Gyro is started in normal mode during init.
 *
 * @version V1.0 2026-6-19
 *
 * @note 1 tab == 4 spaces!
 *****************************************************************************/

#include "sensor_bmi160.h"

static void bmi160_read_reg(uint8_t reg, uint8_t *data, uint8_t len)
{
    Board_I2C_ReadReg(BMI160_I2C_ADDR, reg, data, len);
}

static void bmi160_write_reg(uint8_t reg, uint8_t value)
{
    Board_I2C_WriteReg(BMI160_I2C_ADDR, reg, value);
}

int BMI160_Init(void)
{
    uint8_t chipId;

    /* Verify chip ID */
    bmi160_read_reg(BMI160_REG_CHIP_ID, &chipId, 1);
    if (chipId != BMI160_CHIP_ID_VAL) {
        return -1;
    }

    /* Start gyroscope in normal mode to enable angular rate output */
    bmi160_write_reg(BMI160_REG_CMD, BMI160_CMD_GYR_NORMAL);

    /* Wait for gyro power-up and stabilization (~80ms) */
    delay_cycles(2560000);

    return 0;
}

void BMI160_ReadGyro(float *gx, float *gy, float *gz)
{
    uint8_t xLsb, xMsb, yLsb, yMsb, zLsb, zMsb;
    int16_t rawX, rawY, rawZ;

    /*
     * Read all 6 gyro data bytes with single-byte I2C reads.
     * Multi-byte reads on this board are unreliable (auto-increment
     * returns corrupted data ~50% of the time), so each register
     * is read individually with a short settle delay between reads.
     * GYR data format: LSB first at even address, MSB at odd address.
     */

    bmi160_read_reg(BMI160_REG_GYR_X_LSB,     &xLsb, 1);
    delay_cycles(1600); /* ~50us settle */
    bmi160_read_reg(BMI160_REG_GYR_X_LSB + 1, &xMsb, 1);
    delay_cycles(1600);

    bmi160_read_reg(BMI160_REG_GYR_Y_LSB,     &yLsb, 1);
    delay_cycles(1600);
    bmi160_read_reg(BMI160_REG_GYR_Y_LSB + 1, &yMsb, 1);
    delay_cycles(1600);

    bmi160_read_reg(BMI160_REG_GYR_Z_LSB,     &zLsb, 1);
    delay_cycles(1600);
    bmi160_read_reg(BMI160_REG_GYR_Z_LSB + 1, &zMsb, 1);

    /* LSB first: raw = LSB | (MSB << 8) */
    rawX = (int16_t)(((uint16_t)xMsb << 8) | xLsb);
    rawY = (int16_t)(((uint16_t)yMsb << 8) | yLsb);
    rawZ = (int16_t)(((uint16_t)zMsb << 8) | zLsb);

    *gx = (float)rawX / BMI160_GYR_SENSITIVITY;
    *gy = (float)rawY / BMI160_GYR_SENSITIVITY;
    *gz = (float)rawZ / BMI160_GYR_SENSITIVITY;
}
