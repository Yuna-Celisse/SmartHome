/******************************************************************************
 * @file cloud_config.h
 *
 * @par dependencies
 *      — (none; this is a pure configuration header)
 *
 * @author Yuna-Celisse
 *
 * @brief  Static WiFi and Huawei Cloud IoTDA credentials for the ESP8266.
 *
 * All credentials are consolidated in this single file so they can be
 * updated without touching the protocol logic.
 *
 * IMPORTANT: Do NOT commit the production WiFi password or device secret
 * to a public repository. Use a local .gitignore'd override or keep
 * this file in .gitignore when credentials contain secrets.
 *
 * @version V1.0 2026-6-22
 *
 * @note 1 tab == 4 spaces!
 *****************************************************************************/

#ifndef CLOUD_CONFIG_H
#define CLOUD_CONFIG_H

/* ========== WiFi Access Point ========== */

/** WiFi SSID — edit this to match your router. */
#define WIFI_SSID       "vivo"

/** WiFi password — edit this to match your router. */
#define WIFI_PASSWORD   "12345678"

/* ========== Huawei Cloud IoTDA Device Identity ========== */

/** IoTDA-registered device ID (matches frontend .env / config.json). */
#define IOTDA_DEVICE_ID     "6a369f1618855b39c526bc63_M001"

/**
 * Timestamp used when the MQTT password was generated (format YYYYMMDDHH).
 *
 * The IoTDA MQTT password is HMAC-SHA256(device_secret, timestamp).
 * This value MUST be updated when the password is regenerated.
 */
#define IOTDA_PWD_TIMESTAMP "2026062310"

/**
 * MQTT Client ID = {deviceId}_0_0_{timestamp}
 *
 * The "_0_0_" separator is a Huawei Cloud IoTDA convention.
 */
#define IOTDA_CLIENT_ID     IOTDA_DEVICE_ID "_0_0_" IOTDA_PWD_TIMESTAMP

/** MQTT username = device ID. */
#define IOTDA_USERNAME      IOTDA_DEVICE_ID

/**
 * MQTT password — pre-computed HMAC-SHA256(device_secret, timestamp).
 *
 * This is a 64-character lowercase hex string. If you have the raw
 * device_secret, use it instead and compute the HMAC at runtime.
 * Otherwise, regenerate this on the Huawei Cloud console before it
 * expires (typically valid for ~24 hours from the timestamp).
 */
#define IOTDA_PASSWORD \
    "ab27148ada157fa65ba69f0aa0378186c5fc488ba8e2b88979e4904bafa3e216"

/* ========== MQTT Broker ========== */

/** IoTDA MQTT device-access endpoint (instance-scoped). */
#define IOTDA_HOST \
    "c52bdd1643.st1.iotda-device.cn-east-3.myhuaweicloud.com"

/** MQTT port — 1883 = plain MQTT (no TLS), 8883 = MQTTS. */
#define IOTDA_PORT      1883

/* ========== MQTT Topics ========== */

/**
 * Property-report topic.
 * Publishes sensor readings as JSON to the device shadow.
 */
#define IOTDA_REPORT_TOPIC \
    "$oc/devices/" IOTDA_DEVICE_ID "/sys/properties/report"

/**
 * Command subscription topic (wildcard matches any request ID).
 * Receives setFan / setLight / resetAlarm commands.
 */
#define IOTDA_SUB_COMMAND \
    "$oc/devices/" IOTDA_DEVICE_ID "/sys/commands/request/+"

/**
 * Command response topic prefix.
 * Append "/{requestId}" before publishing.
 */
#define IOTDA_RESP_PREFIX \
    "$oc/devices/" IOTDA_DEVICE_ID "/sys/commands/response/"

/* ========== Timing Constants (milliseconds) ========== */

/** Maximum wait for ESP8266 to respond to a bare AT. */
#define ESP_INIT_TIMEOUT_MS         10000U

/** Maximum wait for WiFi association (AT+CWJAP). */
#define WIFI_CONNECT_TIMEOUT_MS     30000U

/** Maximum wait for MQTT broker connection (TLS handshake + CONNECT). */
#define MQTT_CONNECT_TIMEOUT_MS     15000U

/** Generic AT command response timeout. */
#define CMD_RESPONSE_TIMEOUT_MS      5000U

/** Interval between automatic property reports to IoTDA. */
#define PROPERTY_REPORT_INTERVAL_MS 10000U

/** Pause between reconnection attempts. */
#define RECONNECT_INTERVAL_MS       30000U

/** Maximum retries per connection step before falling back. */
#define MAX_RETRIES_PER_STEP            3U

#endif /* CLOUD_CONFIG_H */
