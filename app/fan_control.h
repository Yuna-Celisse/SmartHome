/******************************************************************************
 * @file fan_control.h
 *
 * @par dependencies
 *      - board_init.h (Board_Fan_SetSpeed)
 *
 * @author Yuna-Celisse
 *
 * @brief Thermostatic fan control: maps TMP116 temperature readings to
 *        TB6612-driven fan speed via TIMA0 PWM.
 *
 * The control algorithm implements a linear temperature-to-speed mapping
 * with hysteresis to prevent oscillation near threshold boundaries.
 *
 * Temperature  | Fan Speed
 * ------------ | ---------
 * < 20 C       |   0% (off)
 * 20 C .. 40 C |  20% → 100% (linear)
 * > 40 C       | 100% (full)
 *
 * Hysteresis: once the fan turns on, it stays on until temperature
 * drops 1 C below the activation threshold. Once the fan turns off,
 * it stays off until temperature rises 1 C above the deactivation
 * threshold.
 *
 * @version V1.0 2026-6-21
 *
 * @note 1 tab == 4 spaces!
 *****************************************************************************/

#ifndef FAN_CONTROL_H
#define FAN_CONTROL_H

#include <stdbool.h>
#include <stdint.h>

/**
 * @brief  Initialize the fan control subsystem.
 *
 * Calls Board_Fan_Init() to configure TIMA0 PWM (PA1) and TB6612
 * direction GPIOs (PB0, PB1). Sets the internal fan state to "off".
 * Must be called once after SYSCFG_DL_init() and before
 * FanControl_Update().
 */
void FanControl_Init(void);

/**
 * @brief  Update fan speed based on the current temperature reading.
 *
 * Applies the thermostatic mapping (20 C .. 40 C → 20% .. 100%)
 * with +/- 1 C hysteresis to prevent oscillation. This function
 * is designed to be called from the 1 Hz main loop.
 *
 * @param[in] temperatureC  Current temperature in degrees Celsius
 *                          (typically from TMP116_ReadTemperature()).
 */
void FanControl_Update(float temperatureC);

/**
 * @brief  Get the current fan speed percentage.
 *
 * @return Current speed as a percentage (0 = off, 100 = full).
 */
uint8_t FanControl_GetSpeed(void);

/**
 * @brief  Check whether the fan is currently running.
 *
 * @return true if fan speed > 0%, false otherwise.
 */
bool FanControl_IsRunning(void);

#endif /* FAN_CONTROL_H */
