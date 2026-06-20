/******************************************************************************
 * @file sensor_opt3001.c
 *
 * @par dependencies
 *      - board_init.h (SENSOR_I2C, delay_cycles, DL I2C API)
 *      - sensor_opt3001.h (this module's interface)
 *
 * @author Yuna-Celisse
 *
 * @brief OPT3001 ambient light sensor driver for BOOSTXL-SENSORS.
 *
 * Uses raw DL I2C API (DL_I2C_fillControllerTXFIFO /
 * DL_I2C_startControllerTransfer / DL_I2C_receiveControllerData)
 * to bypass the Board_I2C_Write/ReadReg wrappers, which produce
 * corrupted register values on this board (see CLAUDE.md I2C 已知问题).
 *
 * Configuration: 0xCE10 = auto-range + continuous + 800ms + latch.
 * I2C address: 0x47 (ADDR pin tied to SCL on BOOSTXL-SENSORS).
 *
 * @version V1.2 2026-6-20
 *
 * @note 1 tab == 4 spaces!
 *****************************************************************************/

#include "sensor_opt3001.h"
#include "ti_msp_dl_config.h" /* DL_I2C API, delay_cycles */

/* ---- I2C packet sizes ---- */
#define OPT_I2C_TX_INIT_SIZE         (3)
#define OPT_I2C_READSEND_SIZE        (1)
#define OPT_I2C_READRECEIVE_SIZE     (2)

/* ---- Pre-built I2C packets ---- */
static const uint8_t opt_config_packet[OPT_I2C_TX_INIT_SIZE] = {
    0x01,                           /* register: CONFIG */
    OPT3001_CONFIG_VALUE_MSB,       /* 0xCE */
    OPT3001_CONFIG_VALUE_LSB        /* 0x10 */
};

static const uint8_t opt_readsend_packet[OPT_I2C_READSEND_SIZE] = {
    0x00                            /* register: RESULT */
};

/* ---- Internal helpers ---- */

/**
 * @brief  Wait until the I2C controller is idle (IDLE flag set).
 */
static void opt_wait_idle(void)
{
    while (!(DL_I2C_getControllerStatus(SENSOR_I2C)
             & DL_I2C_CONTROLLER_STATUS_IDLE)) {
    }
}

/**
 * @brief  Wait until the I2C bus is free (BUSY_BUS flag cleared = STOP detected).
 */
static void opt_wait_bus_free(void)
{
    while (DL_I2C_getControllerStatus(SENSOR_I2C)
           & DL_I2C_CONTROLLER_STATUS_BUSY_BUS) {
    }
}

/* ---- Public API ---- */

int OPT3001_Init(void)
{
    /*
     * Write the 16-bit CONFIG register via raw DL I2C API.
     * This bypasses Board_I2C_Write/WriteReg which are known to
     * produce corrupted register values on this board.
     */
    DL_I2C_fillControllerTXFIFO(SENSOR_I2C,
        &opt_config_packet[0], OPT_I2C_TX_INIT_SIZE);

    opt_wait_idle();

    DL_I2C_startControllerTransfer(SENSOR_I2C,
        OPT3001_I2C_ADDR,
        DL_I2C_CONTROLLER_DIRECTION_TX,
        OPT_I2C_TX_INIT_SIZE);

    opt_wait_bus_free();

    if (DL_I2C_getControllerStatus(SENSOR_I2C)
        & DL_I2C_CONTROLLER_STATUS_ERROR) {
        return -1;
    }

    opt_wait_idle();
    return 0;
}

float OPT3001_ReadLux(void)
{
    uint8_t rxBuf[OPT_I2C_READRECEIVE_SIZE] = {0, 0};

    /* ---- Step 1: Send register address (0x00 = RESULT) ---- */
    opt_wait_idle();

    DL_I2C_fillControllerTXFIFO(SENSOR_I2C,
        &opt_readsend_packet[0], OPT_I2C_READSEND_SIZE);

    DL_I2C_startControllerTransfer(SENSOR_I2C,
        OPT3001_I2C_ADDR,
        DL_I2C_CONTROLLER_DIRECTION_TX,
        OPT_I2C_READSEND_SIZE);

    opt_wait_bus_free();

    if (DL_I2C_getControllerStatus(SENSOR_I2C)
        & DL_I2C_CONTROLLER_STATUS_ERROR) {
        opt_wait_idle();
        return 0.0f;
    }

    opt_wait_idle();

    /* ---- Step 2: Read 2-byte result ---- */
    DL_I2C_startControllerTransfer(SENSOR_I2C,
        OPT3001_I2C_ADDR,
        DL_I2C_CONTROLLER_DIRECTION_RX,
        OPT_I2C_READRECEIVE_SIZE);

    for (uint8_t i = 0; i < OPT_I2C_READRECEIVE_SIZE; i++) {
        while (DL_I2C_isControllerRXFIFOEmpty(SENSOR_I2C)) {
        }
        rxBuf[i] = DL_I2C_receiveControllerData(SENSOR_I2C);
    }

    opt_wait_bus_free();

    if (DL_I2C_getControllerStatus(SENSOR_I2C)
        & DL_I2C_CONTROLLER_STATUS_ERROR) {
        return 0.0f;
    }

    /*
     * Convert raw value to lux.
     * OPT3001 format: bits 15:12 = exponent, bits 11:0 = mantissa.
     * lux = mantissa × 2^exponent × 0.01
     */
    uint16_t raw      = ((uint16_t)rxBuf[0] << 8) | rxBuf[1];
    uint8_t  exponent = (raw >> 12) & 0x0F;
    uint16_t mantissa = raw & 0x0FFF;

    return (float)mantissa * (float)(1 << exponent) * 0.01f;
}
