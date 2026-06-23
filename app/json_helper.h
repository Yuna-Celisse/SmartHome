/******************************************************************************
 * @file json_helper.h
 *
 * @par dependencies
 *      - stdint.h, stdbool.h
 *      - str_utils.h (ftoa_fixed, str_find, str_parse_uint32)
 *
 * @author Yuna-Celisse
 *
 * @brief  Manual JSON builder and parser for Huawei Cloud IoTDA.
 *
 * All JSON is constructed character-by-character — no dynamic
 * allocation, no sprintf, no external library dependencies.
 *
 * @version V1.0 2026-6-22
 *
 * @note 1 tab == 4 spaces!
 *****************************************************************************/

#ifndef JSON_HELPER_H
#define JSON_HELPER_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/**
 * @brief  Service selector for Json_BuildServiceReport().
 */
typedef enum {
    JSON_SVC_FAN   = 0,   /**< FanService only */
    JSON_SVC_LIGHT = 1,   /**< LightService only */
    JSON_SVC_ALARM = 2    /**< AlarmService only */
} JsonService;

/**
 * @brief  Data bundle for a single property-report service call.
 *
 * All sensor/state values are packed into this struct and passed by
 * pointer to avoid cross-TU float-in-register ABI issues with
 * TI Arm Clang -O2 (Cortex-M0+ soft-float AAPCS).
 *
 * @note  The `lux` field is NOT used by Json_BuildServiceReport() —
 *        it reads g_json_lux directly (see below).  The field exists
 *        only for caller convenience.
 */
typedef struct {
    char     luxStr[8];   /* pre-formatted by caller via ftoa_fixed */
    float    temp;
    uint8_t  fanLevel;
    uint8_t  fanDuty;
    uint8_t  lightOn;
    uint8_t  lightMode;
    uint8_t  alarmState;
    float    gyroX;
    float    gyroY;
    float    gyroZ;
} ServiceReportData;

/**
 * @brief  Build an IoTDA property-report JSON payload for ONE service.
 *
 * Each service report is under 150 bytes — well below the ESP8266
 * MQTTPUBRAW 256-byte limit.
 *
 * @param[out] buf       Output buffer (≥ 256 bytes recommended).
 * @param[in]  bufSize   Size of the output buffer.
 * @param[in]  svc       Which service to report.
 * @param[in]  data      Pointer to sensor/state values (all fields read).
 * @return Number of bytes written (excluding null terminator),
 *         or 0 if bufSize is insufficient.
 */
size_t Json_BuildServiceReport(char *buf, size_t bufSize,
                               JsonService svc,
                               const ServiceReportData *data);

/**
 * @brief  Parse an incoming IoTDA command JSON payload.
 *
 * Detects the command_name field and extracts parameters for:
 *   - "setFan"    → parses FanLevel (0-4)
 *   - "setLight"  → parses LightState ("ON"/"OFF"), LightMode ("AUTO"/"MANUAL")
 *   - "resetAlarm"→ no parameters
 *   - "setFanMode"→ parses FanMode ("AUTO"/"MANUAL")
 *
 * @param[in]  json          Null-terminated JSON payload.
 * @param[out] outFanLevel   Fan level (0-4), valid only for setFan.
 * @param[out] outLightOn    Light state, valid only for setLight.
 * @param[out] outLightMode  Light mode (0=AUTO, 1=MANUAL), valid for setLight.
 * @param[out] outFanMode    Fan mode (0=AUTO, 1=MANUAL), valid for setFanMode.
 * @return 0 = setFan, 1 = setLight, 2 = resetAlarm, 3 = setFanMode,
 *         -1 = unknown.
 */
int Json_ParseCommand(const char *json,
                      uint8_t *outFanLevel,
                      bool *outLightOn,
                      uint8_t *outLightMode,
                      uint8_t *outFanMode);

#endif /* JSON_HELPER_H */
