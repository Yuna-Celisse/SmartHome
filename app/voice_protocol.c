/******************************************************************************
 * @file voice_protocol.c
 *
 * @par dependencies
 *      - board_init.h (Board_UART_Write, LED macros, UART_VOICE_INST)
 *      - voice_protocol.h (this module's interface)
 *
 * @author Yuna-Celisse
 *
 * @brief 5-byte voice-module protocol parser and command handler.
 *
 * Receives raw bytes from the voice UART (UART3), assembles 5-byte
 * packets of the form AA 55 [type] [cmd] FB, and dispatches:
 *   - FAN_OFF   (0x01): manual mode, fan 0%, send ack
 *   - FAN_L1    (0x02): manual mode, fan 25%, send ack
 *   - FAN_L2    (0x03): manual mode, fan 50%, send ack
 *   - FAN_L3    (0x04): manual mode, fan 75%, send ack
 *   - FAN_L4    (0x05): manual mode, fan 100%, send ack
 *   - LIGHT_OFF (0x0A): manual mode, LED off, send ack
 *   - LIGHT_ON  (0x0B): manual mode, LED on, send ack
 *   - Unknown commands: silently discarded
 *   - Invalid packets (wrong header/footer): silently discarded
 *
 * @version V2.0 2026-6-24
 *
 * @note 1 tab == 4 spaces!
 *****************************************************************************/

#include "voice_protocol.h"
#include "system_state.h"

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
 * given type and command bytes. Used for acknowledgment responses
 * to valid voice commands.
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

            switch (cmd) {
            case VOICE_CMD_FAN_OFF:
                g_fan_level = FAN_OFF;
                g_fan_duty  = 0;
                g_fan_mode  = FAN_MODE_MANUAL;
                voice_send_packet(type, cmd);
                break;

            case VOICE_CMD_FAN_L1:
                g_fan_level = FAN_LOW;
                g_fan_duty  = 25;
                g_fan_mode  = FAN_MODE_MANUAL;
                voice_send_packet(type, cmd);
                break;

            case VOICE_CMD_FAN_L2:
                g_fan_level = FAN_MED;
                g_fan_duty  = 50;
                g_fan_mode  = FAN_MODE_MANUAL;
                voice_send_packet(type, cmd);
                break;

            case VOICE_CMD_FAN_L3:
                g_fan_level = FAN_HIGH;
                g_fan_duty  = 75;
                g_fan_mode  = FAN_MODE_MANUAL;
                voice_send_packet(type, cmd);
                break;

            case VOICE_CMD_FAN_L4:
                g_fan_level = FAN_MAX;
                g_fan_duty  = 100;
                g_fan_mode  = FAN_MODE_MANUAL;
                voice_send_packet(type, cmd);
                break;

            case VOICE_CMD_LIGHT_OFF:
                g_light_mode = LIGHT_MODE_MANUAL;
                g_light_on   = false;
                LED_OFF();
                voice_send_packet(type, cmd);
                break;

            case VOICE_CMD_LIGHT_ON:
                g_light_mode = LIGHT_MODE_MANUAL;
                g_light_on   = true;
                LED_ON();
                voice_send_packet(type, cmd);
                break;

            default:
                /* Unknown command — silently discard. */
                break;
            }
        }
    }
}
