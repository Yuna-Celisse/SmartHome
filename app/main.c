#include "ti_msp_dl_config.h"
#include "board_init.h"
#include "sensors/sensor_hdc2010.h"
#include "sensors/sensor_tmp007.h"
#include "sensors/sensor_bmi160.h"
#include "sensors/sensor_bme280.h"
#include "sensors/sensor_opt3001.h"
#include "sensors/sensor_drv5055.h"
#include "voice_protocol.h"

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

/**
 * @brief  UART3 interrupt handler — voice-module protocol processing.
 *
 * Fires on every byte received from the voice module on UART3 (PB12/PB13).
 * Each byte is:
 *   1. Fed into Voice_Process_Byte() for 5-byte protocol parsing.
 *   2. Forwarded to UART0 (XDS110 debug console) so protocol traffic
 *      is visible in the serial terminal for debugging.
 *
 * Interrupt source: DL_UART_MAIN_IIDX_RX (UART RX FIFO threshold event).
 */
void UART3_IRQHandler(void)
{
    switch (DL_UART_Main_getPendingInterrupt(UART_VOICE_INST)) {
    case DL_UART_MAIN_IIDX_RX:
        while (!DL_UART_Main_isRXFIFOEmpty(UART_VOICE_INST)) {
            uint8_t ch = DL_UART_Main_receiveData(UART_VOICE_INST);
            /**
             * Parse the received byte through the voice-module
             * 5-byte protocol handler (AA 55 [type] [cmd] FB).
             */
            Voice_Process_Byte(ch);
            /**
             * Forward raw bytes to UART0 for debug visibility.
             * Protocol bytes will appear in the serial terminal
             * alongside other UART traffic.
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

/**
 * @brief  Convert a signed float to a string with one decimal place.
 *
 * Handles the range -999.9 to +999.9. Positive values have no explicit
 * '+' sign. Zero is formatted as "0.0". The output is null-terminated.
 *
 * @param[in]  value  Float value to format.
 * @param[out] buf    Output buffer, minimum 8 bytes.
 * @return            Pointer to buf for use in string assembly.
 */
static char* float_to_str(float value, char *buf)
{
    char *ptr = buf;

    if (value < 0.0f) {
        *ptr++ = '-';
        value = -value;
    }

    /* Clamp to prevent integer overflow on extreme values */
    if (value > 999.9f) {
        value = 999.9f;
    }

    int int_part = (int)value;
    int frac_part = (int)((value - (float)int_part) * 10.0f + 0.5f);

    /* Handle rounding carry: e.g. 25.95 -> "26.0" not "25.10" */
    if (frac_part >= 10) {
        frac_part = 0;
        int_part++;
    }

    /* Emit integer digits — suppress leading zeros */
    if (int_part >= 100) *ptr++ = (char)('0' + int_part / 100);
    if (int_part >= 10)  *ptr++ = (char)('0' + (int_part / 10) % 10);
    *ptr++ = (char)('0' + int_part % 10);

    *ptr++ = '.';
    *ptr++ = (char)('0' + frac_part);
    *ptr = '\0';

    return buf;
}

int main(void)
{
    SYSCFG_DL_init();

    /**
     * Power on BOOSTXL-BASSENSORS sensors FIRST.
     * The sensor power rail also supplies the I2C pull-up
     * resistors — pull-up voltage must be present before
     * the I2C peripheral is initialized.
     */
    Board_Sensor_Enable();

    /**
     * Initialize I2C1 bus (pinmux, reset, power, clock,
     * 100kHz timer, controller enable). Must run AFTER
     * sensor power is stable so I2C pull-ups are active.
     */
    Board_I2C_Init();

    /* Allow I2C bus and sensors to stabilize */
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

    /**
     * Initialize UART3 (Voice module, PB12/PB13).
     * Receives 5-byte protocol packets (AA 55 [type] [cmd] FB)
     * and dispatches commands via Voice_Process_Byte().
     */
    Board_UART3_Init(UART_VOICE_BAUD);
    NVIC_EnableIRQ(UART3_INT_IRQn);

    /**
     * Send the init broadcast packet on the voice UART to notify
     * any connected voice modules that the MCU is ready.
     */
    Voice_Protocol_Init();

    /* Power-on LED indication: brief flash */
    LED_ON();
    delay_ms(200);
    LED_OFF();

    /* Initialize each sensor */
    int bmi160_ok  = (BMI160_Init() == 0);
    int tmp007_ok  = (TMP007_Init() == 0);
    int bme280_ok  = (BME280_Init() == 0);
    int hdc2010_ok = 0; /* skip init: 0x40 is ghost address, writes corrupt bus */
    int opt3001_ok = (OPT3001_Init() == 0);
    int drv5055_ok = (DRV5055_Init() == 0);

    float temp_bme280 = 0.0f;
    uint32_t report_counter = 0;

    while (1) {
        /**
         * UART0 echo + bridge and UART3 voice protocol are handled
         * entirely by interrupt service routines in the background —
         * no polling needed in the main loop.
         */

        /* ---- BME280: Environmental Temperature ---- */
        if (bme280_ok) {
            temp_bme280 = BME280_ReadTemperature();
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

        /* ---- 5-second UART temperature report ---- */
        report_counter++;
        if (report_counter >= 5) {
            report_counter = 0;

            char buf_bme280[8];

            if (bme280_ok) {
                float_to_str(temp_bme280, buf_bme280);
            } else {
                buf_bme280[0] = 'N'; buf_bme280[1] = '/';
                buf_bme280[2] = 'A'; buf_bme280[3] = '\0';
            }

            /* "BME:25.3 C\r\n" */
            char report[32];
            char *p = report;
            const char *s;

            s = "BME:"; while (*s) *p++ = *s++;
            s = buf_bme280; while (*s) *p++ = *s++;
            s = " C\r\n"; while (*s) *p++ = *s++;
            *p = '\0';

            Board_UART_WriteString(UART_DEBUG_INST, report);
        }

        delay_ms(1000);
    }
}
