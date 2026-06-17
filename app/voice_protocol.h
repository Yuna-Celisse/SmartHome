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
 *   - [cmd]:     command byte (0x26 = ALARM, 0x67 = init, etc.)
 *
 * @version V1.0 2026-6-18
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
#define VOICE_CMD_ALARM         0x26
#define VOICE_CMD_INIT          0x67

/* ---- Color / type identifiers (from voice-module spec) ---- */
#define VOICE_TYPE_RED          0x66
#define VOICE_TYPE_BLUE         0x65
#define VOICE_TYPE_GREEN        0x64
#define VOICE_TYPE_YELLOW       0x63
#define VOICE_TYPE_THIS_RED     0x5F
#define VOICE_TYPE_THIS_BLUE    0x60
#define VOICE_TYPE_THIS_GREEN   0x61
#define VOICE_TYPE_THIS_YELLOW  0x62

/**
 * @brief  Send the init broadcast packet (AA 55 FF 67 FB) on the voice UART.
 *
 * Called once at startup to notify connected voice modules that the
 * MCU is ready.
 */
void Voice_Protocol_Init(void);

/**
 * @brief  Feed one received byte into the 5-byte protocol parser.
 *
 * Accumulates bytes until a complete packet is assembled. On detection
 * of the ALARM command (0x26), toggles the LED and sends an
 * acknowledgment response. Other valid commands are echoed back on
 * the voice UART.
 *
 * @param[in] ch  Received byte from the voice-module UART.
 */
void Voice_Process_Byte(uint8_t ch);

#endif /* VOICE_PROTOCOL_H */
