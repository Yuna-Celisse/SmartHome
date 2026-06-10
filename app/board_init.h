#ifndef BOARD_INIT_H
#define BOARD_INIT_H

#include "ti_msp_dl_config.h"

/*
 * BOOSTXL-BASSENSORS board support.
 *
 * Hardware initialization (GPIO, I2C, ADC12) is handled by SysConfig.
 * This module provides runtime read/write helpers for the sensor drivers.
 */

/* I2C1 bus — shared by HDC2010, TMP116, OPT3001 */
#define SENSOR_I2C                              I2C1

/* ADC0 CH2 — DRV5055 Hall sensor on PA25 */
#define DRV5055_ADC                             ADC0
#define DRV5055_ADC_CHANNEL                     DL_ADC12_INPUT_CHAN_2
#define DRV5055_ADC_MEM_IDX                     DL_ADC12_MEM_IDX_0

/* Sensor power-up delay after enable pins are driven low by SysConfig */
void Board_Sensor_Init(void);

/* I2C helpers */
void Board_I2C_Write(uint8_t slaveAddr, const uint8_t *data, uint8_t len);
void Board_I2C_Read(uint8_t slaveAddr, uint8_t *data, uint8_t len);
void Board_I2C_WriteReg(uint8_t slaveAddr, uint8_t reg, uint8_t value);
void Board_I2C_ReadReg(uint8_t slaveAddr, uint8_t reg, uint8_t *data, uint8_t len);

/* ADC helper */
uint16_t Board_ADC_Read(void);

#endif /* BOARD_INIT_H */
