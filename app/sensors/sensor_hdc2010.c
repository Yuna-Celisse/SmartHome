#include "sensor_hdc2010.h"

int HDC2010_Init(void)
{
    /* Soft reset */
    Board_I2C_WriteReg(HDC2010_I2C_ADDR, HDC2010_REG_RESET_DRDY_CONF, HDC2010_SOFT_RESET);
    delay_cycles(32000); /* ~1ms */

    /* Enable DRDY status bits in register 0x04 for data-ready polling */
    Board_I2C_WriteReg(HDC2010_I2C_ADDR, HDC2010_REG_RESET_DRDY_CONF, HDC2010_DRDY_EN);

    /* Configure: 14-bit temp + humidity, measure both */
    uint8_t config = HDC2010_RES_14BIT | HDC2010_MEAS_CONF_TEMP_HUMID;
    Board_I2C_WriteReg(HDC2010_I2C_ADDR, HDC2010_REG_MEAS_CONFIG, config);

    /* Verify: read back config register */
    uint8_t readback;
    Board_I2C_ReadReg(HDC2010_I2C_ADDR, HDC2010_REG_MEAS_CONFIG, &readback, 1);

    /* Init passes only if config matches (real I2C verification) */
    return (readback == config) ? 0 : -1;
}

void HDC2010_StartMeasurement(void)
{
    uint8_t config;
    Board_I2C_ReadReg(HDC2010_I2C_ADDR, HDC2010_REG_MEAS_CONFIG, &config, 1);
    config |= HDC2010_MEAS_TRIG;
    Board_I2C_WriteReg(HDC2010_I2C_ADDR, HDC2010_REG_MEAS_CONFIG, config);
}

int HDC2010_IsDataReady(void)
{
    uint8_t drdy;
    Board_I2C_ReadReg(HDC2010_I2C_ADDR, HDC2010_REG_INT_DRDY, &drdy, 1);
    return (drdy & HDC2010_DRDY_TEMP) ? 1 : 0;
}

float HDC2010_ReadTemperature(void)
{
    uint8_t buf[2];
    Board_I2C_ReadReg(HDC2010_I2C_ADDR, HDC2010_REG_TEMP_LOW, buf, 2);

    uint16_t raw = ((uint16_t)buf[1] << 8) | buf[0];

    /* Formula: Temp(C) = raw / 65536 * 165 - 40.5 */
    return ((float)raw / 65536.0f) * 165.0f - 40.5f;
}

float HDC2010_ReadHumidity(void)
{
    uint8_t buf[2];
    Board_I2C_ReadReg(HDC2010_I2C_ADDR, HDC2010_REG_HUMID_LOW, buf, 2);

    uint16_t raw = ((uint16_t)buf[1] << 8) | buf[0];

    /* Formula: RH(%) = raw / 65536 * 100 */
    return ((float)raw / 65536.0f) * 100.0f;
}
