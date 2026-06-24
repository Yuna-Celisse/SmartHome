/******************************************************************************
 * @file iotda_mqtt.c
 *
 * @par dependencies
 *      - iotda_mqtt.h (this module's interface)
 *      - esp8266_at.h (ESP_SendAndWait, ESP_SendData, ESP_PollRx, etc.)
 *      - cloud_config.h (credentials, topics, timing)
 *      - json_helper.h (Json_BuildPropertyReport, Json_ParseCommand)
 *      - system_state.h (global state variables)
 *      - board_init.h (delay_ms, LED macros)
 *
 * @author Yuna-Celisse
 *
 * @brief  Huawei Cloud IoTDA MQTT protocol layer.
 *
 * ==================== State Machine ====================
 *
 * IOTDA_INIT → ATE0 → CWMODE → CWJAP → MQTT_USERCFG → MQTT_CONN
 *   → MQTT_SUB → OPERATIONAL (← RECONNECT on failure)
 *
 * Each state attempts its AT command up to MAX_RETRIES_PER_STEP
 * times before falling back to RECONNECT. In OPERATIONAL state
 * the module periodically calls IoTDA_ReportProperties() and
 * dispatches incoming commands via IoTDA_ProcessCommand().
 *
 * @version V1.0 2026-6-22
 *
 * @note 1 tab == 4 spaces!
 *****************************************************************************/

#include "iotda_mqtt.h"
#include "esp8266_at.h"
#include "cloud_config.h"
#include "json_helper.h"
#include "system_state.h"
#include "board_init.h"
#include "str_utils.h"

/* JSON build buffer — must hold the full property report payload. */
#define JSON_TX_BUF_SIZE    512U
static char g_json_tx_buf[JSON_TX_BUF_SIZE];

/* Current state and retry counter. */
static IoTDAState g_iotda_state      = IOTDA_INIT;
static uint8_t    g_retry_count      = 0;
static uint32_t   g_last_report_ms   = 0;
static uint32_t   g_system_tick_ref  = 0;

/* ---- Forward declarations ---- */

static bool iotda_publish(const char *topic,
                          const uint8_t *payload, uint16_t len);

/* ---- Time reference (set by main loop) ---- */

/**
 * @brief  Set the system tick reference for internal timers.
 *
 * Must be called once per main-loop iteration with the current
 * system_tick value (in milliseconds).
 *
 * @param[in] tickMs  Current system_tick from main.c.
 */
void IoTDA_SetTick(uint32_t tickMs)
{
    g_system_tick_ref = tickMs;
}

/* ---- State machine ---- */

/**
 * @brief  Run one step of the connection state machine.
 *
 * @return true when OPERATIONAL, false otherwise.
 */
bool IoTDA_Step(void)
{
    switch (g_iotda_state) {

    case IOTDA_INIT:
        /**
         * ESP8266 needs ~2-3 seconds to boot after power-on.
         * A pre-delay avoids the initial AT timeouts seen in
         * the debug log. Subsequent retries skip the delay
         * (the module is already awake).
         */
        if (g_retry_count == 0) {
            ESP_DelayMs(3000);
        }

        /* Check firmware version for debugging */
        ESP_SendAndWait("AT+GMR", CMD_RESPONSE_TIMEOUT_MS);

        if (ESP_SendAndWait("AT", ESP_INIT_TIMEOUT_MS)) {
            g_retry_count = 0;
            g_iotda_state = IOTDA_ATE0;
        } else {
            g_retry_count++;
            if (g_retry_count >= MAX_RETRIES_PER_STEP) {
                g_iotda_state = IOTDA_RECONNECT;
            }
        }
        break;

    case IOTDA_ATE0:
        /* Disable echo to simplify response parsing. */
        if (ESP_SendAndWait("ATE0", CMD_RESPONSE_TIMEOUT_MS)) {
            g_retry_count = 0;
            g_iotda_state = IOTDA_CWMODE;
        } else {
            g_retry_count++;
            if (g_retry_count >= MAX_RETRIES_PER_STEP) {
                g_iotda_state = IOTDA_RECONNECT;
            }
        }
        break;

    case IOTDA_CWMODE:
        /* Set WiFi station mode. */
        if (ESP_SendAndWait("AT+CWMODE=1", CMD_RESPONSE_TIMEOUT_MS)) {
            g_retry_count = 0;
            g_iotda_state = IOTDA_CWJAP;
        } else {
            g_retry_count++;
            if (g_retry_count >= MAX_RETRIES_PER_STEP) {
                g_iotda_state = IOTDA_RECONNECT;
            }
        }
        break;

    case IOTDA_CWJAP:
        /**
         * Disconnect any lingering WiFi association first (matching
         * the INI-file sequence).  This prevents the ESP8266 from
         * rejecting CWJAP when it still thinks it's connected to a
         * previous AP after an MCU-only reset.
         */
        ESP_SendAndWait("AT+CWQAP", CMD_RESPONSE_TIMEOUT_MS);

        /* Connect to the WiFi access point. */
        {
            char cmd[128];
            char *p = cmd;
            const char *s;

            s = "AT+CWJAP=\"";
            while (*s) *p++ = *s++;
            s = WIFI_SSID;
            while (*s) *p++ = *s++;
            *p++ = '\"';
            *p++ = ',';
            *p++ = '\"';
            s = WIFI_PASSWORD;
            while (*s) *p++ = *s++;
            *p++ = '\"';
            *p = '\0';

            if (ESP_SendAndWait(cmd, WIFI_CONNECT_TIMEOUT_MS)) {
                g_retry_count = 0;
                g_iotda_state = IOTDA_WAIT_NET;
            } else {
                g_retry_count++;
                if (g_retry_count >= MAX_RETRIES_PER_STEP) {
                    g_iotda_state = IOTDA_RECONNECT;
                }
            }
        }
        break;

    case IOTDA_WAIT_NET:
        /**
         * After WiFi connects, the ESP8266 needs a few seconds for:
         *  - DHCP lease finalisation
         *  - DNS resolver initialisation
         *  - Internal network stack stabilisation
         *
         * The "busy p..." messages during MQTT USERCFG indicate
         * the module wasn't ready yet.  A 3-second delay here
         * prevents those races.
         */
        ESP_DelayMs(3000);
        g_retry_count = 0;
        g_iotda_state = IOTDA_MQTT_USERCFG;
        break;

    case IOTDA_MQTT_USERCFG:
        /**
         * Release any previous MQTT resources on the ESP8266.
         * Stale MQTT state (from an MCU-only reset while the ESP8266
         * stayed powered) causes AT+MQTTUSERCFG to return ERROR.
         * AT+MQTTCLEAN=0 tears down the old link so we can configure
         * a fresh one.
         */
        ESP_SendAndWait("AT+MQTTCLEAN=0", CMD_RESPONSE_TIMEOUT_MS);

        /**
         * Configure MQTT client parameters.
         *
         * AT+MQTTUSERCFG=0,1,"clientId","username","password",0,0,""
         *
         * Param 0: link ID = 0
         * Param 1: scheme = 1 (MQTT over TCP, no TLS on port 1883)
         * Param 2: client ID
         * Param 3: username
         * Param 4: password
         * Param 5: cert key ID = 0
         * Param 6: CA = 0
         * Param 7: path = ""
         */
        {
            char cmd[256];
            char *p = cmd;
            const char *s;

            s = "AT+MQTTUSERCFG=0,1,\"";
            while (*s) *p++ = *s++;
            s = IOTDA_CLIENT_ID;
            while (*s) *p++ = *s++;
            s = "\",\"";
            while (*s) *p++ = *s++;
            s = IOTDA_USERNAME;
            while (*s) *p++ = *s++;
            s = "\",\"";
            while (*s) *p++ = *s++;
            s = IOTDA_PASSWORD;
            while (*s) *p++ = *s++;
            s = "\",0,0,\"\"";
            while (*s) *p++ = *s++;
            *p = '\0';

            if (ESP_SendAndWait(cmd, CMD_RESPONSE_TIMEOUT_MS)) {
                g_retry_count = 0;
                g_iotda_state = IOTDA_MQTT_CONN;
            } else {
                g_retry_count++;
                if (g_retry_count >= MAX_RETRIES_PER_STEP) {
                    g_iotda_state = IOTDA_RECONNECT;
                }
            }
        }
        break;

    case IOTDA_MQTT_CONN:
        /**
         * Connect to IoTDA MQTT broker (plain TCP, port 1883).
         */
        {
            char cmd[192];
            char *p = cmd;
            const char *s;

            s = "AT+MQTTCONN=0,\"";
            while (*s) *p++ = *s++;
            s = IOTDA_HOST;
            while (*s) *p++ = *s++;
            s = "\",1883,1";
            while (*s) *p++ = *s++;
            *p = '\0';

            if (ESP_SendAndWait(cmd, MQTT_CONNECT_TIMEOUT_MS)) {
                g_retry_count = 0;

                /**
                 * Explicitly enable MQTT active receive mode.
                 * Needed on v2.3.0.0-dev firmware where the
                 * default may be passive (polling).  On v2.2.0
                 * this returns ERROR which is harmless — we
                 * proceed regardless.
                 */
                ESP_SendAndWait("AT+MQTTRECV=0,1",
                                CMD_RESPONSE_TIMEOUT_MS);

                g_iotda_state = IOTDA_MQTT_SUB;
            } else {
                g_retry_count++;
                if (g_retry_count >= MAX_RETRIES_PER_STEP) {
                    g_iotda_state = IOTDA_RECONNECT;
                }
            }
        }
        break;

    case IOTDA_MQTT_SUB:
        /**
         * Subscribe to cloud downlink topics.
         *
         * Topic 1: commands/request/+ — receives setFan / setLight /
         *           resetAlarm / setFanMode commands (wildcard matches
         *           any requestId).
         * Topic 2: properties/report/response — property-report ack.
         * Topic 3: messages/down — device messages (async, no response
         *           required; also subscribed for completeness).
         */
        {
            char cmd[192];
            char *p = cmd;
            const char *s;

            /* Subscribe: device commands (primary control channel).
             * Huawei IoTDA delivers commands to:
             *   $oc/devices/{id}/sys/commands/request_id={requestId}
             * The wildcard '#' matches everything under commands/. */
            s = "AT+MQTTSUB=0,\"$oc/devices/";
            while (*s) *p++ = *s++;
            s = IOTDA_DEVICE_ID;
            while (*s) *p++ = *s++;
            s = "/sys/commands/#\",1";
            while (*s) *p++ = *s++;
            *p = '\0';

            ESP_SendAndWait(cmd, CMD_RESPONSE_TIMEOUT_MS);

            /* Subscribe: property report response */
            p = cmd;
            s = "AT+MQTTSUB=0,\"$oc/devices/";
            while (*s) *p++ = *s++;
            s = IOTDA_DEVICE_ID;
            while (*s) *p++ = *s++;
            s = "/sys/properties/report/response\",1";
            while (*s) *p++ = *s++;
            *p = '\0';

            ESP_SendAndWait(cmd, CMD_RESPONSE_TIMEOUT_MS);

            /* Subscribe: device messages (async downlink, secondary) */
            p = cmd;
            s = "AT+MQTTSUB=0,\"$oc/devices/";
            while (*s) *p++ = *s++;
            s = IOTDA_DEVICE_ID;
            while (*s) *p++ = *s++;
            s = "/sys/messages/down\",1";
            while (*s) *p++ = *s++;
            *p = '\0';

            if (ESP_SendAndWait(cmd, CMD_RESPONSE_TIMEOUT_MS)) {
                g_retry_count = 0;
                g_last_report_ms = g_system_tick_ref;
                g_iotda_state = IOTDA_OPERATIONAL;
            } else {
                g_retry_count++;
                if (g_retry_count >= MAX_RETRIES_PER_STEP) {
                    g_iotda_state = IOTDA_RECONNECT;
                }
            }
        }
        break;

    case IOTDA_OPERATIONAL:
        /* Check for disconnection */
        if (g_esp_disconnected) {
            g_esp_disconnected = false;
            g_iotda_state = IOTDA_RECONNECT;
        }
        break;

    case IOTDA_RECONNECT:
        /**
         * Back off for RECONNECT_INTERVAL_MS, then start over.
         * We use a simple delay approach — in practice, the
         * main loop just keeps calling IoTDA_Step() and we stay
         * here until the timer expires.
         */
        {
            /* Blink LED to indicate reconnection attempt */
            LED_OFF();
            ESP_DelayMs(1000);
            LED_ON();
            ESP_DelayMs(1000);
            LED_OFF();

            g_retry_count = 0;
            g_iotda_state = IOTDA_INIT;
        }
        break;
    }

    return (g_iotda_state == IOTDA_OPERATIONAL);
}

/* ---- Property reporting ---- */

/**
 * @brief  Build and publish a property-report JSON payload.
 *
 * @note   Caller (main.c) is responsible for rate-limiting via a
 *         call counter.  This function publishes immediately when
 *         called.
 */
void IoTDA_ReportProperties(void)
{
    size_t len;

    if (g_iotda_state != IOTDA_OPERATIONAL) {
        return;
    }

    /* Interval gate: skip N-1 calls, publish on the Nth.
     * Called every ~10 ms from the main loop → ~60 s between reports. */
    {
        static uint32_t skipCount = 0;
        if (skipCount > 0) {
            skipCount--;
            return;
        }
        skipCount = PROPERTY_REPORT_INTERVAL_MS / 10U;
    }

    /* Debug: dump state before building report */
    {
        char dbg[80];
        char *dp = dbg;
        const char *s;
        s = "[RPT] lux=";
        while (*s) *dp++ = *s++;
        {
            char t[12];
            uint8_t j;
            ftoa_fixed(t, g_last_lux, 1);
            for (j = 0; t[j]; j++) *dp++ = t[j];
        }
        s = " lightOn=";
        while (*s) *dp++ = *s++;
        *dp++ = g_light_on ? '1' : '0';
        s = " lightMode=";
        while (*s) *dp++ = *s++;
        *dp++ = (g_light_mode == LIGHT_MODE_AUTO) ? 'A' : 'M';
        s = " fanMode=";
        while (*s) *dp++ = *s++;
        *dp++ = (g_fan_mode == FAN_MODE_AUTO) ? 'A' : 'M';
        *dp++ = '\r';
        *dp++ = '\n';
        *dp = '\0';
        Board_UART_WriteString(UART_DEBUG_INST, dbg);
    }

    /**
     * Publish each service as a separate MQTT message.
     *
     * The ESP8266 v2.2.0 MQTTPUBRAW implementation has a ~256-byte
     * payload limit.  The combined 3-service JSON is 290-313 bytes
     * and gets truncated.  Each individual service report is under
     * 150 bytes and transmits reliably.
     *
     * A short pre-publish settling delay lets the ESP8266 MQTT stack
     * stabilise after the previous connection handshake; 500 ms
     * between publishes prevents overlapping MQTT transactions.
     */
    {
        static const JsonService svcOrder[] = {
            JSON_SVC_ALARM,   /* first — test if timing or data */
            JSON_SVC_FAN,
            JSON_SVC_LIGHT
        };
        static bool firstReport = true;
        uint8_t    si;

        if (firstReport) {
            firstReport = false;
            Board_UART_WriteString(UART_DEBUG_INST,
                                   "[RPT] pre-publish settle 500ms\r\n");
            ESP_DelayMs(500);
        }

        for (si = 0; si < 3; si++) {
            ServiceReportData d = {0};

            /* Read all extern globals into a stack-allocated struct.
             * The struct pointer is passed to Json_BuildServiceReport
             * — floats are NEVER passed in registers across TUs,
             * sidestepping the TI Arm Clang -O2 R3 zeroing bug. */
            /* Pre-format lux as a string — the callee TU cannot
             * reliably read extern float globals (linker / GOT bug
             * specific to json_helper.c).  Passing a char array in
             * the struct works because it is fixed-size inline data,
             * not a pointer or float. */
            ftoa_fixed(d.luxStr, g_last_lux, 1);
            d.temp      = g_last_temp;
            d.fanLevel  = (uint8_t)g_fan_level;
            d.fanDuty   = g_fan_duty;
            d.lightOn   = g_light_on ? 1 : 0;
            d.lightMode = (uint8_t)g_light_mode;
            d.alarmState = (uint8_t)g_alarm_state;
            d.gyroX     = g_last_gyro_x;
            d.gyroY     = g_last_gyro_y;
            d.gyroZ     = g_last_gyro_z;

            len = Json_BuildServiceReport(g_json_tx_buf,
                                          JSON_TX_BUF_SIZE,
                                          svcOrder[si],
                                          &d);
            if (len == 0) {
                Board_UART_WriteString(UART_DEBUG_INST,
                                       "[RPT] JSON OVERFLOW\r\n");
                return;
            }

            /* Debug: dump each service JSON before transmission */
            {
                uint16_t i;
                Board_UART_WriteString(UART_DEBUG_INST, "[RPT] JSON(");
                {
                    uint16_t v = (uint16_t)len;
                    char     tmp[6];
                    uint8_t  pos2 = 0;
                    if (v == 0) {
                        tmp[pos2++] = '0';
                    } else {
                        while (v > 0) {
                            tmp[pos2++] = '0' + (uint8_t)(v % 10);
                            v /= 10;
                        }
                    }
                    while (pos2 > 0) {
                        Board_UART_Write(UART_DEBUG_INST,
                                         (uint8_t)tmp[--pos2]);
                    }
                }
                Board_UART_WriteString(UART_DEBUG_INST, "): ");
                for (i = 0; i < (uint16_t)len; i++) {
                    Board_UART_Write(UART_DEBUG_INST,
                                     (uint8_t)g_json_tx_buf[i]);
                }
                Board_UART_WriteString(UART_DEBUG_INST, "\r\n");
            }

            /* Publish to IoTDA */
            iotda_publish(IOTDA_REPORT_TOPIC,
                          (const uint8_t *)g_json_tx_buf,
                          (uint16_t)len);

            /* Pace the publishes — the ESP8266 needs time to
             * complete each MQTT transaction before starting the
             * next one. */
            if (si < 2) {
                ESP_DelayMs(500);
            }
        }
    }
}

/* ---- Command processing ---- */

/**
 * @brief  Parse and dispatch a received MQTT command.
 */
void IoTDA_ProcessCommand(void)
{
    uint16_t    payloadLen;
    const char *payload;
    uint8_t     fanLevel   = 0;
    bool        lightOn    = false;
    uint8_t     lightMode  = (uint8_t)g_light_mode;
    uint8_t     fanMode    = (uint8_t)g_fan_mode;

    if (!g_esp_mqtt_pending) {
        return;
    }

    payload = ESP_GetMQTTData(&payloadLen);
    if (!payload || payloadLen == 0) {
        ESP_ConsumeMQTTData();
        return;
    }

    /* Parse the JSON command */
    {
        int cmd = Json_ParseCommand(payload, &fanLevel, &lightOn,
                                    &lightMode, &fanMode);

        switch (cmd) {
        case 0: /* setFan */
            /**
             * Cloud setFan command: apply the requested level
             * and switch to MANUAL mode so temperature-based
             * auto control does not override the user's choice.
             */
            g_fan_mode  = FAN_MODE_MANUAL;
            g_fan_level = (FanLevel)fanLevel;
            {
                static const uint8_t dutyMap[] = {
                    FAN_DUTY_OFF,
                    FAN_DUTY_LOW,
                    FAN_DUTY_MED,
                    FAN_DUTY_HIGH,
                    FAN_DUTY_MAX
                };
                g_fan_duty = dutyMap[fanLevel];
            }
            break;

        case 1: /* setLight */
            g_light_on  = lightOn;
            g_light_mode = (LightMode)lightMode;
            /* Debug: confirm parsed values */
            {
                char dbg[64];
                char *dp = dbg;
                const char *s;
                s = "[DBG] setLight lightOn=";
                while (*s) *dp++ = *s++;
                *dp++ = lightOn ? '1' : '0';
                s = " lightMode=";
                while (*s) *dp++ = *s++;
                *dp++ = (lightMode == LIGHT_MODE_AUTO) ? 'A' : 'M';
                *dp++ = '\r';
                *dp++ = '\n';
                *dp = '\0';
                Board_UART_WriteString(UART_DEBUG_INST, dbg);
            }
            break;

        case 2: /* resetAlarm */
            g_alarm_state = ALARM_NORMAL;
            break;

        case 3: /* setFanMode */
            g_fan_mode = (FanMode)fanMode;
            break;

        default:
            break;
        }
    }

    /* Send command response ack — only for synchronous commands
     * (topics containing "commands/request"), not for async
     * messages on "messages/down" which are fire-and-forget. */
    {
        const char *topic = ESP_GetMQTTTopic();
        if (topic && topic[0] != '\0'
            && str_find(topic, "commands/request") != 0xFFFF) {
            /**
             * Extract requestId — everything after the last '/'
             * in the topic string.
             */
            uint16_t lastSlash = 0xFFFF;
            uint16_t i;

            for (i = 0; topic[i] != '\0'; i++) {
                if (topic[i] == '/') {
                    lastSlash = i;
                }
            }

            if (lastSlash != 0xFFFF) {
                char respTopic[224];
                char *p = respTopic;
                const char *s;

                s = IOTDA_RESP_PREFIX;
                while (*s) *p++ = *s++;
                s = &topic[lastSlash + 1];
                while (*s) *p++ = *s++;
                *p = '\0';

                iotda_publish(respTopic,
                              (const uint8_t *)"{\"result_code\":0}", 17);
            }
        }
    }

    /* Consume the processed command.  If additional +MQTTSUBRECV
     * lines arrived during the response publish above (ESP_PollRx
     * is called inside iotda_publish), process them immediately
     * rather than waiting for the next main-loop iteration. */
    ESP_ConsumeMQTTData();
    ESP_PollRx();
    if (g_esp_mqtt_pending) {
        /**
         * Recursive call: process the next command inline.
         * One level of recursion handles the common case of a
         * buffered command arriving during the response publish.
         */
        IoTDA_ProcessCommand();
    }
}

/* ---- Helpers ---- */

/**
 * @brief  Check whether the IoTDA connection is operational.
 */
bool IoTDA_IsConnected(void)
{
    return (g_iotda_state == IOTDA_OPERATIONAL);
}

/**
 * @brief  Publish a payload to an MQTT topic via AT+MQTTPUBRAW.
 *
 * AT+MQTTPUBRAW sends raw bytes: no quoting, no escaping needed.
 *
 * Sequence:
 *   1. AT+MQTTPUBRAW=0,"topic",<len>,0,0\r\n
 *   2. Wait for ">" prompt (NOT "OK" — the OK comes before ">")
 *   3. Send <len> raw payload bytes (no trailing \r\n)
 *   4. Poll for response (+MQTTPUB:OK / ERROR) — no transaction
 *
 * Step 4 does NOT send an empty AT command; it just polls ESP_PollRx
 * until the publish result arrives or timeout expires.
 *
 * @return true on success, false on failure.
 */
static bool iotda_publish(const char *topic,
                          const uint8_t *payload, uint16_t len)
{
    char     cmd[192];
    bool     prevForward;

    /**
     * Disable UART0→UART1 forwarding for the entire publish sequence.
     * If the XDS110 back-channel or terminal sends any byte while we
     * are writing the raw MQTTPUBRAW payload to UART1, the UART0 RX
     * ISR would interleave it into the payload stream, corrupting the
     * JSON.  Restore the original state on every return path.
     */
    prevForward              = g_uart0_forward_enabled;
    g_uart0_forward_enabled  = false;

    /* Build header: AT+MQTTPUBRAW=0,"topic",len,0,0 */
    {
        char *p = cmd;
        const char *s;

        s = "AT+MQTTPUBRAW=0,\"";
        while (*s) *p++ = *s++;
        while (*topic) *p++ = *topic++;
        *p++ = '\"';
        *p++ = ',';

        /* Convert length (uint16_t) to ASCII */
        {
            char   tmp[6];
            uint8_t pos = 0;
            uint16_t val = len;

            if (val == 0) {
                tmp[pos++] = '0';
            } else {
                while (val > 0) {
                    tmp[pos++] = '0' + (uint8_t)(val % 10);
                    val /= 10;
                }
            }
            while (pos > 0) {
                *p++ = tmp[--pos];
            }
        }

        s = ",0,0";
        while (*s) *p++ = *s++;
        *p = '\0';
    }

    /* Step 1: Send the header */
    ESP_FlushRx();
    g_esp_ok_received    = false;
    g_esp_error_received = false;
    g_esp_data_prompt    = false;

    Board_UART_WriteString(UART_DEBUG_INST, "[ESP TX] ");
    Board_UART_WriteString(UART_DEBUG_INST, cmd);
    Board_UART_WriteString(UART_DEBUG_INST, "\r\n");

    Board_UART_WriteString(UART_ESP_INST, cmd);
    Board_UART_WriteString(UART_ESP_INST, "\r\n");

    /* Step 2: Wait for OK — the ESP8266 v2.2.0 firmware
     * responds with OK (not ">") when it is ready to
     * receive the MQTTPUBRAW payload bytes. */
    {
        uint32_t elapsed = 0;
        bool     ok      = false;

        while (elapsed < CMD_RESPONSE_TIMEOUT_MS) {
            delay_ms(10);
            elapsed += 10;
            ESP_PollRx();

            if (g_esp_error_received) {
                Board_UART_WriteString(UART_DEBUG_INST,
                                       "[ESP] PUB header ERROR\r\n");
                goto cleanup;
            }
            if (g_esp_ok_received) {
                ok = true;
                break;
            }
        }

        if (!ok) {
            Board_UART_WriteString(UART_DEBUG_INST,
                                   "[ESP] PUB prompt TIMEOUT\r\n");
            goto cleanup;
        }
    }

    /* Step 3: Send the raw payload bytes (no \r\n!) */
    ESP_SendData(payload, len);

    /* Step 4: Poll for the publish result.
     * Do NOT use ESP_SendAndWait("") — that would send a bare \r\n
     * which is an invalid AT command.  Instead just poll until the
     * ESP responds with OK / +MQTTPUB:OK or ERROR. */
    {
        uint32_t elapsed = 0;

        g_esp_ok_received  = false;
        g_esp_error_received = false;

        while (elapsed < CMD_RESPONSE_TIMEOUT_MS) {
            delay_ms(10);
            elapsed += 10;
            ESP_PollRx();

            if (g_esp_error_received) {
                Board_UART_WriteString(UART_DEBUG_INST,
                                       "[ESP] PUB data ERROR\r\n");
                goto cleanup;
            }
            if (g_esp_ok_received) {
                Board_UART_WriteString(UART_DEBUG_INST,
                                       "[ESP] PUB OK\r\n");
                g_uart0_forward_enabled = prevForward;
                return true;
            }
        }

        Board_UART_WriteString(UART_DEBUG_INST,
                               "[ESP] PUB result TIMEOUT\r\n");
    }

cleanup:
    g_uart0_forward_enabled = prevForward;
    return false;
}
