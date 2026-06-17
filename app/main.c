#include "ti_msp_dl_config.h"
#include "board_init.h"
#include "sensors/sensor_hdc2010.h"
#include "sensors/sensor_tmp116.h"
#include "sensors/sensor_opt3001.h"
#include "sensors/sensor_drv5055.h"

/**
 * @brief  UART0 interrupt handler — local echo + forward to ESP8266.
 *
 * Fires on every received byte (RX FIFO threshold = 1 entry).
 * Each byte is:
 *   1. Echoed back to UART0 (local echo so the user sees what they type).
 *   2. Forwarded to UART1 (ESP8266) for AT command processing.
 *
 * Interrupt source: DL_UART_MAIN_IIDX_RX (UART RX FIFO threshold event).
 *
 * @note  Blocking TX on UART1 in ISR context is safe because TX at
 *        115200 baud keeps pace with RX — the TX FIFO is never full
 *        during normal operation.
 */
void UART0_IRQHandler(void)
{
    switch (DL_UART_Main_getPendingInterrupt(UART0)) {
    case DL_UART_MAIN_IIDX_RX:
        while (!DL_UART_Main_isRXFIFOEmpty(UART0)) {
            uint8_t ch = DL_UART_Main_receiveData(UART0);
            /**
             * Local echo on UART0 so the user sees typed characters
             * immediately in the serial terminal.
             */
            DL_UART_Main_transmitDataBlocking(UART0, ch);
            /**
             * Forward the same byte to UART1 (ESP8266) for AT command
             * processing. The ESP8266 receives the command character
             * by character, matching standard UART AT firmware behavior.
             */
            DL_UART_Main_transmitDataBlocking(UART1, ch);
        }
        break;
    default:
        break;
    }
}

/**
 * @brief  UART1 interrupt handler — forward ESP8266 responses to UART0.
 *
 * Fires on every byte received from the ESP8266 on UART1.
 * Each byte is forwarded to UART0 (XDS110 debug console) so the user
 * can see AT command responses in the serial terminal.
 *
 * Interrupt source: DL_UART_MAIN_IIDX_RX (UART RX FIFO threshold event).
 */
void UART1_IRQHandler(void)
{
    switch (DL_UART_Main_getPendingInterrupt(UART1)) {
    case DL_UART_MAIN_IIDX_RX:
        while (!DL_UART_Main_isRXFIFOEmpty(UART1)) {
            uint8_t ch = DL_UART_Main_receiveData(UART1);
            /**
             * Forward ESP8266 response byte to UART0 for display
             * in the debug serial terminal.
             */
            DL_UART_Main_transmitDataBlocking(UART0, ch);
        }
        break;
    default:
        break;
    }
}

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

    /**
     * Initialize UART0 (PA10/PA11, XDS110 back-channel).
     * Board_UART0_Init() enables the UART RX interrupt at the
     * peripheral level; NVIC_EnableIRQ() unmasks it at the core level.
     * Order matters: peripheral interrupt must be unmasked BEFORE the
     * NVIC, or an edge-triggered IRQ could be missed.
     */
    Board_UART0_Init(UART_DEBUG_BAUD);
    NVIC_EnableIRQ(UART0_INT_IRQn);

    /* Send startup banner so PC terminal can verify connection */
    Board_UART_WriteString(UART_DEBUG_INST,
        "\r\n=== SmartHome UART0<->UART1 Bridge ===\r\n");
    Board_UART_WriteString(UART_DEBUG_INST,
        "Type AT commands -> ESP8266, responses -> terminal.\r\n");

    /**
     * Initialize UART1 (ESP8266 WiFi module, PB6/PB7).
     * Board_UART1_Init() enables the UART RX interrupt at the
     * peripheral level; NVIC_EnableIRQ() unmasks it at the core.
     * Together with UART0, this forms a bidirectional bridge:
     *   UART0 (terminal) ←→ UART1 (ESP8266)
     */
    Board_UART1_Init(UART_ESP_BAUD);
    NVIC_EnableIRQ(UART1_INT_IRQn);

    /* Power-on LED indication: brief flash */
    LED_ON();
    delay_ms(200);
    LED_OFF();

    /* Initialize each sensor */
    int hdc2010_ok = (HDC2010_Init() == 0);
    int tmp116_ok  = (TMP116_Init() == 0);
    int opt3001_ok = (OPT3001_Init() == 0);
    int drv5055_ok = (DRV5055_Init() == 0);

    while (1) {
        LED_TOGGLE();

        /**
         * UART0 echo is handled entirely by UART0_IRQHandler()
         * in the background — no polling needed in the main loop.
         */

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
