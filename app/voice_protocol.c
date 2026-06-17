/******************************************************************************
 * @file voice_protocol.c
 *
 * @par dependencies
 *      - board_init.h (Board_UART_Write, LED_TOGGLE, UART_VOICE_INST)
 *      - voice_protocol.h (this module's interface)
 *
 * @author Yuna-Celisse
 *
 * @brief 5-byte voice-module protocol parser and command handler.
 *
 * Receives raw bytes from the voice UART (UART3), assembles 5-byte
 * packets of the form AA 55 [type] [cmd] FB, and dispatches:
 *   - ALARM (0x26): toggle LED + send acknowledgment
 *   - Other valid packets: echo the command byte back on the voice UART
 *   - Invalid packets (wrong header/footer): silently discarded
 *
 * @version V1.0 2026-6-18
 *
 * @note 1 tab == 4 spaces!
 *****************************************************************************/

#include "voice_protocol.h"

/**
 * 5-byte receive buffer — filled one byte at a time by
 * Voice_Process_Byte() as bytes arrive on the voice UART.
 */
static uint8_t g_voice_rx_buf[5];

/**
 * Current write position in g_voice_rx_buf. Resets to 0 after
 * a complete packet is processed or an invalid header is detected.
 */
static uint8_t g_voice_rx_idx;

/**
 * @brief  Send a 5-byte response packet on the voice UART.
 *
 * Constructs and transmits a complete protocol packet with the
 * given type and command bytes. Used for both the init broadcast
 * and ALARM acknowledgment.
 *
 * @param[in] type  Type / source identifier byte.
 * @param[in] cmd   Command byte.
 */
static void voice_send_packet(uint8_t type, uint8_t cmd)
{
    Board_UART_Write(UART_VOICE_INST, VOICE_PKT_HEADER1);
    Board_UART_Write(UART_VOICE_INST, VOICE_PKT_HEADER2);
    Board_UART_Write(UART_VOICE_INST, type);
    Board_UART_Write(UART_VOICE_INST, cmd);
    Board_UART_Write(UART_VOICE_INST, VOICE_PKT_FOOTER);
}

void Voice_Protocol_Init(void)
{
    /**
     * Send the init broadcast to notify any connected voice
     * modules that the MCU has powered on and is ready.
     * Type = 0xFF (broadcast), Cmd = VOICE_CMD_INIT (0x67).
     */
    voice_send_packet(0xFF, VOICE_CMD_INIT);
}

void Voice_Process_Byte(uint8_t ch)
{
    /**
     * Header sync: if we are waiting for the first byte and it is
     * not 0xAA, discard it and stay at index 0. This prevents
     * noise or misaligned data from filling the buffer.
     */
    if (g_voice_rx_idx == 0 && ch != VOICE_PKT_HEADER1) {
        return;
    }

    g_voice_rx_buf[g_voice_rx_idx++] = ch;

    /**
     * Once 5 bytes have been accumulated, validate the packet
     * structure and dispatch the command.
     */
    if (g_voice_rx_idx >= 5) {
        g_voice_rx_idx = 0;

        /**
         * Validate protocol framing: header must be AA 55
         * and footer must be FB. Malformed packets are
         * silently discarded.
         */
        if (g_voice_rx_buf[1] == VOICE_PKT_HEADER2
            && g_voice_rx_buf[4] == VOICE_PKT_FOOTER) {

            uint8_t type = g_voice_rx_buf[2];
            uint8_t cmd  = g_voice_rx_buf[3];

            /**
             * Dispatch based on command byte.
             */
            if (cmd == VOICE_CMD_ALARM) {
                /**
                 * ALARM: toggle the board LED and send an
                 * acknowledgment packet with the same type
                 * and command bytes.
                 */
                LED_TOGGLE();
                voice_send_packet(type, cmd);
            } else {
                /**
                 * Other recognized commands: echo the
                 * command byte back on the voice UART
                 * (single-byte echo, not a full packet).
                 */
                Board_UART_Write(UART_VOICE_INST, cmd);
            }
        }
    }
}
