#include "board_init.h"

/*
 * I2C and ADC helper functions for BOOSTXL-BASSENSORS sensors.
 *
 * Hardware initialization (GPIO, I2C, ADC12) is handled by SysConfig
 * via SYSCFG_DL_init(). This module provides only:
 *  - Sensor power-up delay
 *  - I2C read/write helpers
 *  - ADC read helper
 */

void Board_Sensor_Init(void)
{
    /* Wait for sensors to power up after enable pins go low (~1ms) */
    delay_cycles(32000);
}

/* ---- I2C helpers ---- */

void Board_I2C_Write(uint8_t slaveAddr, const uint8_t *data, uint8_t len)
{
    while (DL_I2C_getControllerStatus(SENSOR_I2C)
           & DL_I2C_CONTROLLER_STATUS_BUSY) {}

    DL_I2C_fillControllerTXFIFO(SENSOR_I2C, data, len);
    DL_I2C_startControllerTransfer(SENSOR_I2C, slaveAddr,
        DL_I2C_CONTROLLER_DIRECTION_TX, len);

    while (DL_I2C_getControllerStatus(SENSOR_I2C)
           & DL_I2C_CONTROLLER_STATUS_BUSY) {}
}

void Board_I2C_Read(uint8_t slaveAddr, uint8_t *data, uint8_t len)
{
    while (DL_I2C_getControllerStatus(SENSOR_I2C)
           & DL_I2C_CONTROLLER_STATUS_BUSY) {}

    DL_I2C_startControllerTransfer(SENSOR_I2C, slaveAddr,
        DL_I2C_CONTROLLER_DIRECTION_RX, len);

    while (DL_I2C_getControllerStatus(SENSOR_I2C)
           & DL_I2C_CONTROLLER_STATUS_BUSY) {}

    uint8_t i;
    for (i = 0; i < len; i++) {
        data[i] = DL_I2C_receiveControllerData(SENSOR_I2C);
    }
}

void Board_I2C_WriteReg(uint8_t slaveAddr, uint8_t reg, uint8_t value)
{
    uint8_t buf[2];
    buf[0] = reg;
    buf[1] = value;
    Board_I2C_Write(slaveAddr, buf, 2);
}

void Board_I2C_ReadReg(uint8_t slaveAddr, uint8_t reg, uint8_t *data, uint8_t len)
{
    /* Write register address */
    while (DL_I2C_getControllerStatus(SENSOR_I2C)
           & DL_I2C_CONTROLLER_STATUS_BUSY) {}

    DL_I2C_fillControllerTXFIFO(SENSOR_I2C, &reg, 1);
    DL_I2C_startControllerTransfer(SENSOR_I2C, slaveAddr,
        DL_I2C_CONTROLLER_DIRECTION_TX, 1);

    while (DL_I2C_getControllerStatus(SENSOR_I2C)
           & DL_I2C_CONTROLLER_STATUS_BUSY) {}

    /* Restart as read */
    DL_I2C_startControllerTransfer(SENSOR_I2C, slaveAddr,
        DL_I2C_CONTROLLER_DIRECTION_RX, len);

    while (DL_I2C_getControllerStatus(SENSOR_I2C)
           & DL_I2C_CONTROLLER_STATUS_BUSY) {}

    uint8_t i;
    for (i = 0; i < len; i++) {
        data[i] = DL_I2C_receiveControllerData(SENSOR_I2C);
    }
}

/* ---- ADC helper ---- */

uint16_t Board_ADC_Read(void)
{
    DL_ADC12_startConversion(DRV5055_ADC);

    while (DL_ADC12_getStatus(DRV5055_ADC)
           == DL_ADC12_STATUS_CONVERSION_ACTIVE) {}

    return DL_ADC12_getMemResult(DRV5055_ADC, DRV5055_ADC_MEM_IDX);
}
