/******************************************************************************
 * @file sensor_bmi160.c
 *
 * @par dependencies
 *      - board_init.h (Board_I2C_Write/ReadReg, delay_cycles)
 *      - sensor_bmi160.h (this module's interface)
 *
 * @author Yuna-Celisse
 *
 * @brief BMI160 IMU temperature sensor driver for BOOSTXL-SENSORS.
 *
 * The BMI160's internal temperature sensor provides die temperature
 * as a signed 16-bit value. The sensor is always active when the
 * accelerometer is in normal mode.
 *
 * Temperature formula: T(°C) = 23.0 + raw / 512.0
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

    /* Set accelerometer to normal mode to enable temperature sensor */
    bmi160_write_reg(BMI160_REG_CMD, BMI160_CMD_ACC_NORMAL);

    /* Wait for power-up and sensor stabilization */
    delay_cycles(1600000); /* ~50ms */

    return 0;
}

float BMI160_ReadTemperature(void)
{
    uint8_t msb, lsb;
    int16_t raw;

    /*
     * Read MSB and LSB in two separate I2C transactions.
     * The BMI160 on BOOSTXL-SENSORS appears to have an issue
     * with multi-byte reads (auto-increment returning wrong data).
     * Single-byte reads produce consistent results.
     */
    bmi160_read_reg(BMI160_REG_TEMP_MSB, &msb, 1);
    delay_cycles(1600); /* ~50us settle between reads */
    bmi160_read_reg(BMI160_REG_TEMP_LSB, &lsb, 1);

    raw = (int16_t)(((uint16_t)msb << 8) | lsb);
    return 23.0f + (float)raw / 512.0f;
}
