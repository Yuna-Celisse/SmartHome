/******************************************************************************
 * @file voice_protocol.h
 *
 * @par dependencies
 *      - board_init.h (LED macros, UART_VOICE_INST)
 *
 * @author Yuna-Celisse
 *
 * @brief 5-byte voice-module protocol constants and interface.
 *
 * Protocol format:  AA 55 [type] [cmd] FB
 *   - 0xAA 0x55: fixed header
 *   - 0xFB:      fixed footer
 *   - [type]:    command type / source identifier
 *   - [cmd]:     command byte
 *
 * Commands:
 *   FAN_OFF  (0x01): fan off (0%)
 *   FAN_L1   (0x02): fan level 1 (25%)
 *   FAN_L2   (0x03): fan level 2 (50%)
 *   FAN_L3   (0x04): fan level 3 (75%)
 *   FAN_L4   (0x05): fan level 4 (100%)
 *   LIGHT_OFF(0x0A): turn light off
 *   LIGHT_ON (0x0B): turn light on
 *
 * @version V2.0 2026-6-24
 *
 * @note 1 tab == 4 spaces!
 *****************************************************************************/

#ifndef VOICE_PROTOCOL_H
#define VOICE_PROTOCOL_H

#include <stdint.h>
#include "board_init.h"

/* ---- Protocol framing ---- */
#define VOICE_PKT_HEADER1       0xAA
#define VOICE_PKT_HEADER2       0x55
#define VOICE_PKT_FOOTER        0xFB

/* ---- Command bytes ---- */
#define VOICE_CMD_FAN_OFF       0x01   /**< Voice: fan off (0%)      */
#define VOICE_CMD_FAN_L1        0x02   /**< Voice: fan level 1 (25%) */
#define VOICE_CMD_FAN_L2        0x03   /**< Voice: fan level 2 (50%) */
#define VOICE_CMD_FAN_L3        0x04   /**< Voice: fan level 3 (75%) */
#define VOICE_CMD_FAN_L4        0x05   /**< Voice: fan level 4 (100%)*/
#define VOICE_CMD_LIGHT_OFF     0x0A   /**< Voice: turn light off    */
#define VOICE_CMD_LIGHT_ON      0x0B   /**< Voice: turn light on     */

/**
 * @brief  Feed one received byte into the 5-byte protocol parser.
 *
 * Accumulates bytes until a complete packet is assembled. Valid
 * commands set the corresponding global state variables and send
 * a 5-byte acknowledgment response on the voice UART. Unknown
 * commands are silently discarded.
 *
 * @param[in] ch  Received byte from the voice-module UART.
 */
void Voice_Process_Byte(uint8_t ch);

#endif /* VOICE_PROTOCOL_H */
