/******************************************************************************
 * @file board_init.h
 *
 * @par dependencies
 *      - ti_msp_dl_config.h (SysConfig-generated peripheral macros)
 *
 * @author Yuna-Celisse
 *
 * @brief Board-level macros and function declarations for I2C, ADC, UART,
 *        and LED peripherals on the BOOSTXL-BASSENSORS sensor board.
 *
 * Hardware initialization (GPIO, I2C, ADC12) is handled by SysConfig.
 * This header provides:
 *  - Peripheral instance and pin mapping macros
 *  - Runtime read/write helper function declarations
 *  - LED control macros (PA0, active low)
 *
 * @version V1.0 2026-6-17
 *
 * @note 1 tab == 4 spaces!
 *****************************************************************************/

#ifndef BOARD_INIT_H
#define BOARD_INIT_H

#include "ti_msp_dl_config.h"

/* I2C1 bus — shared by HDC2010, TMP116, OPT3001 */
#define SENSOR_I2C                              I2C1

/* ADC0 CH2 — DRV5055 Hall sensor on PA25 */
#define DRV5055_ADC                             ADC0
#define DRV5055_ADC_CHANNEL                     DL_ADC12_INPUT_CHAN_2
#define DRV5055_ADC_MEM_IDX                     DL_ADC12_MEM_IDX_0

/* Sensor power-up delay after enable pins are driven low by SysConfig */
void Board_Sensor_Init(void);

/**
 * @brief  Initialize I2C1 peripheral (PB2 SCL / PB3 SDA @ 400kHz).
 *
 * Configures GPIO pinmux for I2C1 alternate function, resets and
 * powers on the I2C peripheral, sets BUSCLK clock source, configures
 * controller mode with 400kHz bus speed, and enables the controller.
 *
 * Must be called before any sensor I2C read/write operations.
 */
void Board_I2C_Init(void);

/**
 * @brief  Drive sensor enable pins low to power on the BoosterPack sensors.
 *
 * HDC2010_EN (PB24) and DRV5055_EN (PB15) are active-low load-switch
 * enables. Driving them low turns on the respective sensor LDOs.
 * Includes a 10ms delay for LDO ramp and sensor startup.
 */
void Board_Sensor_Enable(void);

/* I2C helpers */
void Board_I2C_Write(uint8_t slaveAddr, const uint8_t *data, uint8_t len);
void Board_I2C_Read(uint8_t slaveAddr, uint8_t *data, uint8_t len);
void Board_I2C_WriteReg(uint8_t slaveAddr, uint8_t reg, uint8_t value);
void Board_I2C_ReadReg(uint8_t slaveAddr, uint8_t reg, uint8_t *data, uint8_t len);

/* ADC helper */
uint16_t Board_ADC_Read(void);

/* ---- UART0: XDS110 debug back-channel, PA10(TX) / PA11(RX) ---- */
#define UART_DEBUG_INST             UART0
#define UART_DEBUG_BAUD             115200

#define UART_DEBUG_IOMUX_TX         (IOMUX_PINCM21)
#define UART_DEBUG_IOMUX_TX_FUNC    IOMUX_PINCM21_PF_UART0_TX

#define UART_DEBUG_IOMUX_RX         (IOMUX_PINCM22)
#define UART_DEBUG_IOMUX_RX_FUNC    IOMUX_PINCM22_PF_UART0_RX

/* Legacy aliases — kept for backwards compatibility */
#define UART_TEST_INST              UART_DEBUG_INST
#define UART_TEST_BAUD              UART_DEBUG_BAUD
#define GPIO_UART_IOMUX_TX          UART_DEBUG_IOMUX_TX
#define GPIO_UART_IOMUX_TX_FUNC     UART_DEBUG_IOMUX_TX_FUNC
#define GPIO_UART_IOMUX_RX          UART_DEBUG_IOMUX_RX
#define GPIO_UART_IOMUX_RX_FUNC     UART_DEBUG_IOMUX_RX_FUNC

/* ---- UART1: ESP8266 WiFi module, PB6(TX) / PB7(RX) ---- */
#define UART_ESP_INST               UART1
#define UART_ESP_BAUD               115200

#define UART_ESP_IOMUX_TX           (IOMUX_PINCM23)
#define UART_ESP_IOMUX_TX_FUNC      IOMUX_PINCM23_PF_UART1_TX

#define UART_ESP_IOMUX_RX           (IOMUX_PINCM24)
#define UART_ESP_IOMUX_RX_FUNC      IOMUX_PINCM24_PF_UART1_RX

/* ---- UART3: Voice module, PB12(TX) / PB13(RX) ---- */
#define UART_VOICE_INST             UART3
#define UART_VOICE_BAUD             115200

#define UART_VOICE_IOMUX_TX         (IOMUX_PINCM29)
#define UART_VOICE_IOMUX_TX_FUNC    IOMUX_PINCM29_PF_UART3_TX

#define UART_VOICE_IOMUX_RX         (IOMUX_PINCM30)
#define UART_VOICE_IOMUX_RX_FUNC    IOMUX_PINCM30_PF_UART3_RX

/**
 * @brief  Initialize UART0 (XDS110 debug) with 8N1, FIFOs, and RX interrupt.
 *
 * Configures PA10(TX)/PA11(RX) GPIO pinmux, resets and powers on the
 * UART peripheral, applies BUSCLK@32MHz, 8N1 framing, enables FIFOs,
 * and enables the UART RX interrupt at the peripheral level.
 * Caller must separately invoke NVIC_EnableIRQ(UART0_INT_IRQn).
 *
 * @param[in] baudRate  Desired baud rate (e.g. 115200).
 */
void Board_UART0_Init(uint32_t baudRate);

/**
 * @brief  Initialize UART1 (ESP8266) with 8N1, FIFOs, and RX interrupt.
 *
 * Configures PB6(TX)/PB7(RX) GPIO pinmux, resets and powers on the
 * UART peripheral, applies BUSCLK@32MHz, 8N1 framing, enables FIFOs,
 * and enables the UART RX interrupt at the peripheral level.
 * Caller must separately invoke NVIC_EnableIRQ(UART1_INT_IRQn).
 *
 * @param[in] baudRate  Desired baud rate (e.g. 115200).
 */
void Board_UART1_Init(uint32_t baudRate);

/**
 * @brief  Initialize UART3 (Voice module) with 8N1, FIFOs, and RX interrupt.
 *
 * Configures PB12(TX)/PB13(RX) GPIO pinmux, resets and powers on the
 * UART peripheral, applies BUSCLK@32MHz, 8N1 framing, enables FIFOs,
 * and enables the UART RX interrupt at the peripheral level.
 * Caller must separately invoke NVIC_EnableIRQ(UART3_INT_IRQn).
 *
 * @param[in] baudRate  Desired baud rate (e.g. 115200).
 */
void Board_UART3_Init(uint32_t baudRate);

/**
 * @brief  Check if at least one byte is available in the UART RX FIFO.
 * @param[in] uart  UART peripheral instance (e.g. UART0, UART1).
 * @return true if data is ready to read, false otherwise.
 */
bool Board_UART_RXAvailable(const UART_Regs *uart);

/**
 * @brief  Read one byte from UART RX (blocks until data arrives).
 * @param[in] uart  UART peripheral instance (e.g. UART0, UART1).
 * @return The received byte.
 */
uint8_t Board_UART_Read(const UART_Regs *uart);

/**
 * @brief  Transmit one byte over UART (blocks until TX FIFO has space).
 * @param[in] uart  UART peripheral instance (e.g. UART0, UART1).
 * @param[in] data  Byte to transmit.
 */
void Board_UART_Write(UART_Regs *uart, uint8_t data);

/**
 * @brief  Transmit a null-terminated string over UART byte by byte.
 * @param[in] uart  UART peripheral instance (e.g. UART0, UART1).
 * @param[in] str   Null-terminated string to transmit.
 */
void Board_UART_WriteString(UART_Regs *uart, const char *str);

/* ---- LED: PA0, active low (LOW = ON, HIGH = OFF) ---- */
#define LED_PORT      GPIO_LED_PORT
#define LED_PIN       GPIO_LED_USER_LED_PIN

#define LED_ON()      DL_GPIO_clearPins(LED_PORT, LED_PIN)
#define LED_OFF()     DL_GPIO_setPins(LED_PORT, LED_PIN)
#define LED_TOGGLE()  DL_GPIO_togglePins(LED_PORT, LED_PIN)

/* ---- TIMA0 PWM on PA1 (fan speed control) ---- */
#define FAN_PWM_TIM                                 TIMA0
#define FAN_PWM_CC_INDEX                            DL_TIMERA_CAPTURE_COMPARE_1_INDEX
#define FAN_PWM_IOMUX                               (IOMUX_PINCM2)
#define FAN_PWM_IOMUX_FUNC                          IOMUX_PINCM2_PF_TIMA0_CCP1
#define FAN_PWM_FREQ_HZ                             25000u
#define FAN_PWM_PERIOD                              1280u   /* 32MHz / 25kHz */

/**
 * @brief  Initialize TIMA0 in edge-aligned PWM mode on PA1.
 *
 * Configures PA1 pinmux to TIMA0_CCP1, resets and powers on
 * the timer peripheral, sets BUSCLK clock source at 32 MHz with
 * no division or prescale, and starts PWM output with 0% duty.
 *
 * Call once during board bring-up, after SYSCFG_DL_init().
 */
void Board_Fan_Init(void);

/**
 * @brief  Set fan PWM duty cycle on PA1 (TIMA0 CCP1).
 * @param[in] percent  Duty cycle 0–100 (clamped). 0 = off, 100 = full speed.
 */
void Board_Fan_SetDuty(uint8_t percent);

#endif /* BOARD_INIT_H */
