/******************************************************************************
 * @file json_helper.c
 *
 * @par dependencies
 *      - json_helper.h (this module's interface)
 *      - str_utils.h (ftoa_fixed, str_len, str_find, str_parse_uint32)
 *
 * @author Yuna-Celisse
 *
 * @brief  Manual JSON builder and parser for Huawei Cloud IoTDA.
 *
 * All output is built character-by-character through a "cursor" pointer
 * that is advanced past each written byte. A sentinel "end" pointer
 * guards against buffer overflow.  No stdio, no malloc, no recursion.
 *
 * @version V1.0 2026-6-22
 *
 * @note 1 tab == 4 spaces!
 *****************************************************************************/

#include "json_helper.h"
#include "str_utils.h"
#include "system_state.h"
#include "cloud_config.h"
#include "board_init.h"

/* ---- Internal helpers (static) ---- */

/**
 * @brief  Write a single character, advancing the cursor.
 *
 * @param[in,out] pp   Pointer to the current write position (updated).
 * @param[in]     end  One past the last valid byte of the buffer.
 * @param[in]     c    Character to write.
 * @return true if there was space, false on overflow.
 */
static bool json_putc(char **pp, const char *end, char c)
{
    if (*pp >= end) {
        return false;
    }
    **pp = c;
    (*pp)++;
    return true;
}

/**
 * @brief  Write a null-terminated string literal.
 */
static bool json_puts(char **pp, const char *end, const char *s)
{
    while (*s) {
        if (!json_putc(pp, end, *s)) {
            return false;
        }
        s++;
    }
    return true;
}

/**
 * @brief  Write a signed 32-bit integer in decimal.
 */
static bool json_put_int(char **pp, const char *end, int32_t val)
{
    char    tmp[12];
    uint8_t pos = 0;

    if (val < 0) {
        if (!json_putc(pp, end, '-')) {
            return false;
        }
        val = -val;
    }

    if (val == 0) {
        return json_putc(pp, end, '0');
    }

    while (val > 0) {
        tmp[pos++] = '0' + (uint8_t)(val % 10);
        val /= 10;
    }

    while (pos > 0) {
        if (!json_putc(pp, end, tmp[--pos])) {
            return false;
        }
    }

    return true;
}

/**
 * @brief  Write a boolean as JSON "true" or "false".
 */
static bool json_put_bool(char **pp, const char *end, bool val)
{
    return json_puts(pp, end, val ? "true" : "false");
}

/**
 * @brief  Write a float with the given number of decimal places.
 *
 * Uses ftoa_fixed() from str_utils.h.
 */
static bool json_put_float(char **pp, const char *end,
                           float val, uint8_t decimals)
{
    char buf[16];
    ftoa_fixed(buf, val, decimals);
    return json_puts(pp, end, buf);
}

/* ---- Public API ---- */

/**
 * @brief  Build an IoTDA property-report JSON payload for ONE service.
 *
 * Each service produces a self-contained {"services":[{...}]} document
 * that the IoTDA platform accepts as a partial property report.
 */
size_t Json_BuildServiceReport(char *buf, size_t bufSize,
                               JsonService svc,
                               const ServiceReportData *data)
{
    char *p   = buf;
    char *end = buf + bufSize;

    if (!data) {
        return 0;
    }

    /**
     * Force every struct access to be a real memory load.  TI Arm Clang
     * -O2 on Cortex-M0+ can "optimise" const-qualified pointer derefs
     * into stale register values when the pointed-to memory sits in the
     * caller's stack frame and the compiler loses track of the store.
     */
    const volatile ServiceReportData *const vd =
        (const volatile ServiceReportData *)data;

    /* clang-format off */
    if (!json_puts(&p, end, "{\"services\":[{")) { return 0; }

    switch (svc) {

    case JSON_SVC_FAN:
        if (!json_puts(&p, end,
            "\"service_id\":\"FanService\",\"properties\":{")) { return 0; }
        if (!json_puts(&p, end, "\"FanLevel\":"))     { return 0; }
        if (!json_put_int(&p, end, vd->fanLevel))      { return 0; }
        if (!json_putc(&p, end, ','))                  { return 0; }
        if (!json_puts(&p, end, "\"FanDuty\":"))       { return 0; }
        if (!json_put_int(&p, end, vd->fanDuty))        { return 0; }
        if (!json_putc(&p, end, ','))                  { return 0; }
        if (!json_puts(&p, end, "\"Temperature\":"))   { return 0; }
        if (!json_put_float(&p, end, vd->temp, 1))      { return 0; }
        if (!json_putc(&p, end, '}'))                 { return 0; }
        break;

    case JSON_SVC_LIGHT:
        if (!json_puts(&p, end,
            "\"service_id\":\"LightService\",\"properties\":{")) { return 0; }
        if (!json_puts(&p, end, "\"Lux\":"))           { return 0; }
        /* Caller pre-formatted the lux value as a string —
         * no float operations in this TU at all. */
        {
            Board_UART_WriteString(UART_DEBUG_INST, "[JBLD] lux=");
            Board_UART_WriteString(UART_DEBUG_INST, vd->luxStr);
            Board_UART_WriteString(UART_DEBUG_INST, "\r\n");
            if (!json_puts(&p, end, vd->luxStr))        { return 0; }
        }
        if (!json_putc(&p, end, ','))                  { return 0; }
        if (!json_puts(&p, end, "\"LightState\":\""))  { return 0; }
        if (!json_puts(&p, end,
            vd->lightOn ? "ON" : "OFF"))               { return 0; }
        if (!json_putc(&p, end, '\"'))                 { return 0; }
        if (!json_putc(&p, end, ','))                  { return 0; }
        if (!json_puts(&p, end, "\"LightMode\":\""))   { return 0; }
        if (!json_puts(&p, end,
            (vd->lightMode == LIGHT_MODE_AUTO)
                ? "AUTO" : "MANUAL"))                  { return 0; }
        if (!json_putc(&p, end, '\"'))                 { return 0; }
        if (!json_putc(&p, end, '}'))                 { return 0; }
        break;

    case JSON_SVC_ALARM:
        if (!json_puts(&p, end,
            "\"service_id\":\"AlarmService\",\"properties\":{")) { return 0; }
        if (!json_puts(&p, end, "\"GyroX\":"))         { return 0; }
        if (!json_put_float(&p, end, vd->gyroX, 1))     { return 0; }
        if (!json_putc(&p, end, ','))                  { return 0; }
        if (!json_puts(&p, end, "\"GyroY\":"))         { return 0; }
        if (!json_put_float(&p, end, vd->gyroY, 1))     { return 0; }
        if (!json_putc(&p, end, ','))                  { return 0; }
        if (!json_puts(&p, end, "\"GyroZ\":"))         { return 0; }
        if (!json_put_float(&p, end, vd->gyroZ, 1))     { return 0; }
        if (!json_putc(&p, end, ','))                  { return 0; }
        if (!json_puts(&p, end, "\"AlarmState\":\""))  { return 0; }
        if (!json_puts(&p, end,
            (vd->alarmState == ALARM_FIRING)
                ? "FIRING" : "NORMAL"))                { return 0; }
        if (!json_putc(&p, end, '\"'))                 { return 0; }
        if (!json_putc(&p, end, '}'))                 { return 0; }
        break;
    }

    /**
     * IoTDA device-side property reports MUST include an event_time
     * field per the Huawei Cloud API specification.  Without it the
     * platform JSON-schema validator rejects the payload as "not in
     * correct JSON format".
     *
     * We don't have an RTC on the MSPM0G3507; the timestamp is
     * derived from the compile-time IOTDA_PWD_TIMESTAMP macro
     * (format YYYYMMDDHH) and extended with a fixed minute/second
     * suffix.  IoTDA accepts this as long as the timestamp is
     * syntactically valid ISO 8601 – the exact value is advisory.
     */
    if (!json_puts(&p, end, ",\"event_time\":\""))  { return 0; }
    if (!json_puts(&p, end, IOTDA_PWD_TIMESTAMP))    { return 0; }
    if (!json_puts(&p, end, "00Z\""))               { return 0; }
    if (!json_putc(&p, end, '}'))                   { return 0; }

    if (!json_puts(&p, end, "]}"))                 { return 0; }
    /* clang-format on */

    /* Null-terminate */
    if (p >= end) {
        return 0;
    }
    *p = '\0';

    return (size_t)(p - buf);
}

/* ---- Command JSON parser ---- */

/**
 * @brief  Parse an incoming IoTDA command JSON payload.
 *
 * Handles two formats:
 *   A) REST device message (from messages/down topic):
 *      {"name":"setFan","message":{"FanLevel":2}}
 *   B) MQTT command (from commands/request topic):
 *      {"command_name":"setFan","paras":{"fanLevel":2}}
 *
 * Both parameter naming conventions are supported:
 *   FanLevel / fanLevel — case-insensitive via substring search.
 */
int Json_ParseCommand(const char *json,
                      uint8_t *outFanLevel,
                      bool *outLightOn,
                      uint8_t *outLightMode,
                      uint8_t *outFanMode)
{
    uint16_t pos;

    if (!json) {
        return -1;
    }

    /* Detect command type */
    if (str_find(json, "\"setFan\"") != 0xFFFF
        && str_find(json, "FanMode") == 0xFFFF) {
        /**
         * setFan — extract FanLevel or fanLevel.
         * Only matches setFan (not setFanMode).
         */
        pos = str_find(json, "FanLevel\":");
        if (pos == 0xFFFF) {
            pos = str_find(json, "fanLevel\":");
        }
        if (pos != 0xFFFF) {
            uint32_t val = 0;
            /* "FanLevel": → 10 chars, value starts at pos+10 */
            str_parse_uint32(&json[pos + 10], &val);
            *outFanLevel = (uint8_t)(val > 4 ? 4 : val);
        }
        return 0;
    }

    if (str_find(json, "\"setFanMode\"") != 0xFFFF) {
        /**
         * setFanMode — extract FanMode ("AUTO" or "MANUAL").
         */
        pos = str_find(json, "FanMode\":");
        if (pos != 0xFFFF) {
            if (str_find(&json[pos], "\"AUTO\"") != 0xFFFF) {
                *outFanMode = FAN_MODE_AUTO;
            } else {
                *outFanMode = FAN_MODE_MANUAL;
            }
        }
        return 3;
    }

    if (str_find(json, "\"setLight\"") != 0xFFFF) {
        /**
         * setLight — extract LightState and LightMode.
         * Frontend sends:
         *   {"name":"setLight","message":{"LightState":"ON","LightMode":"AUTO"}}
         */
        pos = str_find(json, "LightState\":");
        if (pos == 0xFFFF) {
            pos = str_find(json, "lightOn\":");
        }
        if (pos != 0xFFFF) {
            if (str_find(&json[pos], "\"ON\"") != 0xFFFF ||
                str_find(&json[pos], "true")   != 0xFFFF) {
                *outLightOn = true;
            } else {
                *outLightOn = false;
            }
        }
        /* Parse LightMode */
        pos = str_find(json, "LightMode\":");
        if (pos != 0xFFFF) {
            if (str_find(&json[pos], "\"AUTO\"") != 0xFFFF) {
                *outLightMode = LIGHT_MODE_AUTO;
            } else {
                *outLightMode = LIGHT_MODE_MANUAL;
            }
        }
        return 1;
    }

    if (str_find(json, "\"resetAlarm\"") != 0xFFFF) {
        return 2;
    }

    return -1;
}
