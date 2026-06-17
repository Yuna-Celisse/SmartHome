#include "ti_msp_dl_config.h"
#include "board_init.h"
#include "sensors/sensor_hdc2010.h"
#include "sensors/sensor_tmp116.h"
#include "sensors/sensor_opt3001.h"
#include "sensors/sensor_drv5055.h"

/**
 * UART RX buffer for echoing received lines back to the terminal.
 *
 * All accesses are from the main polling loop (no ISR concurrency).
 * Declared volatile to prevent compiler optimization so the debugger
 * can inspect buffer contents at any breakpoint.
 */
#define UART_RX_BUF_SIZE    64
volatile uint8_t g_uart_rx_buf[UART_RX_BUF_SIZE];
volatile uint8_t g_uart_rx_len = 0;

/**
 * Set to 1 when a complete line (terminated by '\n') has been received.
 * Intended for debugger inspection and future async processing — not
 * currently consumed by firmware logic.
 */
volatile uint8_t g_uart_rx_flag = 0;

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

    /* Initialize UART0 (PA10/PA11, XDS110 back-channel) */
    Board_UART_Init(UART_TEST_BAUD);

    /* Send startup banner so PC terminal can verify connection */
    Board_UART_WriteString("\r\n=== SmartHome UART0 Test ===\r\n");
    Board_UART_WriteString("Send AT commands to test serial.\r\n");

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

        /* ---- UART RX: buffer until newline, echo complete line ---- */
        while (Board_UART_RXAvailable()) {
            uint8_t ch = Board_UART_Read();

            /**
             * Buffer incoming byte if space remains (reserve last byte
             * for null terminator). Excess bytes are intentionally
             * discarded — this is a debug echo, not a reliable transport.
             */
            if (g_uart_rx_len < UART_RX_BUF_SIZE - 1) {
                g_uart_rx_buf[g_uart_rx_len++] = ch;
            }

            if (ch == '\n') {
                g_uart_rx_buf[g_uart_rx_len] = '\0';
                g_uart_rx_flag = 1;
                Board_UART_WriteString((const char *)g_uart_rx_buf);
                g_uart_rx_len = 0;
            }
        }

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
