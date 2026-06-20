#include "ti_msp_dl_config.h"
#include "board_init.h"
#include "sensors/sensor_bmi160.h"
#include "sensors/sensor_bme280.h"
#include "sensors/sensor_opt3001.h"
#include "voice_protocol.h"

/**
 * @brief  UART0 interrupt handler — forward RX bytes to ESP8266.
 *
 * Fires on every received byte (RX FIFO threshold = 1 entry).
 * Each byte is forwarded to UART1 (ESP8266) for AT command processing.
 * No local echo — UART0 TX is reserved for FireWater gyroscope frames.
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
 * FireWater gyroscope frames.
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

    /* Initialize sensors: BMI160 gyro, BME280 temp, OPT3001 lux */
    int bmi160_ok  = (BMI160_Init() == 0);
    int bme280_ok  = (BME280_Init() == 0);
    int opt3001_ok = (OPT3001_Init() == 0);

    float temp  = 0.0f;
    float lux   = 0.0f;
    uint32_t cycle = 0;

    while (1) {
        /* ---- BMI160: Gyroscope @ ~50Hz → FireWater CSV ---- */
        if (bmi160_ok) {
            float gx, gy, gz;
            BMI160_ReadGyro(&gx, &gy, &gz);

            /*
             * VOFA+ FireWater CSV frame:
             *   ch0,ch1,ch2\r\n
             */
            char buf_gx[8], buf_gy[8], buf_gz[8];
            float_to_str(gx, buf_gx);
            float_to_str(gy, buf_gy);
            float_to_str(gz, buf_gz);

            char line[48];
            char *p = line;
            const char *s;

            s = buf_gx; while (*s) *p++ = *s++;
            *p++ = ',';
            s = buf_gy; while (*s) *p++ = *s++;
            *p++ = ',';
            s = buf_gz; while (*s) *p++ = *s++;
            *p++ = '\r';
            *p++ = '\n';
            *p = '\0';

            Board_UART_WriteString(UART_DEBUG_INST, line);
        }

        /* ---- BME280 + OPT3001: read every ~1s (50 cycles × 20ms) ---- */
        cycle++;
        if (cycle >= 50) {
            cycle = 0;
            if (bme280_ok)  { temp = BME280_ReadTemperature(); }
            if (opt3001_ok) { lux  = OPT3001_ReadLux(); }
            (void)temp;
            (void)lux;
        }

        delay_ms(20); /* ~50Hz */
    }
}
