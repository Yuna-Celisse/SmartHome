/******************************************************************************
 * @file sensor_opt3001.h
 *
 * @par dependencies
 *      - board_init.h (SENSOR_I2C, delay_cycles)
 *
 * @author Yuna-Celisse
 *
 * @brief OPT3001 ambient light sensor interface (BOOSTXL-SENSORS).
 *
 * The OPT3001 measures visible light intensity in lux. Communication
 * over I2C1. ADDR pin tied to GND on BOOSTXL-SENSORS → address 0x44.
 *
 * Configuration: auto-range, continuous mode, 800ms conversion time,
 * latched window-style comparison.
 *
 * @version V1.1 2026-6-20
 *
 * @note 1 tab == 4 spaces!
 *****************************************************************************/

#ifndef SENSOR_OPT3001_H
#define SENSOR_OPT3001_H

#include "board_init.h"

/* BOOSTXL-SENSORS: ADDR = SCL → 0x47 */
#define OPT3001_I2C_ADDR      0x47

/* OPT3001 registers */
#define OPT3001_REG_RESULT         0x00
#define OPT3001_REG_CONFIG         0x01
#define OPT3001_REG_LOW_LIMIT      0x02
#define OPT3001_REG_HIGH_LIMIT     0x03
#define OPT3001_REG_MANUFACTURER   0x7E
#define OPT3001_REG_DEVICE_ID      0x7F

/* Configuration value (0xCC10): auto-range, continuous, 800ms, latch */
#define OPT3001_CONFIG_VALUE_MSB   0xCC
#define OPT3001_CONFIG_VALUE_LSB   0x10

/**
 * @brief  Initialize OPT3001: configure auto-range continuous mode.
 *
 * Uses raw DL I2C API to write the 16-bit configuration register,
 * bypassing Board_I2C_Write which produces corrupted register values
 * on this board.
 *
 * @return 0 on success, -1 on I2C error.
 */
int OPT3001_Init(void);

/**
 * @brief  Read ambient light intensity in lux.
 *
 * Reads the 16-bit result register (0x00) via raw DL I2C and converts
 * using the OPT3001 formula: lux = mantissa × 2^exponent × 0.01.
 *
 * Range: 0.01 to 83865 lux (auto-range).
 *
 * @return Illuminance in lux.
 */
float OPT3001_ReadLux(void);

#endif /* SENSOR_OPT3001_H */
