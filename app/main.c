#include "ti_msp_dl_config.h"
#include "board_init.h"
#include "system_state.h"
#include "sensors/sensor_opt3001.h"
#include "sensors/sensor_bmi160.h"
#include "sensors/sensor_bme280.h"
#include "voice_protocol.h"

/**
 * @brief  UART0 interrupt handler — forward RX bytes to ESP8266.
 *
 * Fires on every received byte (RX FIFO threshold = 1 entry).
 * Each byte is forwarded to UART1 (ESP8266) for AT command processing.
 * No local echo — UART0 TX is reserved for FireWater frames.
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
 * FireWater frames.
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

/* ========== Global system state ========== */
LightMode   g_light_mode           = LIGHT_MODE_AUTO;
bool        g_light_on             = false;
FanLevel    g_fan_level            = FAN_OFF;
uint8_t     g_fan_duty             = FAN_DUTY_OFF;
AlarmState  g_alarm_state          = ALARM_NORMAL;
uint32_t    g_alarm_cooldown_ms    = 0;
float       g_last_lux             = 0.0f;
float       g_last_temp            = 25.0f;
float       g_last_gyro_x          = 0.0f;
float       g_last_gyro_y          = 0.0f;
float       g_last_gyro_z          = 0.0f;

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

    /**
     * Initialize all three sensors and fan PWM.
     * If any sensor fails, enter infinite error blink
     * (rapid 100ms on/off on the LED).
     */
    int opt3001_ok = (OPT3001_Init() == 0);
    int bme280_ok  = (BME280_Init()  == 0);
    int bmi160_ok  = (BMI160_Init()  == 0);
    Board_Fan_Init();

    if (!opt3001_ok || !bme280_ok || !bmi160_ok) {
        while (1) {
            LED_ON();  delay_ms(100);
            LED_OFF(); delay_ms(100);
        }
    }

    /**
     * Main loop: tick-based cooperative scheduler.
     *
     * Each iteration adds LOOP_DELAY_MS (10ms) to system_tick.
     * Per-sensor last_*_ms timestamps track when each task last
     * ran, enabling independent intervals: gyro @100ms,
     * light @800ms, temp @1000ms, FireWater lux @1000ms.
     * Unsigned subtraction handles uint32_t wrap correctly
     * for intervals under ~24 days.
     */
#define LOOP_DELAY_MS               10u
#define BMI160_INTERVAL_MS          100u
#define OPT3001_INTERVAL_MS         100u  /* 10Hz, matches HW 100ms conversion */
#define BME280_INTERVAL_MS          1000u
#define FIREWATER_INTERVAL_MS       1000u
#define ALARM_BLINK_INTERVAL_MS     100u

    uint32_t system_tick      = 0;
    uint32_t last_gyro_ms     = 0;
    uint32_t last_light_ms    = 0;
    uint32_t last_temp_ms     = 0;
    uint32_t last_uart_ms     = 0;

    while (1) {
        system_tick += LOOP_DELAY_MS;

        /* ===== BMI160: gyroscope vibration detection @100ms ===== */
        if ((system_tick - last_gyro_ms) >= BMI160_INTERVAL_MS) {
            last_gyro_ms = system_tick;

            float gx, gy, gz;
            BMI160_ReadGyro(&gx, &gy, &gz);
            g_last_gyro_x = gx;
            g_last_gyro_y = gy;
            g_last_gyro_z = gz;

            /* Trigger alarm if any axis exceeds threshold
             * AND cooldown period has elapsed */
            if ((gx >  GYRO_ALARM_THRESHOLD_DPS ||
                 gx < -GYRO_ALARM_THRESHOLD_DPS ||
                 gy >  GYRO_ALARM_THRESHOLD_DPS ||
                 gy < -GYRO_ALARM_THRESHOLD_DPS ||
                 gz >  GYRO_ALARM_THRESHOLD_DPS ||
                 gz < -GYRO_ALARM_THRESHOLD_DPS) &&
                (system_tick - g_alarm_cooldown_ms) >= ALARM_COOLDOWN_MS) {

                g_alarm_state       = ALARM_FIRING;
                g_alarm_cooldown_ms = system_tick;
            }
        }

        /* ===== OPT3001: ambient light auto-control @800ms ===== */
        if ((system_tick - last_light_ms) >= OPT3001_INTERVAL_MS) {
            last_light_ms = system_tick;

            float lux = OPT3001_ReadLux();
            g_last_lux = lux;

            /*
             * Auto-light control: only applies when mode is AUTO.
             * Hysteresis prevents flicker near the threshold:
             *   lux < 50  → turn ON
             *   lux > 100 → turn OFF
             *   between   → keep current state
             *
             * IMPORTANT: lux == 0.0f is the I2C error sentinel
             * from OPT3001_ReadLux() (all retries exhausted).
             * Never use it for auto-light decisions — a bogus
             * "dark" reading would turn the light on and keep
             * it on indefinitely.
             */
            if (g_light_mode == LIGHT_MODE_AUTO && lux > 0.0f) {
                if (lux < LIGHT_ON_THRESHOLD_LUX) {
                    g_light_on = true;
                } else if (lux > LIGHT_OFF_THRESHOLD_LUX) {
                    g_light_on = false;
                }
                /* Between 50 and 100 lux: maintain previous state */
            }
        }

        /* ===== BME280: temperature → fan speed @1000ms ===== */
        if ((system_tick - last_temp_ms) >= BME280_INTERVAL_MS) {
            last_temp_ms = system_tick;

            float temp = BME280_ReadTemperature();
            g_last_temp = temp;

            /* Map temperature to fan level with hysteresis */
            FanLevel newLevel;
            uint8_t  newDuty;

            if (temp < TEMP_FAN_OFF_THRESHOLD) {
                newLevel = FAN_OFF;
                newDuty  = FAN_DUTY_OFF;
            } else if (temp < TEMP_FAN_LOW_THRESHOLD) {
                newLevel = FAN_LOW;
                newDuty  = FAN_DUTY_LOW;
            } else if (temp < TEMP_FAN_MED_THRESHOLD) {
                newLevel = FAN_MED;
                newDuty  = FAN_DUTY_MED;
            } else if (temp < TEMP_FAN_HIGH_THRESHOLD) {
                newLevel = FAN_HIGH;
                newDuty  = FAN_DUTY_HIGH;
            } else {
                newLevel = FAN_MAX;
                newDuty  = FAN_DUTY_MAX;
            }

            /* Only update PWM when fan level actually changes */
            if (newLevel != g_fan_level) {
                g_fan_level = newLevel;
                g_fan_duty  = newDuty;
                Board_Fan_SetDuty(newDuty);
            }
        }

        /* ===== FireWater lux output @1000ms ===== */
        if ((system_tick - last_uart_ms) >= FIREWATER_INTERVAL_MS) {
            last_uart_ms = system_tick;

            /*
             * VOFA+ FireWater frame (single channel):
             *   lux\r\n
             */
            char buf_lux[8];
            float_to_str(g_last_lux, buf_lux);

            char line[16];
            char *p = line;
            const char *s = buf_lux;
            while (*s) *p++ = *s++;
            *p++ = '\r';
            *p++ = '\n';
            *p = '\0';

            Board_UART_WriteString(UART_DEBUG_INST, line);
        }

        /* ===== LED output state machine (every iteration) ===== */
        if (g_alarm_state == ALARM_FIRING) {
            /*
             * Alarm active: 5Hz blink overrides light state.
             * De-escalate after ALARM_COOLDOWN_MS (5s).
             */
            if ((system_tick - g_alarm_cooldown_ms)
                >= ALARM_COOLDOWN_MS) {
                /* Alarm period expired — restore light state */
                g_alarm_state = ALARM_NORMAL;
                if (g_light_on) {
                    LED_ON();
                } else {
                    LED_OFF();
                }
            } else {
                /* Still in alarm: 5Hz blink */
                if (((system_tick / ALARM_BLINK_INTERVAL_MS) & 1u) == 0) {
                    LED_ON();
                } else {
                    LED_OFF();
                }
            }
        } else {
            /* Normal mode: LED follows g_light_on */
            if (g_light_on) {
                LED_ON();
            } else {
                LED_OFF();
            }
        }

        /* Base loop delay: 10ms per iteration */
        delay_ms(LOOP_DELAY_MS);
    }
}
