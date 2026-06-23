 /******************************************************************************
 * @file fan_control.c
 *
 * @par dependencies
 *      - board_init.h (Board_Fan_Init, Board_Fan_SetSpeed)
 *      - fan_control.h (this module's interface)
 *
 * @author Yuna-Celisse
 *
 * @brief Thermostatic fan control: translates TMP116 temperature
 *        readings into TB6612-driven fan speed via TIMA0 CCP3 PWM.
 *
 * Control algorithm:
 *
 * @verbatim
 *   Speed %
 *    100 ┤                         ┌────────
 *        │                        ╱
 *        │                      ╱
 *     20 ┤                    ╱
 *        │                  ╱
 *      0 ┼──────────────────┴────────────── T (C)
 *        0                20            40
 * @endverbatim
 *
 * - Below T_ON (20 C):  fan off (0%)
 * - T_ON .. T_MAX (40 C): linear 20% → 100%
 * - Above T_MAX: fan full speed (100%)
 *
 * Hysteresis (1 C band):
 * - Fan currently OFF: turns on  when T >  T_ON + HYST = 21 C
 * - Fan currently ON:  turns off when T <  T_ON - HYST = 19 C
 *
 * This prevents rapid on/off cycling when temperature hovers near
 * the threshold.
 *
 * @version V1.0 2026-6-21
 *
 * @note 1 tab == 4 spaces!
 *****************************************************************************/

#include "fan_control.h"
#include "board_init.h"

/* ---- Thermometer constants ---- */

/** Temperature below which the fan is turned off (Celsius). */
#define FAN_TEMP_OFF        20.0f

/** Temperature at which the fan reaches maximum speed (Celsius). */
#define FAN_TEMP_MAX        40.0f

/** Hysteresis band: +/- 1 C around the on/off threshold. */
#define FAN_TEMP_HYST       1.0f

/** Minimum duty cycle when the fan is running (avoids stall). */
#define FAN_DUTY_MIN        20U

/** Maximum duty cycle (full speed). */
#define FAN_DUTY_MAX        100U

/* ---- Internal state ---- */

/** Current fan speed percentage (0 .. 100). */
static uint8_t g_fanSpeed = 0;

/** true = fan is currently running (speed > 0). */
static bool g_fanRunning = false;

/* ---- Public API ---- */

/**
 * @brief  Initialize the fan control subsystem.
 *
 * Delegates hardware initialization to Board_Fan_Init() (TIMA0 PWM
 * on PA8, TB6612 direction GPIOs on PB0/PB1) and sets the internal
 * state to "fan off, speed 0%".
 */
void FanControl_Init(void)
{
    Board_Fan_Init();
    g_fanSpeed   = 0;
    g_fanRunning = false;
}

/**
 * @brief  Update fan speed based on the current temperature reading.
 *
 * Implements the thermostatic mapping with hysteresis:
 *   1. If the fan is currently OFF and temperature exceeds
 *      FAN_TEMP_OFF + FAN_TEMP_HYST → turn on at FAN_DUTY_MIN.
 *   2. If the fan is currently ON and temperature drops below
 *      FAN_TEMP_OFF - FAN_TEMP_HYST → turn off.
 *   3. Otherwise, if the fan is ON, linearly map temperature
 *      [FAN_TEMP_OFF .. FAN_TEMP_MAX] to duty [FAN_DUTY_MIN .. FAN_DUTY_MAX].
 *
 * @param[in] temperatureC  Current temperature from TMP116 (Celsius).
 */
void FanControl_Update(float temperatureC)
{
    uint8_t targetSpeed = 0;

    if (!g_fanRunning) {
        /**
         * Fan is off. Turn on only if temperature exceeds the
         * threshold + hysteresis (i.e., clear warming trend).
         */
        if (temperatureC > (FAN_TEMP_OFF + FAN_TEMP_HYST)) {
            g_fanRunning = true;
        }
    } else {
        /**
         * Fan is running. Turn off only if temperature drops below
         * the threshold - hysteresis (i.e., clear cooling trend).
         */
        if (temperatureC < (FAN_TEMP_OFF - FAN_TEMP_HYST)) {
            g_fanRunning = false;
        }
    }

    if (g_fanRunning) {
        if (temperatureC >= FAN_TEMP_MAX) {
            targetSpeed = FAN_DUTY_MAX;
        } else if (temperatureC <= FAN_TEMP_OFF) {
            targetSpeed = FAN_DUTY_MIN;
        } else {
            /**
             * Linear interpolation between
             * (FAN_TEMP_OFF, FAN_DUTY_MIN) and (FAN_TEMP_MAX, FAN_DUTY_MAX).
             */
            float ratio;
            ratio = (temperatureC - FAN_TEMP_OFF)
                    / (FAN_TEMP_MAX - FAN_TEMP_OFF);
            targetSpeed = (uint8_t)(FAN_DUTY_MIN
                + ratio * (FAN_DUTY_MAX - FAN_DUTY_MIN));
        }
    } else {
        targetSpeed = 0;
    }

    /* Apply new speed only if it differs from the current setting */
    if (targetSpeed != g_fanSpeed) {
        g_fanSpeed = targetSpeed;
        Board_Fan_SetSpeed(g_fanSpeed);
    }
}

/**
 * @brief  Get the current fan speed percentage.
 *
 * @return Current speed (0 = off, 100 = full).
 */
uint8_t FanControl_GetSpeed(void)
{
    return g_fanSpeed;
}

/**
 * @brief  Check whether the fan is currently running.
 *
 * @return true if fan speed > 0%, false otherwise.
 */
bool FanControl_IsRunning(void)
{
    return g_fanRunning;
}

/**
 * @brief  Directly set the fan PWM duty cycle.
 *
 * Intended for manual / voice-command fan control. Bypasses the
 * thermostatic temperature mapping. Clamps speedPercent to [0, 100].
 *
 * @param[in] speedPercent  Fan speed as a percentage (0 = off,
 *                          100 = full).
 */
void FanControl_SetSpeed(uint8_t speedPercent)
{
    if (speedPercent > 100) {
        speedPercent = 100;
    }

    g_fanSpeed   = speedPercent;
    g_fanRunning = (speedPercent > 0);

    Board_Fan_SetSpeed(speedPercent);
}
