#include "ti_msp_dl_config.h"
#include "board_init.h"
#include "sensors/sensor_opt3001.h"
#include "voice_protocol.h"

/**
 * @brief  UART0 interrupt handler — forward RX bytes to ESP8266.
 *
 * Fires on every received byte (RX FIFO threshold = 1 entry).
 * Each byte is forwarded to UART1 (ESP8266) for AT command processing.
 * No local echo — UART0 TX is reserved for FireWater lux frames.
 *
 * Interrupt source: DL_UART_MAIN_IIDX_RX (UART RX FIFO threshold event).
 */
void UART0_IRQHandler(void)
{
    switch (DL_UART_Main_getPendingInterrupt(UART0)) {
    case DL_UART_MAIN_IIDX_RX:
        while (!DL_UART_Main_isRXFIFOEmpty(UART0)) {
            uint8_t ch = DL_UART_Main_receiveData(UART0);
            DL_UART_Main_transmitDataBlocking(UART1, ch);
        }
        break;
    default:
        break;
    }
}

/**
 * @brief  UART1 interrupt handler — receive ESP8266 responses.
 *
 * Fires on every byte received from the ESP8266 on UART1.
 * Receives and discards — UART0 TX is reserved for FireWater frames.
 * To monitor ESP8266 responses, connect a separate serial adapter to UART1.
 *
 * Interrupt source: DL_UART_MAIN_IIDX_RX (UART RX FIFO threshold event).
 */
void UART1_IRQHandler(void)
{
    switch (DL_UART_Main_getPendingInterrupt(UART1)) {
    case DL_UART_MAIN_IIDX_RX:
        while (!DL_UART_Main_isRXFIFOEmpty(UART1)) {
            (void)DL_UART_Main_receiveData(UART1);
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
 * Each byte is fed into Voice_Process_Byte() for 5-byte protocol parsing.
 * Raw bytes are NOT forwarded to UART0 — UART0 TX is reserved for
 * FireWater lux frames.
 *
 * Interrupt source: DL_UART_MAIN_IIDX_RX (UART RX FIFO threshold event).
 */
void UART3_IRQHandler(void)
{
    switch (DL_UART_Main_getPendingInterrupt(UART_VOICE_INST)) {
    case DL_UART_MAIN_IIDX_RX:
        while (!DL_UART_Main_isRXFIFOEmpty(UART_VOICE_INST)) {
            uint8_t ch = DL_UART_Main_receiveData(UART_VOICE_INST);
            Voice_Process_Byte(ch);
        }
        break;
    default:
        break;
    }
}

/**
 * @brief  Convert a signed float to a string with one decimal place.
 *
 * Handles the range -999.9 to +999.9. Zero is formatted as "0.0".
 *
 * @param[in]  value  Float value to format.
 * @param[out] buf    Output buffer, minimum 8 bytes.
 * @return            Pointer to buf.
 */
static char* float_to_str(float value, char *buf)
{
    char *ptr = buf;

    if (value < 0.0f) {
        *ptr++ = '-';
        value = -value;
    }
    if (value > 999.9f) { value = 999.9f; }

    int int_part = (int)value;
    int frac_part = (int)((value - (float)int_part) * 10.0f + 0.5f);
    if (frac_part >= 10) { frac_part = 0; int_part++; }

    if (int_part >= 100) *ptr++ = (char)('0' + int_part / 100);
    if (int_part >= 10)  *ptr++ = (char)('0' + (int_part / 10) % 10);
    *ptr++ = (char)('0' + int_part % 10);
    *ptr++ = '.';
    *ptr++ = (char)('0' + frac_part);
    *ptr = '\0';
    return buf;
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

    /**
     * Power on BOOSTXL-SENSORS sensors FIRST.
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
     */
    Board_UART0_Init(UART_DEBUG_BAUD);
    NVIC_EnableIRQ(UART0_INT_IRQn);

    /**
     * Initialize UART1 (ESP8266 WiFi module, PB6/PB7).
     * UART0 RX → UART1 TX forwarding by UART0_IRQHandler.
     * UART1 RX discarded — UART0 TX reserved for FireWater.
     */
    Board_UART1_Init(UART_ESP_BAUD);
    NVIC_EnableIRQ(UART1_INT_IRQn);

    /**
     * Initialize UART3 (Voice module, PB12/PB13).
     */
    Board_UART3_Init(UART_VOICE_BAUD);
    NVIC_EnableIRQ(UART3_INT_IRQn);

    Voice_Protocol_Init();

    /* Power-on LED indication: brief flash */
    LED_ON();
    delay_ms(200);
    LED_OFF();

    /* Initialize OPT3001 ambient light sensor */
    int opt3001_ok = (OPT3001_Init() == 0);

    /* Error indication: rapid LED blink if sensor init failed */
    if (!opt3001_ok) {
        while (1) {
            LED_ON();  delay_ms(100);
            LED_OFF(); delay_ms(100);
        }
    }

    while (1) {
        /* ---- OPT3001: ambient light @ ~1.25Hz → FireWater ---- */
        float lux = OPT3001_ReadLux();

        /*
         * VOFA+ FireWater frame (single channel):
         *   lux\r\n
         */
        char buf_lux[8];
        float_to_str(lux, buf_lux);

        char line[16];
        char *p = line;
        const char *s = buf_lux;
        while (*s) *p++ = *s++;
        *p++ = '\r';
        *p++ = '\n';
        *p = '\0';

        Board_UART_WriteString(UART_DEBUG_INST, line);

        delay_ms(800); /* OPT3001 continuous mode: 800ms/conversion */
    }
}
