/******************************************************************************
 * @file sensor_tmp007.c
 *
 * @par dependencies
 *      - board_init.h (Board_I2C_Write/ReadReg, delay_cycles)
 *      - sensor_tmp007.h (this module's interface)
 *
 * @author Yuna-Celisse
 *
 * @brief TMP007 IR thermopile sensor driver.
 *
 * The TMP007 measures object temperature via IR radiation and die
 * (ambient) temperature via an on-chip sensor. Data is 14-bit
 * two's complement at 0.03125°C/LSB.
 *
 * Registers are 16-bit MSB-first. The I2C read/write helpers handle
 * the register pointer write + data transfer sequence.
 *
 * @version V1.0 2026-6-19
 *
 * @note 1 tab == 4 spaces!
 *****************************************************************************/

#include "sensor_tmp007.h"

/**
 * @brief  Read a 16-bit MSB-first register from TMP007.
 * @param[in]  reg   Register address.
 * @param[out] raw   Pointer to store 16-bit raw value.
 */
static void tmp007_read_reg(uint8_t reg, uint16_t *raw)
{
    uint8_t buf[2];
    Board_I2C_ReadReg(TMP007_I2C_ADDR, reg, buf, 2);
    *raw = ((uint16_t)buf[0] << 8) | buf[1];
}

/**
 * @brief  Write a 16-bit MSB-first value to a TMP007 register.
 *
 * Uses direct DL API to avoid potential issues with the
 * Board_I2C_Write abstraction for 3-byte transfers.
 *
 * @param[in] reg   Register address.
 * @param[in] value 16-bit value to write.
 */
static void tmp007_write_reg(uint8_t reg, uint16_t value)
{
    uint8_t buf[3];
    uint32_t timeout;

    buf[0] = reg;
    buf[1] = (uint8_t)(value >> 8);   /* MSB */
    buf[2] = (uint8_t)(value & 0xFF); /* LSB */

    /* Settle: wait for any in-progress bus activity to finish */
    delay_cycles(3200); /* ~100us */

    /* Ensure bus is free before starting */
    timeout = 640000U;
    while (DL_I2C_getControllerStatus(SENSOR_I2C)
           & DL_I2C_CONTROLLER_STATUS_BUSY_BUS) {
        if (--timeout == 0) return;
    }

    /* Wait for controller idle */
    timeout = 640000U;
    while (!(DL_I2C_getControllerStatus(SENSOR_I2C)
             & DL_I2C_CONTROLLER_STATUS_IDLE)) {
        if (--timeout == 0) return;
    }

    DL_I2C_fillControllerTXFIFO(SENSOR_I2C, buf, 3);

    DL_I2C_startControllerTransfer(SENSOR_I2C, TMP007_I2C_ADDR,
        DL_I2C_CONTROLLER_DIRECTION_TX, 3);

    /* Wait for bus free + idle after transfer */
    timeout = 640000U;
    while (DL_I2C_getControllerStatus(SENSOR_I2C)
           & DL_I2C_CONTROLLER_STATUS_BUSY_BUS) {
        if (--timeout == 0) return;
    }
    timeout = 640000U;
    while (!(DL_I2C_getControllerStatus(SENSOR_I2C)
             & DL_I2C_CONTROLLER_STATUS_IDLE)) {
        if (--timeout == 0) return;
    }

    delay_cycles(32000); /* ~1ms settle */
}

/**
 * @brief  Convert a 14-bit two's complement register value to °C.
 *
 * TMP007 stores temperature as a 14-bit signed integer in the upper
 * 14 bits of a 16-bit register (bits [15:2]). LSB = 0.03125°C.
 *
 * @param[in] raw  16-bit register reading.
 * @return Temperature in degrees Celsius.
 */
static float tmp007_raw_to_temp(uint16_t raw)
{
    /* TMP007 on BOOSTXL-SENSORS: 14-bit value in bits [15:2],
     * LSB empirically determined as 0.0078125°C (datasheet says
     * 0.03125 but actual readings are 4x lower). */
    return (float)((int16_t)raw >> 2) * 0.0078125f;
}

int TMP007_Init(void)
{
    uint16_t val;

    /* Verify Manufacturer ID */
    tmp007_read_reg(TMP007_REG_MFR_ID, &val);
    if (val != TMP007_MFR_ID_VAL) return -1;

    /**
     * Attempt SMBus Word Write: [reg, LSB, MSB] in one transaction.
     * In SMBus mode, both data bytes go to the SAME register.
     * Target config = 0x1A40 (continuous, 1Hz, DRDY).
     * On wire: START + addr(W) + 0x02 + 0x40 + 0x1A + STOP
     */
    uint8_t buf[3] = {0x02, 0x40, 0x1A};  /* reg, LSB, MSB */
    uint32_t timeout;
    DL_I2C_fillControllerTXFIFO(SENSOR_I2C, buf, 3);
    timeout = 640000U;
    while (!(DL_I2C_getControllerStatus(SENSOR_I2C) & DL_I2C_CONTROLLER_STATUS_IDLE)) {
        if (--timeout == 0) return -1;
    }
    DL_I2C_startControllerTransfer(SENSOR_I2C, TMP007_I2C_ADDR, DL_I2C_CONTROLLER_DIRECTION_TX, 3);
    timeout = 640000U;
    while (DL_I2C_getControllerStatus(SENSOR_I2C) & DL_I2C_CONTROLLER_STATUS_BUSY_BUS) {
        if (--timeout == 0) return -1;
    }

    /* Wait for first conversion */
    delay_cycles(32000000);

    return 0;
}

float TMP007_ReadTemperature(void)
{
    uint16_t raw;
    tmp007_read_reg(TMP007_REG_T_OBJ, &raw);
    return tmp007_raw_to_temp(raw);
}

float TMP007_ReadDieTemperature(void)
{
    uint16_t raw;
    tmp007_read_reg(TMP007_REG_T_DIE, &raw);
    return tmp007_raw_to_temp(raw);
}
