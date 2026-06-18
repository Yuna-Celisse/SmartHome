#include "sensor_tmp116.h"

int TMP116_Init(void)
{
    /* Verify device ID — should be 0x0116 at address 0x48 on BOOSTXL-BASSENSORS */
    uint16_t devid = TMP116_ReadDeviceID();
    if (devid != TMP116_DEVICE_ID_VAL) {
        return -1;
    }

    /* Configure: continuous conversion, 8-sample averaging */
    uint8_t config[2];
    config[0] = TMP116_CONFIG_CONV_125MS & 0xFF;
    config[1] = (TMP116_CONFIG_CONV_125MS >> 8) | (TMP116_CONFIG_MODE_CONT >> 8);

    /* Write 16-bit config register (MSB first for TMP116) */
    uint8_t buf[3];
    buf[0] = TMP116_REG_CONFIG;
    buf[1] = config[1];  /* MSB */
    buf[2] = config[0];  /* LSB */
    Board_I2C_Write(TMP116_I2C_ADDR, buf, 3);

    delay_cycles(32000); /* let first conversion complete */

    return 0;
}

float TMP116_ReadTemperature(void)
{
    uint8_t buf[2];
    Board_I2C_ReadReg(TMP116_I2C_ADDR, TMP116_REG_TEMP, buf, 2);

    /* TMP116 returns 16-bit MSB-first, two's complement */
    int16_t raw = ((int16_t)buf[0] << 8) | buf[1];

    /* TMP116: LSB = 0.0078125 °C */
    return (float)raw * 0.0078125f;
}

uint16_t TMP116_ReadDeviceID(void)
{
    uint8_t buf[2];
    Board_I2C_ReadReg(TMP116_I2C_ADDR, TMP116_REG_DEVICE_ID, buf, 2);
    return ((uint16_t)buf[0] << 8) | buf[1];
}
