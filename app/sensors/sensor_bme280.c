/******************************************************************************
 * @file sensor_bme280.c
 *
 * @par dependencies
 *      - board_init.h (SENSOR_I2C, delay_cycles, DL I2C API)
 *      - sensor_bme280.h (this module's interface)
 *
 * @author Yuna-Celisse
 *
 * @brief BME280 environmental temperature sensor driver for
 *        BOOSTXL-SENSORS.
 *
 * All I2C operations use raw DL I2C API to bypass the
 * Board_I2C_WriteReg/ReadReg wrappers which produce corrupted
 * register values on this board.
 *
 * Data reads use an 8-byte burst from 0xF7 to trigger the BME280's
 * internal shadow-latch, guaranteeing all bytes belong to the same
 * measurement. Retries up to 5 times with MSB range validation.
 *
 * Temperature compensation (Bosch BME280 datasheet §4.2.3):
 *   var1 = ((raw/16384.0) - (T1/1024.0)) * T2
 *   var2 = ((raw/131072.0) - (T1/8192.0))^2 * T3
 *   t_fine = var1 + var2
 *   T = t_fine / 5120.0
 *
 * @version V1.1 2026-6-20
 *
 * @note 1 tab == 4 spaces!
 *****************************************************************************/

#include "sensor_bme280.h"
#include "ti_msp_dl_config.h" /* DL_I2C API, delay_cycles */

static uint8_t g_bme280_addr = 0;
static Bme280Calib g_bme280_calib;

/* ---- I2C Helpers ---- */

static void bme280_wait_idle(void)
{
    while (!(DL_I2C_getControllerStatus(SENSOR_I2C)
             & DL_I2C_CONTROLLER_STATUS_IDLE)) {
    }
}

static void bme280_wait_bus_free(void)
{
    while (DL_I2C_getControllerStatus(SENSOR_I2C)
           & DL_I2C_CONTROLLER_STATUS_BUSY_BUS) {
    }
}

/**
 * @brief  Read len bytes from register reg using raw DL I2C write-then-read.
 */
static void bme280_read_reg(uint8_t reg, uint8_t *data, uint8_t len)
{
    /* Step 1: write register address */
    bme280_wait_idle();
    DL_I2C_fillControllerTXFIFO(SENSOR_I2C, &reg, 1);
    DL_I2C_startControllerTransfer(SENSOR_I2C,
        g_bme280_addr,
        DL_I2C_CONTROLLER_DIRECTION_TX, 1);
    bme280_wait_bus_free();

    /* Step 2: read data */
    bme280_wait_idle();
    DL_I2C_startControllerTransfer(SENSOR_I2C,
        g_bme280_addr,
        DL_I2C_CONTROLLER_DIRECTION_RX, len);

    for (uint8_t i = 0; i < len; i++) {
        while (DL_I2C_isControllerRXFIFOEmpty(SENSOR_I2C)) {
        }
        data[i] = DL_I2C_receiveControllerData(SENSOR_I2C);
    }
    bme280_wait_bus_free();
}

/**
 * @brief  Write register value using raw DL I2C API.
 *
 * Direct DL I2C API calls bypass the Board_I2C_WriteReg corruption
 * (confirmed: write 0x23 to ctrl_meas, readback got 0x35 via wrapper).
 */
static void bme280_write_reg(uint8_t reg, uint8_t value)
{
    uint8_t buf[2];
    buf[0] = reg;
    buf[1] = value;

    bme280_wait_idle();
    DL_I2C_fillControllerTXFIFO(SENSOR_I2C, buf, 2);
    DL_I2C_startControllerTransfer(SENSOR_I2C,
        g_bme280_addr,
        DL_I2C_CONTROLLER_DIRECTION_TX, 2);
    bme280_wait_bus_free();
}

/**
 * @brief  Probe a candidate I2C address for the BME280.
 *
 * Attempts to read the CHIP_ID register via raw DL I2C.
 * Returns 1 if chip ID matches, 0 on error or mismatch.
 */
static int bme280_probe(uint8_t addr, uint8_t *chipId)
{
    uint8_t reg = BME280_REG_CHIP_ID;

    /* Write register address */
    bme280_wait_idle();
    DL_I2C_fillControllerTXFIFO(SENSOR_I2C, &reg, 1);
    DL_I2C_startControllerTransfer(SENSOR_I2C,
        addr, DL_I2C_CONTROLLER_DIRECTION_TX, 1);
    bme280_wait_bus_free();

    if (DL_I2C_getControllerStatus(SENSOR_I2C)
        & DL_I2C_CONTROLLER_STATUS_ERROR) {
        return 0; /* NACK — no sensor at this address */
    }

    /* Read chip ID */
    bme280_wait_idle();
    DL_I2C_startControllerTransfer(SENSOR_I2C,
        addr, DL_I2C_CONTROLLER_DIRECTION_RX, 1);
    while (DL_I2C_isControllerRXFIFOEmpty(SENSOR_I2C)) {
    }
    *chipId = DL_I2C_receiveControllerData(SENSOR_I2C);
    bme280_wait_bus_free();

    return (*chipId == BME280_CHIP_ID_VAL);
}

/* ---- Calibration ---- */

static int bme280_read_calib(void)
{
    uint8_t lo, hi;

    bme280_read_reg(0x88, &lo, 1);
    bme280_read_reg(0x89, &hi, 1);
    g_bme280_calib.dig_T1 = ((uint16_t)hi << 8) | lo;

    bme280_read_reg(0x8A, &lo, 1);
    bme280_read_reg(0x8B, &hi, 1);
    g_bme280_calib.dig_T2 = (int16_t)(((uint16_t)hi << 8) | lo);

    bme280_read_reg(0x8C, &lo, 1);
    bme280_read_reg(0x8D, &hi, 1);
    g_bme280_calib.dig_T3 = (int16_t)(((uint16_t)hi << 8) | lo);

    /* Validate: T1 must be in valid range for a genuine BME280 */
    if (g_bme280_calib.dig_T1 < 25000 || g_bme280_calib.dig_T1 > 31000) {
        return -1;
    }
    return 0;
}

/* ---- Public API ---- */

int BME280_Init(void)
{
    uint8_t chipId;
    int found = 0;

    /* Probe I2C address: try 0x77 first (TI BOOSTXL-SENSORS default) */
    if (bme280_probe(BME280_I2C_ADDR_ALT, &chipId)) {
        g_bme280_addr = BME280_I2C_ADDR_ALT;
        found = 1;
    } else if (bme280_probe(BME280_I2C_ADDR, &chipId)) {
        g_bme280_addr = BME280_I2C_ADDR;
        found = 1;
    }

    if (!found) {
        return -1;
    }

    /* Load temperature calibration coefficients */
    if (bme280_read_calib() != 0) {
        return -1;
    }

    /*
     * Configure per datasheet §5.4.3 order:
     *   ctrl_hum → config → ctrl_meas
     * Normal mode, 1x temp, skip pressure, 0.5ms standby, no filter.
     */
    bme280_write_reg(BME280_REG_CTRL_HUM, BME280_OSRS_H_1);
    bme280_write_reg(BME280_REG_CONFIG,
        BME280_STANDBY_0_5MS | BME280_FILTER_OFF);
    bme280_write_reg(BME280_REG_CTRL_MEAS,
        BME280_OSRS_T_1 | BME280_OSRS_P_SKIP | BME280_MODE_NORMAL);

    /* Wait for first conversion (~10ms) */
    delay_cycles(320000);

    return 0;
}

float BME280_ReadTemperature(void)
{
    uint8_t buf[8];
    int32_t raw;
    float var1, var2, t_fine;
    int retries;

    /*
     * Retry loop with shadow-latch burst read.
     *
     * Reading all 8 data registers (0xF7-0xFE) in one burst triggers
     * the BME280's internal shadow-latch: all bytes are atomically
     * captured from the same measurement cycle.
     *
     * Multi-byte I2C reads on this bus are corrupted ~50% of the time
     * (bytes shift or mirror). Validate by checking the temperature
     * MSB falls within the sensor's physical range (0x30-0xA0 covers
     * -40 to +85 °C). Retry up to 5 times with 1ms settling delay.
     */
    for (retries = 0; retries < 5; retries++) {
        bme280_read_reg(0xF7, buf, 8);

        if (buf[3] >= 0x30 && buf[3] <= 0xA0) {
            break;
        }
        delay_cycles(32000); /* ~1ms */
    }

    /*
     * Assemble 20-bit raw temperature:
     *   buf[3] (0xFA) = MSB[19:12]
     *   buf[4] (0xFB) = LSB[11:4]
     *   buf[5] (0xFC) = XLSB[7:4] → bits [3:0]
     */
    raw = ((int32_t)((uint32_t)buf[3] << 12))
        | ((int32_t)((uint32_t)buf[4] << 4))
        | ((int32_t)(buf[5] >> 4));

    /* Bosch compensation formula (datasheet §4.2.3) */
    var1 = ((float)raw / 16384.0f
            - (float)g_bme280_calib.dig_T1 / 1024.0f)
           * (float)g_bme280_calib.dig_T2;

    var2 = ((float)raw / 131072.0f
            - (float)g_bme280_calib.dig_T1 / 8192.0f);
    var2 = var2 * var2 * (float)g_bme280_calib.dig_T3;

    t_fine = var1 + var2;

    return t_fine / 5120.0f;
}
