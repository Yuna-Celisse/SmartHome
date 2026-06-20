/******************************************************************************
 * @file sensor_bmi160.c
 *
 * @par dependencies
 *      - board_init.h (SENSOR_I2C, delay_cycles, DL I2C API)
 *      - sensor_bmi160.h (this module's interface)
 *
 * @author Yuna-Celisse
 *
 * @brief BMI160 IMU gyroscope driver for BOOSTXL-SENSORS.
 *
 * Uses raw DL I2C API (DL_I2C_fillControllerTXFIFO /
 * DL_I2C_startControllerTransfer / DL_I2C_receiveControllerData)
 * to bypass Board_I2C_ReadReg/WriteReg which produce corrupted
 * register values on this board.
 *
 * Reads 3-axis gyroscope angular rate (0x0C-0x11) and converts
 * to °/s using sensitivity = 16.384 LSB/°/s (default ±2000°/s range).
 * Gyro is started in normal mode during init.
 *
 * Multi-byte I2C reads are unreliable on this bus, so each of the
 * 6 gyro data bytes is read individually with a ~50µs settle delay.
 *
 * @version V1.1 2026-6-20
 *
 * @note 1 tab == 4 spaces!
 *****************************************************************************/

#include "sensor_bmi160.h"
#include "ti_msp_dl_config.h" /* DL_I2C API, delay_cycles */

/* ---- Pre-built I2C packets ---- */
static const uint8_t bmi160_chip_id_reg = BMI160_REG_CHIP_ID;
static const uint8_t bmi160_gyr_cmd_packet[2] = {
    BMI160_REG_CMD,
    BMI160_CMD_GYR_NORMAL
};

/* ---- I2C Helpers ---- */

static void bmi160_wait_idle(void)
{
    while (!(DL_I2C_getControllerStatus(SENSOR_I2C)
             & DL_I2C_CONTROLLER_STATUS_IDLE)) {
    }
}

static void bmi160_wait_bus_free(void)
{
    while (DL_I2C_getControllerStatus(SENSOR_I2C)
           & DL_I2C_CONTROLLER_STATUS_BUSY_BUS) {
    }
}

/**
 * @brief  Read len bytes from register reg using raw DL I2C API.
 *
 * Performs a write-then-read sequence: first sends the register
 * address in a TX transfer, then reads len bytes in an RX transfer.
 */
static void bmi160_read_reg(uint8_t reg, uint8_t *data, uint8_t len)
{
    /* Step 1: write register address */
    bmi160_wait_idle();
    DL_I2C_fillControllerTXFIFO(SENSOR_I2C, &reg, 1);
    DL_I2C_startControllerTransfer(SENSOR_I2C,
        BMI160_I2C_ADDR,
        DL_I2C_CONTROLLER_DIRECTION_TX, 1);
    bmi160_wait_bus_free();

    /* Step 2: read data */
    bmi160_wait_idle();
    DL_I2C_startControllerTransfer(SENSOR_I2C,
        BMI160_I2C_ADDR,
        DL_I2C_CONTROLLER_DIRECTION_RX, len);

    for (uint8_t i = 0; i < len; i++) {
        while (DL_I2C_isControllerRXFIFOEmpty(SENSOR_I2C)) {
        }
        data[i] = DL_I2C_receiveControllerData(SENSOR_I2C);
    }
    bmi160_wait_bus_free();
}

/**
 * @brief  Write a value to a register using raw DL I2C API.
 */
static void bmi160_write_reg(uint8_t reg, uint8_t value)
{
    uint8_t buf[2];
    buf[0] = reg;
    buf[1] = value;

    bmi160_wait_idle();
    DL_I2C_fillControllerTXFIFO(SENSOR_I2C, buf, 2);
    DL_I2C_startControllerTransfer(SENSOR_I2C,
        BMI160_I2C_ADDR,
        DL_I2C_CONTROLLER_DIRECTION_TX, 2);
    bmi160_wait_bus_free();
}

/* ---- Public API ---- */

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
