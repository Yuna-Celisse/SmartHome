/******************************************************************************
 * @file esp8266_at.h
 *
 * @par dependencies
 *      - stdint.h, stdbool.h
 *      - board_init.h (Board_UART_Write*, UART_ESP_INST)
 *
 * @author Yuna-Celisse
 *
 * @brief  ESP8266 AT command driver — UART1 RX ring buffer and response
 *         line parser.
 *
 * The driver operates in two phases:
 *   1. **Init phase** — blocking AT command sequences using
 *      ESP_SendAndWait() with per-step timeouts.
 *   2. **Runtime phase** — non-blocking polling via ESP_PollRx()
 *      to detect unsolicited +MQTTSUBRECV messages and connection
 *      status changes.
 *
 * @version V1.0 2026-6-22
 *
 * @note 1 tab == 4 spaces!
 *****************************************************************************/

#ifndef ESP8266_AT_H
#define ESP8266_AT_H

#include <stdint.h>
#include <stdbool.h>

/**
 * @brief  Initialise the ring buffer and parser state.
 *
 * Must be called once after UART1 is initialised and the NVIC is
 * enabled, before any ESP_SendAndWait() or ESP_PollRx() calls.
 */
void ESP_Init(void);

/**
 * @brief  Feed one received byte into the UART1 RX ring buffer.
 *
 * Called from UART1_IRQHandler (ISR context). This is the ONLY
 * function in this module that runs at interrupt level — all
 * parsing happens in the main loop via ESP_PollRx().
 *
 * @param[in] ch  Byte received from ESP8266 on UART1 RX.
 */
void ESP_ProcessRxByte(uint8_t ch);

/**
 * @brief  Drain the ring buffer and run the response-line parser.
 *
 * Call once per main-loop iteration (every ~10 ms). Non-blocking —
 * processes as many bytes as are currently in the ring buffer.
 * Sets g_esp_ok_received / g_esp_error_received /
 * g_esp_mqtt_pending flags when a complete response is detected.
 *
 * After calling this function, check those global flags to decide
 * whether to advance the connection state machine.
 */
void ESP_PollRx(void);

/**
 * @brief  Blocking delay that continuously drains the ESP8266 RX buffer.
 *
 * Use this instead of a plain delay_ms() whenever the ESP8266 may
 * send unsolicited data (MQTT publish acks, cloud commands, etc.)
 * during the wait — otherwise the 256-byte ring buffer overflows
 * and the AT response parser loses synchronisation.
 *
 * @param[in] ms  Total delay in milliseconds.
 */
void ESP_DelayMs(uint32_t ms);

/**
 * @brief  Discard all bytes currently in the ring buffer.
 *
 * Useful for flushing stale data before starting a new AT
 * command sequence.
 */
void ESP_FlushRx(void);

/**
 * @brief  Send an AT command string and block until a response arrives.
 *
 * The function appends "\r\n" to cmd automatically. It polls
 * ESP_PollRx() every ~10 ms until OK or ERROR/FAIL is seen, or
 * timeoutMs elapses.
 *
 * @param[in] cmd       AT command text WITHOUT trailing \r\n.
 * @param[in] timeoutMs Maximum wait time in milliseconds.
 * @return true if OK was received, false on ERROR / timeout.
 */
bool ESP_SendAndWait(const char *cmd, uint32_t timeoutMs);

/**
 * @brief  Send raw bytes over UART1 (ESP8266 TX).
 *
 * Used to transmit MQTT payload bodies (the JSON data after the
 * AT+MQTTPUB ">" prompt). No response parsing is performed — the
 * caller is responsible for handling any subsequent "+MQTTPUB:OK"
 * or similar.
 *
 * @param[in] data  Pointer to the byte buffer.
 * @param[in] len   Number of bytes to send.
 */
void ESP_SendData(const uint8_t *data, uint16_t len);

/**
 * @brief  Get a pointer to the most recently received MQTT payload.
 *
 * Valid only after g_esp_mqtt_data_received is set to true by
 * ESP_PollRx(). The caller must call ESP_ConsumeMQTTData() after
 * processing to reset the payload receiver.
 *
 * @param[out] len  Receives the payload byte count.
 * @return Pointer to the payload buffer (null-terminated).
 */
const char *ESP_GetMQTTData(uint16_t *len);

/**
 * @brief  Get the topic from the most recent +MQTTSUBRECV.
 *
 * The topic is extracted from the +MQTTSUBRECV header line and
 * stored as a null-terminated string. Valid only after
 * g_esp_mqtt_data_received is true.
 *
 * @return Pointer to the topic string (null-terminated).
 */
const char *ESP_GetMQTTTopic(void);

/**
 * @brief  Reset the MQTT payload receiver for the next message.
 */
void ESP_ConsumeMQTTData(void);

/* ---- Global flags set by ESP_PollRx(), read by the state machine ---- */

/** Set to true when an "OK" line is parsed. */
extern bool g_esp_ok_received;

/** Set to true when an "ERROR" or "FAIL" line is parsed. */
extern bool g_esp_error_received;

/** Set to true when the MQTT publish ">" data prompt is received. */
extern bool g_esp_data_prompt;

/** Number of unconsumed +MQTTSUBRECV commands (0–2).  Check with >0. */
extern uint8_t g_esp_mqtt_pending;

/** Set to true when a "WIFI DISCONNECT" or MQTT "CLOSED" event is seen. */
extern bool g_esp_disconnected;

#endif /* ESP8266_AT_H */
