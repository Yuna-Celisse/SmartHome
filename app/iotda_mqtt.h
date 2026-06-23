/******************************************************************************
 * @file iotda_mqtt.h
 *
 * @par dependencies
 *      - esp8266_at.h (ESP_SendAndWait, ESP_SendData, ESP_PollRx)
 *      - cloud_config.h (WiFi / IoTDA / MQTT credentials)
 *      - json_helper.h (Json_BuildPropertyReport, Json_ParseCommand)
 *
 * @author Yuna-Celisse
 *
 * @brief  Huawei Cloud IoTDA MQTT protocol layer — connection state
 *         machine, periodic property reporting, and command dispatch.
 *
 * The state machine is driven by calling IoTDA_Step() repeatedly
 * (once per main-loop iteration). During the initial connection
 * sequence it blocks internally via ESP_SendAndWait(); once
 * OPERATIONAL it is fully non-blocking.
 *
 * @version V1.0 2026-6-22
 *
 * @note 1 tab == 4 spaces!
 *****************************************************************************/

#ifndef IOTDA_MQTT_H
#define IOTDA_MQTT_H

#include <stdint.h>
#include <stdbool.h>

/**
 * @brief  Connection state-machine states.
 */
typedef enum {
    IOTDA_INIT,
    IOTDA_ATE0,
    IOTDA_CWMODE,
    IOTDA_CWJAP,
    IOTDA_WAIT_NET,       /**< Wait for DNS / network to stabilise */
    IOTDA_MQTT_USERCFG,
    IOTDA_MQTT_CONN,
    IOTDA_MQTT_SUB,
    IOTDA_OPERATIONAL,
    IOTDA_RECONNECT
} IoTDAState;

/**
 * @brief  Set the system tick reference for internal timers.
 *
 * Must be called once per main-loop iteration with the current
 * system_tick value (in milliseconds) before IoTDA_Step(),
 * IoTDA_ReportProperties(), or IoTDA_ProcessCommand().
 *
 * @param[in] tickMs  Current system_tick from main.c.
 */
void IoTDA_SetTick(uint32_t tickMs);

/**
 * @brief  Run one step of the connection state machine.
 *
 * Call once per main-loop iteration (~10 ms). In non-OPERATIONAL
 * states this may block for up to 30 seconds (WiFi connect).
 *
 * @return true when the connection is fully established
 *         (OPERATIONAL), false otherwise.
 */
bool IoTDA_Step(void);

/**
 * @brief  Build and publish a property-report JSON payload.
 *
 * Reads current sensor values from the global state variables and
 * posts them to IoTDA. Call periodically from the main loop
 * (internal timer enforces ~10 s interval).
 */
void IoTDA_ReportProperties(void);

/**
 * @brief  Parse and dispatch a received MQTT command.
 *
 * Call when g_esp_mqtt_data_received is true. Parses the JSON
 * payload and updates global state (g_fan_level, g_fan_duty,
 * g_light_on, g_alarm_state) as appropriate. Sends a command
 * response ack to IoTDA.
 */
void IoTDA_ProcessCommand(void);

/**
 * @brief  Check whether the IoTDA connection is operational.
 *
 * @return true if in IOTDA_OPERATIONAL state.
 */
bool IoTDA_IsConnected(void);

#endif /* IOTDA_MQTT_H */
