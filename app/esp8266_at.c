/******************************************************************************
 * @file esp8266_at.c
 *
 * @par dependencies
 *      - esp8266_at.h (this module's interface)
 *      - board_init.h (Board_UART_WriteString, UART_ESP_INST, delay_ms)
 *      - str_utils.h (str_len, str_find)
 *      - mspm0-sdk/source/ti/drivers/utils/RingBuf.h
 *
 * @author Yuna-Celisse
 *
 * @brief  ESP8266 AT command driver with ring-buffered RX and a
 *         line-oriented response parser.
 *
 * ==================== Architecture ====================
 *
 *   UART1_IRQHandler (ISR)
 *        │
 *        └─ ESP_ProcessRxByte(ch) → RingBuf_put
 *
 *   main() loop (~10 ms)
 *        │
 *        └─ ESP_PollRx() → RingBuf_get → parser FSM
 *               │
 *               ├─ "OK"        → g_esp_ok_received = true
 *               ├─ "ERROR/FAIL"→ g_esp_error_received = true
 *               ├─ "+MQTTSUBRECV:…" → collect payload
 *               └─ "…DISCONNECT/CLOSED" → g_esp_disconnected = true
 *
 * ==================== AT response format ====================
 *
 * ESP8266 AT firmware (v2.2+) sends responses as CR-LF-delimited
 * lines. A typical exchange:
 *
 *   send: AT\r\n
 *   recv: \r\nOK\r\n
 *
 *   send: AT+CWJAP="ssid","pwd"\r\n
 *   recv: WIFI CONNECTED\r\nWIFI GOT IP\r\n\r\nOK\r\n
 *
 * MQTT subscription data arrives as:
 *   +MQTTSUBRECV:0,"topic",payload_len,payload_bytes
 *
 * where payload_bytes is exactly payload_len raw bytes (NOT
 * CR-LF-delimited, NOT null-terminated).
 *
 * @version V1.0 2026-6-22
 *
 * @note 1 tab == 4 spaces!
 *****************************************************************************/

#include "esp8266_at.h"
#include "board_init.h"
#include "str_utils.h"

/* ---- Buffer sizes ---- */

/** UART1 RX ring buffer — absorbs bursts from the ESP8266. */
#define ESP_RX_RING_SIZE    256U

/** Maximum length of a single AT response line (CR-LF delimited).
 *  Must hold a full +MQTTSUBRECV line including inline JSON payload.
 *  Topic (~100 chars) + length field + JSON body (~100 chars) ≈ 256. */
#define ESP_LINE_BUF_SIZE   256U

/** Maximum MQTT subscription payload (JSON command body). */
#define ESP_PAYLOAD_BUF_SIZE 256U

/** Maximum topic string length from +MQTTSUBRECV. */
#define ESP_TOPIC_BUF_SIZE   160U

/* ---- Minimal ring buffer (avoids SDK HwiP dependency) ---- */

static unsigned char g_esp_rx_ring_buf[ESP_RX_RING_SIZE];
static uint16_t      g_esp_rx_head  = 0;
static uint16_t      g_esp_rx_tail  = 0;
static uint16_t      g_esp_rx_count = 0;

static bool ring_put(unsigned char b)
{
    if (g_esp_rx_count >= ESP_RX_RING_SIZE) {
        return false;
    }
    g_esp_rx_ring_buf[g_esp_rx_head] = b;
    g_esp_rx_head = (g_esp_rx_head + 1) % ESP_RX_RING_SIZE;
    /* ISR only writes head; main loop only reads count/tail + writes tail.
     * On Cortex-M0+ these uint16_t accesses are atomic. */
    g_esp_rx_count++;
    return true;
}

static int ring_get(unsigned char *b)
{
    if (g_esp_rx_count == 0) {
        return -1;
    }
    *b = g_esp_rx_ring_buf[g_esp_rx_tail];
    g_esp_rx_tail = (g_esp_rx_tail + 1) % ESP_RX_RING_SIZE;
    g_esp_rx_count--;
    return (int)g_esp_rx_count;
}

static int ring_count(void)
{
    return (int)g_esp_rx_count;
}

static void ring_flush(void)
{
    g_esp_rx_head  = 0;
    g_esp_rx_tail  = 0;
    g_esp_rx_count = 0;
}

/* ---- RX parser states ---- */
typedef enum {
    ESP_RX_IDLE,        /* Idle — skip leading whitespace */
    ESP_RX_LINE,        /* Accumulating a response line */
    ESP_RX_CR_SEEN,     /* Saw '\r', waiting for '\n' to complete the line */
    ESP_RX_PAYLOAD       /* Collecting raw MQTT payload bytes */
} EspRxState;

/* ---- Static buffers ---- */

static char       g_esp_line_buf[ESP_LINE_BUF_SIZE];
static uint8_t    g_esp_line_pos;

/* MQTT payload tracking */
static char       g_esp_payload_buf[ESP_PAYLOAD_BUF_SIZE];
static char       g_esp_topic_buf[ESP_TOPIC_BUF_SIZE];
static uint16_t   g_esp_payload_len;      /* Length declared in +MQTTSUBRECV */
static uint16_t   g_esp_payload_count;    /* Bytes received so far */

/* Parser state */
static EspRxState g_esp_rx_state = ESP_RX_IDLE;

/* ---- Global flags ---- */

bool g_esp_ok_received;
bool g_esp_error_received;
bool g_esp_data_prompt;
bool g_esp_mqtt_data_received;
bool g_esp_disconnected;

/* ---- Forward declarations ---- */

static void process_line(const char *line);

/* ---- Public API ---- */

/**
 * @brief  Initialise the ring buffer and parser state.
 */
void ESP_Init(void)
{
    ring_flush();
    g_esp_line_pos         = 0;
    g_esp_rx_state         = ESP_RX_IDLE;
    g_esp_payload_len      = 0;
    g_esp_payload_count    = 0;
    g_esp_ok_received      = false;
    g_esp_error_received   = false;
    g_esp_data_prompt      = false;
    g_esp_mqtt_data_received = false;
    g_esp_disconnected     = false;
}

/**
 * @brief  Feed one received byte into the UART1 RX ring buffer (ISR-safe).
 */
void ESP_ProcessRxByte(uint8_t ch)
{
    ring_put(ch);
}

/**
 * @brief  Drain the ring buffer and run the response-line parser.
 */
void ESP_PollRx(void)
{
    int count = ring_count();

    while (count > 0) {
        unsigned char ch;
        if (ring_get(&ch) < 0) {
            break;
        }
        count--;

        switch (g_esp_rx_state) {

        case ESP_RX_IDLE:
            /* Skip CR/LF between lines */
            if (ch == '\r' || ch == '\n') {
                break;
            }
            /* Start of a new line */
            g_esp_line_pos = 0;
            if (g_esp_line_pos < ESP_LINE_BUF_SIZE - 1) {
                g_esp_line_buf[g_esp_line_pos++] = (char)ch;
            }
            g_esp_rx_state = ESP_RX_LINE;
            break;

        case ESP_RX_LINE:
            if (ch == '\r') {
                g_esp_rx_state = ESP_RX_CR_SEEN;
            } else if (ch == '\n') {
                /* Bare LF — treat as line terminator */
                g_esp_line_buf[g_esp_line_pos] = '\0';
                process_line(g_esp_line_buf);
                g_esp_line_pos = 0;
                g_esp_rx_state = ESP_RX_IDLE;
            } else {
                if (g_esp_line_pos < ESP_LINE_BUF_SIZE - 1) {
                    g_esp_line_buf[g_esp_line_pos++] = (char)ch;
                }
                /* If buffer full, stay in LINE state — byte is dropped
                 * but we'll still wait for CR/LF to resynchronise */
            }
            break;

        case ESP_RX_CR_SEEN:
            if (ch == '\n') {
                /* Complete line */
                g_esp_line_buf[g_esp_line_pos] = '\0';
                process_line(g_esp_line_buf);
                g_esp_line_pos = 0;
                g_esp_rx_state = ESP_RX_IDLE;
            } else if (ch == '\r') {
                /* Consecutive CR — treat previous CR as line end,
                 * this CR might start a new empty line */
                g_esp_line_buf[g_esp_line_pos] = '\0';
                process_line(g_esp_line_buf);
                g_esp_line_pos = 0;
                /* Stay in CR_SEEN for the possible following LF */
            } else {
                /* Lone CR not followed by LF — unusual but handle it:
                 * treat the line as complete and start a new one */
                g_esp_line_buf[g_esp_line_pos] = '\0';
                process_line(g_esp_line_buf);
                g_esp_line_pos = 0;
                if (g_esp_line_pos < ESP_LINE_BUF_SIZE - 1) {
                    g_esp_line_buf[g_esp_line_pos++] = (char)ch;
                }
                g_esp_rx_state = ESP_RX_LINE;
            }
            break;

        case ESP_RX_PAYLOAD:
            if (g_esp_payload_count < g_esp_payload_len
                && g_esp_payload_count < ESP_PAYLOAD_BUF_SIZE - 1) {
                g_esp_payload_buf[g_esp_payload_count++] = (char)ch;
            }
            if (g_esp_payload_count >= g_esp_payload_len) {
                /* Payload complete */
                g_esp_payload_buf[g_esp_payload_count] = '\0';
                g_esp_mqtt_data_received = true;
                g_esp_rx_state = ESP_RX_IDLE;
            }
            break;
        }
    }
}

/**
 * @brief  Discard all bytes currently in the ring buffer.
 */
void ESP_FlushRx(void)
{
    ring_flush();
    g_esp_line_pos      = 0;
    g_esp_rx_state      = ESP_RX_IDLE;
    /**
     * Preserve completed MQTT payload across flush operations.
     *
     * When an unsolicited +MQTTSUBRECV arrives mid-publish (while
     * ESP_PollRx is called inside the iotda_publish polling loop),
     * process_line sets g_esp_payload_len / g_esp_payload_count and
     * raises g_esp_mqtt_data_received.  The subsequent service's
     * iotda_publish call would then ESP_FlushRx and zero the length,
     * causing IoTDA_ProcessCommand to see payloadLen==0 and silently
     * discard the command.
     *
     * Only reset the payload length when there is no complete command
     * waiting to be consumed.
     */
    if (!g_esp_mqtt_data_received) {
        g_esp_payload_len   = 0;
        g_esp_payload_count = 0;
    }
    g_esp_data_prompt   = false;
}

/**
 * @brief  Send an AT command string and block until a response arrives.
 */
bool ESP_SendAndWait(const char *cmd, uint32_t timeoutMs)
{
    /* Clear stale flags */
    g_esp_ok_received     = false;
    g_esp_error_received  = false;
    g_esp_data_prompt     = false;
    ESP_FlushRx();

    /* Send command */
    /* Echo outgoing command to UART0 for debugging */
    Board_UART_WriteString(UART_DEBUG_INST, "[ESP TX] ");
    Board_UART_WriteString(UART_DEBUG_INST, cmd);
    Board_UART_WriteString(UART_DEBUG_INST, "\r\n");

    Board_UART_WriteString(UART_ESP_INST, cmd);
    Board_UART_WriteString(UART_ESP_INST, "\r\n");

    /* Poll until we get a response or timeout */
    uint32_t elapsed = 0;
    while (elapsed < timeoutMs) {
        delay_ms(10);
        elapsed += 10;

        ESP_PollRx();

        if (g_esp_ok_received) {
            Board_UART_WriteString(UART_DEBUG_INST, "[ESP] OK\r\n");
            return true;
        }
        if (g_esp_error_received) {
            Board_UART_WriteString(UART_DEBUG_INST, "[ESP] ERROR/FAIL\r\n");
            return false;
        }
    }

    Board_UART_WriteString(UART_DEBUG_INST, "[ESP] TIMEOUT\r\n");
    return false; /* Timed out */
}

/**
 * @brief  Send raw bytes over UART1.
 */
void ESP_SendData(const uint8_t *data, uint16_t len)
{
    uint16_t i;

    /* Step A: Send to ESP8266 FIRST — critical path must not be
     * preceded by UART0 debug traffic that could cause bus contention
     * or timing jitter on the UART1 TX line. */
    for (i = 0; i < len; i++) {
        Board_UART_Write(UART_ESP_INST, data[i]);
    }

    /* Step B: Echo to UART0 for debugging (full payload, no truncation).
     * This runs AFTER the ESP8266 has received the complete payload,
     * so any UART0 back-pressure cannot affect the transmission. */
    Board_UART_WriteString(UART_DEBUG_INST, "[ESP TX DATA] ");
    for (i = 0; i < len; i++) {
        Board_UART_Write(UART_DEBUG_INST, data[i]);
    }
    Board_UART_WriteString(UART_DEBUG_INST, "\r\n");
}

/**
 * @brief  Get a pointer to the most recently received MQTT payload.
 */
const char *ESP_GetMQTTData(uint16_t *len)
{
    *len = g_esp_payload_len;
    return g_esp_payload_buf;
}

/**
 * @brief  Get the topic from the most recent +MQTTSUBRECV.
 */
const char *ESP_GetMQTTTopic(void)
{
    return g_esp_topic_buf;
}

/**
 * @brief  Reset the MQTT payload receiver for the next message.
 */
void ESP_ConsumeMQTTData(void)
{
    g_esp_mqtt_data_received = false;
    g_esp_payload_len        = 0;
    g_esp_payload_count      = 0;
    g_esp_topic_buf[0]       = '\0';
}

/* ---- Internal: line processor ---- */

/**
 * @brief  Process one complete AT response line.
 *
 * Inspects the line content and sets the appropriate global flags.
 * Called from ESP_PollRx() inside the parser state machine.
 */
static void process_line(const char *line)
{
    /* Skip empty lines */
    if (line[0] == '\0') {
        return;
    }

    /* Echo every received line to UART0 for debugging */
    Board_UART_WriteString(UART_DEBUG_INST, "[ESP RX] ");
    Board_UART_WriteString(UART_DEBUG_INST, line);
    Board_UART_WriteString(UART_DEBUG_INST, "\r\n");

    /* ---- Unsolicited MQTT subscription data ---- */
    if (str_find(line, "+MQTTSUBRECV:") != 0xFFFF) {
        /**
         * Format: +MQTTSUBRECV:0,"topic",data_len,<raw bytes>
         *
         * The raw payload bytes follow the 3rd comma on the SAME
         * line.  Fields: link_id, topic, data_len, data.
         *
         * Extract the topic (first double-quoted string), parse
         * data_len after the 2nd comma, then copy inline data from
         * after the 3rd comma.  If the payload is longer than the
         * remaining line chars, switch to ESP_RX_PAYLOAD to collect
         * the rest from subsequent ring-buffer bytes.
         */
        uint16_t i;
        uint16_t topicStart = 0;
        uint16_t topicEnd   = 0;
        uint16_t commas     = 0;
        uint16_t dataStart  = 0;
        uint32_t payloadLen = 0;

        for (i = 0; line[i] != '\0'; i++) {
            if (line[i] == '"' && topicStart == 0) {
                topicStart = i + 1;
            } else if (line[i] == '"' && topicStart != 0
                       && topicEnd == 0) {
                topicEnd = i;
            }
            if (line[i] == ',') {
                commas++;
                if (commas == 2) {
                    /* Parse data_len after 2nd comma */
                    str_parse_uint32(&line[i + 1], &payloadLen);
                }
                if (commas == 3) {
                    /* Raw data starts after 3rd comma */
                    dataStart = i + 1;
                    break;
                }
            }
        }

        /* Copy the topic string */
        if (topicStart > 0 && topicEnd > topicStart
            && topicEnd - topicStart < ESP_TOPIC_BUF_SIZE - 1) {
            uint16_t n = topicEnd - topicStart;
            uint16_t j;
            for (j = 0; j < n; j++) {
                g_esp_topic_buf[j] = line[topicStart + j];
            }
            g_esp_topic_buf[n] = '\0';
        } else {
            g_esp_topic_buf[0] = '\0';
        }

        /* Collect payload data */
        if (payloadLen > 0 && payloadLen <= ESP_PAYLOAD_BUF_SIZE - 1
            && dataStart > 0) {
            uint16_t lineRemaining;
            uint16_t copyLen;

            g_esp_payload_len   = (uint16_t)payloadLen;
            g_esp_payload_count = 0;

            /* Copy inline portion from this line */
            lineRemaining = str_len(&line[dataStart]);
            copyLen = (lineRemaining < (uint16_t)payloadLen)
                          ? lineRemaining
                          : (uint16_t)payloadLen;
            for (i = 0; i < copyLen; i++) {
                g_esp_payload_buf[i] = line[dataStart + i];
            }
            g_esp_payload_count = copyLen;

            if (g_esp_payload_count >= g_esp_payload_len) {
                /* All data was on this line */
                g_esp_payload_buf[g_esp_payload_count] = '\0';
                g_esp_mqtt_data_received = true;
            } else {
                /* More bytes expected — switch to raw mode */
                g_esp_rx_state = ESP_RX_PAYLOAD;
            }
        }

        return;
    }

    /* ---- Connection loss indicators ---- */
    if (str_find(line, "WIFI DISCONNECT") != 0xFFFF ||
        str_find(line, "CLOSED") != 0xFFFF) {
        g_esp_disconnected = true;
        return;
    }

    /* ---- Success responses ---- */
    if (str_find(line, "OK") != 0xFFFF) {
        g_esp_ok_received = true;
        return;
    }

    /* ---- WiFi / MQTT connection success ---- */
    if (str_find(line, "WIFI CONNECTED") != 0xFFFF ||
        str_find(line, "WIFI GOT IP") != 0xFFFF ||
        str_find(line, "+MQTTCONNECTED:") != 0xFFFF) {
        g_esp_ok_received = true;
        return;
    }

    /* ---- Error responses ---- */
    if (str_find(line, "ERROR") != 0xFFFF ||
        str_find(line, "FAIL") != 0xFFFF) {
        g_esp_error_received = true;
        return;
    }

    /* ---- ">" prompt (MQTT publish data prompt) ---- */
    if (line[0] == '>') {
        g_esp_data_prompt = true;
        return;
    }

    /* Other lines (echoes, status messages, etc.) are silently ignored */
}
