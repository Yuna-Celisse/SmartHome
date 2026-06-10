#include "sensor_drv5055.h"

/*
 * DRV5055 Hall Effect sensor.
 *
 * - Analog output centered at Vcc/2 (0 mT = ~1.65V at 3.3V)
 * - Sensitivity depends on variant:
 *   A1: 100 mV/mT,  A2: 50 mV/mT,  A3: 25 mV/mT,  A4: 12.5 mV/mT
 * - This driver defaults to A2 (50 mV/mT) — adjust SENSITIVITY below.
 *
 * BOOSTXL-BASSENSORS uses DRV5055A2 (50 mV/mT), range ~ ±42 mT
 */

#define DRV5055_SENSITIVITY_MV_PER_MT   50.0f    /* A2 variant */
#define DRV5055_VREF                    3.3f
#define ADC_RESOLUTION                  4096.0f  /* 12-bit ADC */

int DRV5055_Init(void)
{
    /* ADC is already configured in Board_Sensor_Init() */
    /* DRV5055 is always-on analog — no register config needed */
    return 0;
}

float DRV5055_ReadMagneticFlux(void)
{
    uint16_t adcRaw = Board_ADC_Read();

    float voltage = ((float)adcRaw / ADC_RESOLUTION) * DRV5055_VREF;

    /* 0 mT → Vcc/2. Positive = south pole, negative = north pole */
    float zeroFieldVoltage = DRV5055_VREF / 2.0f;

    return (voltage - zeroFieldVoltage) / (DRV5055_SENSITIVITY_MV_PER_MT / 1000.0f);
}
