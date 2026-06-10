#include "ti_msp_dl_config.h"
#include "board_init.h"
#include "sensors/sensor_hdc2010.h"
#include "sensors/sensor_tmp116.h"
#include "sensors/sensor_opt3001.h"
#include "sensors/sensor_drv5055.h"

/* Simple busy-wait delay (approx. ms at 32MHz CPUCLK) */
static void delay_ms(uint32_t ms)
{
    for (uint32_t i = 0; i < ms; i++) {
        delay_cycles(32000);
    }
}

int main(void)
{
    SYSCFG_DL_init();

    /* Initialize sensor hardware (I2C, ADC, GPIO enables) */
    Board_Sensor_Init();

    /* Initialize each sensor */
    int hdc2010_ok = (HDC2010_Init() == 0);
    int tmp116_ok  = (TMP116_Init() == 0);
    int opt3001_ok = (OPT3001_Init() == 0);
    int drv5055_ok = (DRV5055_Init() == 0);

    while (1) {
        /* ---- HDC2010: Humidity + Temperature ---- */
        if (hdc2010_ok) {
            HDC2010_StartMeasurement();
            delay_ms(10); /* wait for conversion */

            if (HDC2010_IsDataReady()) {
                float humidity = HDC2010_ReadHumidity();
                float temp_hdc = HDC2010_ReadTemperature();
                (void)humidity;
                (void)temp_hdc;
            }
        }

        /* ---- TMP116: High-precision Temperature ---- */
        if (tmp116_ok) {
            float temp_tmp116 = TMP116_ReadTemperature();
            (void)temp_tmp116;
        }

        /* ---- OPT3001: Ambient Light (lux) ---- */
        if (opt3001_ok) {
            float lux = OPT3001_ReadLux();
            (void)lux;
        }

        /* ---- DRV5055: Hall Effect Magnetic Flux (mT) ---- */
        if (drv5055_ok) {
            float magFlux = DRV5055_ReadMagneticFlux();
            (void)magFlux;
        }

        delay_ms(1000);
    }
}
