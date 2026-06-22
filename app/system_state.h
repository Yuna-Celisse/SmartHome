/******************************************************************************
 * @file system_state.h
 *
 * @par dependencies
 *      - stdint.h
 *      - stdbool.h
 *
 * @author Yuna-Celisse
 *
 * @brief  Shared system state for the Smart Electric Fan.
 *
 * Defines mode enums, threshold constants, and extern globals that are
 * updated by voice_protocol (ISR context) and consumed by main.c (polling
 * loop).  ISR writes to uint32_t/bool globals are atomic on Cortex-M0+
 * bare-metal — no mutex is required for the polling loop to see updated
 * values on its next iteration.
 *
 * @version V1.0 2026-6-20
 *
 * @note 1 tab == 4 spaces!
 *****************************************************************************/

#ifndef SYSTEM_STATE_H
#define SYSTEM_STATE_H

#include <stdint.h>
#include <stdbool.h>

/* ========== Light control mode ========== */
typedef enum {
    LIGHT_MODE_AUTO   = 0,   /**< OPT3001 ambient light decides on/off */
    LIGHT_MODE_MANUAL = 1    /**< Voice command decides, auto disabled  */
} LightMode;

/* ========== Fan speed level ========== */
typedef enum {
    FAN_OFF  = 0,   /**< 0 % duty */
    FAN_LOW  = 1,   /**< 25 % duty */
    FAN_MED  = 2,   /**< 50 % duty */
    FAN_HIGH = 3,   /**< 75 % duty */
    FAN_MAX  = 4    /**< 100 % duty */
} FanLevel;

/* ========== Alarm state ========== */
typedef enum {
    ALARM_NORMAL = 0,   /**< No alarm active */
    ALARM_FIRING = 1    /**< Alarm triggered, LED blinking */
} AlarmState;

/* ========== Global state (defined in main.c) ========== */
extern LightMode   g_light_mode;       /**< AUTO or MANUAL */
extern bool        g_light_on;         /**< Current light output state */
extern FanLevel    g_fan_level;        /**< Current fan speed level */
extern uint8_t     g_fan_duty;         /**< Current PWM duty 0–100 */
extern AlarmState  g_alarm_state;      /**< NORMAL or FIRING */
extern uint32_t    g_alarm_cooldown_ms;/**< System tick when alarm may re-fire */
extern float       g_last_lux;         /**< Last valid OPT3001 reading (lux) */
extern float       g_last_temp;        /**< Last valid BME280 reading (°C) */
extern float       g_last_gyro_x;      /**< Last BMI160 X-axis (°/s) */
extern float       g_last_gyro_y;      /**< Last BMI160 Y-axis (°/s) */
extern float       g_last_gyro_z;      /**< Last BMI160 Z-axis (°/s) */

/* ========== Threshold constants ========== */

/** Light ON threshold — lux below this triggers auto-light ON */
#define LIGHT_ON_THRESHOLD_LUX      50.0f

/** Light OFF threshold — lux above this triggers auto-light OFF */
#define LIGHT_OFF_THRESHOLD_LUX     100.0f

/** Gyro alarm threshold — any axis exceeding this triggers alarm */
#define GYRO_ALARM_THRESHOLD_DPS    150.0f

/** Alarm cooldown period before a new alarm can be triggered */
#define ALARM_COOLDOWN_MS           5000u

/** Buzzer duration — stays active for 10s after trigger */
#define BUZZER_DURATION_MS          10000u

/* ========== Temperature → fan level thresholds ========== */

/** Below this temperature fan is OFF */
#define TEMP_FAN_OFF_THRESHOLD      25.0f
/** Below this temperature (and above OFF) fan is LOW */
#define TEMP_FAN_LOW_THRESHOLD      28.0f
/** Below this temperature (and above LOW) fan is MED */
#define TEMP_FAN_MED_THRESHOLD      32.0f
/** Below this temperature (and above MED) fan is HIGH; above is MAX */
#define TEMP_FAN_HIGH_THRESHOLD     35.0f

/* ========== Fan PWM duty cycles (percent) ========== */
#define FAN_DUTY_OFF    0
#define FAN_DUTY_LOW    25
#define FAN_DUTY_MED    50
#define FAN_DUTY_HIGH   75
#define FAN_DUTY_MAX    100

#endif /* SYSTEM_STATE_H */
