#include "ti_msp_dl_config.h"
#include "board_init.h"
#include "system_state.h"
#include "sensors/sensor_opt3001.h"
#include "sensors/sensor_bmi160.h"
#include "sensors/sensor_bme280.h"
#include "voice_protocol.h"
#include "fan_control.h"
#include "str_utils.h"
#include "esp8266_at.h"
#include "iotda_mqtt.h"
#include "cloud_config.h"

/**
 * @brief  UART0 interrupt handler — forward RX bytes to ESP8266.
 *
 * Fires on every received byte (RX FIFO threshold = 1 entry).
 * When g_uart0_forward_enabled is true, each byte is forwarded to
 * UART1 (ESP8266) for manual AT command entry.  During cloud
 * auto-connection the flag is cleared to prevent interference.
 *
 * Interrupt source: DL_UART_MAIN_IIDX_RX (UART RX FIFO threshold event).
 */
void UART0_IRQHandler(void)
{
    switch (DL_UART_Main_getPendingInterrupt(UART0)) {
    case DL_UART_MAIN_IIDX_RX:
        while (!DL_UART_Main_isRXFIFOEmpty(UART0)) {
            uint8_t ch = DL_UART_Main_receiveData(UART0);
            if (g_uart0_forward_enabled) {
                DL_UART_Main_transmitDataBlocking(UART1, ch);
            }
        }
        break;
    default:
        break;
    }
}

/**
 * @brief  UART1 interrupt handler — ESP8266 response reception.
 *
 * Fires on every byte received from the ESP8266 on UART1.
 * Each byte is enqueued into the ESP driver's ring buffer for
 * non-ISR processing by ESP_PollRx() in the main loop.
 *
 * Interrupt source: DL_UART_MAIN_IIDX_RX (UART RX FIFO threshold event).
 */
void UART1_IRQHandler(void)
{
    switch (DL_UART_Main_getPendingInterrupt(UART1)) {
    case DL_UART_MAIN_IIDX_RX:
        while (!DL_UART_Main_isRXFIFOEmpty(UART1)) {
            uint8_t ch = DL_UART_Main_receiveData(UART1);
            ESP_ProcessRxByte(ch);
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

/* ========== Global system state ========== */
LightMode   g_light_mode           = LIGHT_MODE_AUTO;
bool        g_light_on             = false;
FanLevel    g_fan_level            = FAN_OFF;
uint8_t     g_fan_duty             = FAN_DUTY_OFF;
FanMode     g_fan_mode             = FAN_MODE_AUTO;
AlarmState  g_alarm_state          = ALARM_NORMAL;
uint32_t    g_alarm_cooldown_ms    = 0;
float       g_last_lux             = 0.0f;
float       g_last_temp            = 25.0f;
float       g_last_gyro_x          = 0.0f;
float       g_last_gyro_y          = 0.0f;
float       g_last_gyro_z          = 0.0f;

/* ========== Cloud communication state ========== */

/**
 * @brief  Enable / disable UART0→UART1 forwarding.
 *
 * Cleared during the cloud auto-connection sequence to prevent
 * manual AT commands from interfering with the handshake.
 */
bool g_uart0_forward_enabled = true;

/** True when the ESP8266 is connected to WiFi and IoTDA MQTT. */
bool g_esp_connected        = false;
bool g_mqtt_connected       = false;

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
     * Initialize fan control subsystem (TIMA0 PWM on PA8,
     * TB6612 direction GPIOs on PB0/PB1). Must be called before
     * FanControl_Update().
     */
    FanControl_Init();

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
     * Initialize all three sensors.
     * If any sensor fails, enter infinite error blink
     * (rapid 100ms on/off on the LED).
     */
    int opt3001_ok = (OPT3001_Init() == 0);
    int bme280_ok  = (BME280_Init()  == 0);
    int bmi160_ok  = (BMI160_Init()  == 0);
    Board_Fan_Init();
    Board_Buzzer_Init();

    if (!opt3001_ok || !bme280_ok || !bmi160_ok) {
        while (1) {
            LED_ON();  delay_ms(100);
            LED_OFF(); delay_ms(100);
        }
    }

    /**
     * ===== Cloud connection: ESP8266 → WiFi → IoTDA MQTT =====
     *
     * ESP_Init() prepares the UART1 RX ring buffer and response
     * parser. The connection state machine (IoTDA_Step) runs
     * in a blocking loop until OPERATIONAL is reached or a
     * permanent error occurs (~60 s worst case).
     *
     * During auto-connection, UART0→UART1 forwarding is
     * disabled to prevent manual AT commands from interfering.
     */
    ESP_Init();
    g_uart0_forward_enabled = false;

    while (!IoTDA_IsConnected()) {
        IoTDA_Step();
        delay_ms(100);
    }

    /* Connection established — re-enable manual AT forwarding. */
    g_uart0_forward_enabled = true;
    g_esp_connected  = true;
    g_mqtt_connected = true;

    /* Re-initialise light state: MQTT session resume may have
     * delivered buffered commands that left g_light_on / g_light_mode
     * in an unexpected state.  Force the auto-light defaults. */
    g_light_on   = false;
    g_light_mode = LIGHT_MODE_AUTO;

    /* Quick LED blink to confirm cloud connection */
    LED_ON();  delay_ms(50);
    LED_OFF(); delay_ms(50);
    LED_ON();  delay_ms(50);
    LED_OFF();

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
    uint32_t buzzer_start_ms  = 0;

    /* ---- Sensor + Fan + Cloud Loop ---- */
    while (1) {
        system_tick += LOOP_DELAY_MS;

        /* ===== Cloud: ESP8266 RX polling (every iteration) ===== */
        ESP_PollRx();
        IoTDA_SetTick(system_tick);

        if (!IoTDA_IsConnected()) {
            /* Connection lost — try to reconnect (blocking in steps,
             * but each IoTDA_Step() call yields via delay_ms) */
            g_esp_connected  = false;
            g_mqtt_connected = false;
            g_uart0_forward_enabled = false;
            IoTDA_Step();
            delay_ms(200);
        } else {
            g_uart0_forward_enabled = true;

            /* Dispatch any received MQTT command */
            if (g_esp_mqtt_data_received) {
                IoTDA_ProcessCommand();
            }

            /* Periodic sensor-data report to IoTDA */
            IoTDA_ReportProperties();
        }

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
                buzzer_start_ms     = system_tick;
            }
        }

        /* ===== OPT3001: ambient light control @100ms ===== */
        if ((system_tick - last_light_ms) >= OPT3001_INTERVAL_MS) {
            last_light_ms = system_tick;

            float lux = OPT3001_ReadLux();
            g_last_lux = lux;

            if (g_light_mode == LIGHT_MODE_AUTO && lux > 0.0f) {
                /**
                 * AUTO mode: determine light state from ambient
                 * light level with hysteresis.
                 *   lux < 50  → turn ON
                 *   lux > 100 → turn OFF
                 *   between   → keep current state
                 */
                if (lux < LIGHT_ON_THRESHOLD_LUX) {
                    g_light_on = true;
                } else if (lux > LIGHT_OFF_THRESHOLD_LUX) {
                    g_light_on = false;
                }
            }
            /* In MANUAL mode: g_light_on is held at the value
             * set by voice / cloud command — do not touch it. */

            /**
             * Apply light state to the physical LED immediately,
             * mirroring how the BME280 section applies fan duty
             * in both AUTO and MANUAL modes.
             */
            if (g_alarm_state != ALARM_FIRING) {
                if (g_light_on) {
                    LED_ON();
                } else {
                    LED_OFF();
                }
            }
        }

        /* ===== BME280: temperature reading @1000ms ===== */
        if ((system_tick - last_temp_ms) >= BME280_INTERVAL_MS) {
            last_temp_ms = system_tick;

            float temp = BME280_ReadTemperature();
            g_last_temp = temp;

            if (g_fan_mode == FAN_MODE_AUTO) {
                /**
                 * AUTO mode: thermostatic control via fan_control.c.
                 * FanControl_Update() implements linear mapping
                 * (20°C→40°C to 20%→100%) with ±1°C hysteresis.
                 * Read back the computed speed and map to level.
                 */
                FanControl_Update(temp);
                g_fan_duty = FanControl_GetSpeed();
                if (g_fan_duty == 0) {
                    g_fan_level = FAN_OFF;
                } else if (g_fan_duty <= 25) {
                    g_fan_level = FAN_LOW;
                } else if (g_fan_duty <= 50) {
                    g_fan_level = FAN_MED;
                } else if (g_fan_duty <= 75) {
                    g_fan_level = FAN_HIGH;
                } else {
                    g_fan_level = FAN_MAX;
                }
            } else {
                /**
                 * MANUAL mode: apply voice/cloud-set duty.
                 * Use FanControl_SetSpeed() (not raw
                 * Board_Fan_SetSpeed) to keep fan_control.c
                 * internal state synchronised.
                 */
                FanControl_SetSpeed(g_fan_duty);
            }
        }

#if 0
        /* ===== FireWater CSV output @1000ms ===== */
        /* Disabled: serial port is now used exclusively for ESP8266
         * AT command debug traces.  Sensor data is reported to the
         * cloud via MQTT (IoTDA_ReportProperties). */
        if ((system_tick - last_uart_ms) >= FIREWATER_INTERVAL_MS) {
            last_uart_ms = system_tick;

            /*
             * VOFA+ FireWater frame (2 channels):
             *   lux,temp\r\n
             */
            char buf_lux[8];
            char buf_temp[8];
            ftoa_fixed(buf_lux, g_last_lux, 1);
            ftoa_fixed(buf_temp, g_last_temp, 1);

            char line[32];
            char *p = line;
            const char *s;

            s = buf_lux;
            while (*s) *p++ = *s++;
            *p++ = ',';

            s = buf_temp;
            while (*s) *p++ = *s++;
            *p++ = '\r';
            *p++ = '\n';
            *p = '\0';

            Board_UART_WriteString(UART_DEBUG_INST, line);
        }
#endif

        /* ===== LED + Buzzer output state machine (every iteration) ===== */
        if (g_alarm_state == ALARM_FIRING) {
            if ((system_tick - g_alarm_cooldown_ms)
                >= ALARM_COOLDOWN_MS) {
                /* Alarm period expired — restore normal light */
                g_alarm_state = ALARM_NORMAL;
                if (g_light_on) {
                    LED_ON();
                } else {
                    LED_OFF();
                }
            } else {
                /* Still in alarm: 5Hz blink overrides light */
                if (((system_tick / ALARM_BLINK_INTERVAL_MS) & 1u) == 0) {
                    LED_ON();
                } else {
                    LED_OFF();
                }
            }
        }
        /* Normal LED state is applied in the OPT3001 section above —
         * no need to duplicate it here. */

        /*
         * Buzzer: active for BUZZER_DURATION_MS (10s) from last
         * alarm trigger, independent of the 5s alarm cooldown.
         * A new vibration trigger restarts the 10s timer.
         */
        if ((system_tick - buzzer_start_ms) < BUZZER_DURATION_MS) {
            Board_Buzzer_On();
        } else {
            Board_Buzzer_Off();
        }

        /* Base loop delay: 10ms per iteration */
        delay_ms(LOOP_DELAY_MS);
    }
}
