#ifndef SENSOR_HDC2010_H
#define SENSOR_HDC2010_H

#include "board_init.h"

#define HDC2010_I2C_ADDR      0x40

/* HDC2010 registers */
#define HDC2010_REG_TEMP_LOW         0x00
#define HDC2010_REG_TEMP_HIGH        0x01
#define HDC2010_REG_HUMID_LOW        0x02
#define HDC2010_REG_HUMID_HIGH       0x03
#define HDC2010_REG_INT_DRDY         0x04
#define HDC2010_REG_MEAS_CONFIG      0x0F
#define HDC2010_REG_RESET_DRDY_CONF  0x0E

/* Measurement config bits */
#define HDC2010_MEAS_TRIG            0x01
#define HDC2010_MEAS_CONF_TEMP_HUMID 0x00
#define HDC2010_MEAS_CONF_TEMP_ONLY  0x02
#define HDC2010_RES_14BIT            0x00
#define HDC2010_RES_11BIT            0x10
#define HDC2010_RES_9BIT             0x20
#define HDC2010_TEMP_RES_MASK        0x30
#define HDC2010_HUM_RES_MASK         0xC0

/* DRDY status */
#define HDC2010_DRDY_TEMP           0x01
#define HDC2010_DRDY_HUMID          0x02

/* Reset command */
#define HDC2010_SOFT_RESET          0x80

int  HDC2010_Init(void);
void HDC2010_StartMeasurement(void);
int  HDC2010_IsDataReady(void);
float HDC2010_ReadTemperature(void);
float HDC2010_ReadHumidity(void);

#endif /* SENSOR_HDC2010_H */
