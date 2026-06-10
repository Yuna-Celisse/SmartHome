#include "sensor_opt3001.h"

int OPT3001_Init(void)
{
    /* Verify manufacturer and device ID */
    uint16_t mfgid = OPT3001_ReadManufacturerID();
    uint16_t devid = OPT3001_ReadDeviceID();

    if (mfgid != OPT3001_MANUFACTURER_ID || devid != OPT3001_DEVICE_ID_VAL) {
        return -1;
    }

    /* Configure: auto-range, continuous mode, 800ms conversion */
    uint16_t config = OPT3001_CONFIG_AUTO_RANGE
                    | OPT3001_CONFIG_CONT_MODE
                    | OPT3001_CONFIG_CONV_800MS;

    uint8_t buf[3];
    buf[0] = OPT3001_REG_CONFIG;
    buf[1] = (config >> 8) & 0xFF;  /* MSB */
    buf[2] = config & 0xFF;         /* LSB */
    Board_I2C_Write(OPT3001_I2C_ADDR, buf, 3);

    return 0;
}

float OPT3001_ReadLux(void)
{
    uint8_t buf[2];
    Board_I2C_ReadReg(OPT3001_I2C_ADDR, OPT3001_REG_RESULT, buf, 2);

    uint16_t raw = ((uint16_t)buf[0] << 8) | buf[1];

    /* OPT3001: upper 4 bits = exponent, lower 12 bits = mantissa */
    uint8_t  exponent = (raw >> 12) & 0x0F;
    uint16_t mantissa = raw & 0x0FFF;

    return (float)mantissa * (float)(1 << exponent) * 0.01f;
}

uint16_t OPT3001_ReadManufacturerID(void)
{
    uint8_t buf[2];
    Board_I2C_ReadReg(OPT3001_I2C_ADDR, OPT3001_REG_MANUFACTURER, buf, 2);
    return ((uint16_t)buf[0] << 8) | buf[1];
}

uint16_t OPT3001_ReadDeviceID(void)
{
    uint8_t buf[2];
    Board_I2C_ReadReg(OPT3001_I2C_ADDR, OPT3001_REG_DEVICE_ID, buf, 2);
    return ((uint16_t)buf[0] << 8) | buf[1];
}
