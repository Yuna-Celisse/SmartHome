/******************************************************************************
 * @file board_init.c
 *
 * @par dependencies
 *      - ti_msp_dl_config.h (SysConfig-generated DriverLib config)
 *      - board_init.h (this module's interface)
 *
 * @author Yuna-Celisse
 *
 * @brief Board-level I2C, ADC, and UART helper functions for
 *        BOOSTXL-BASSENSORS sensor expansion board.
 *
 * Hardware initialization (GPIO, I2C, ADC12) is handled by SysConfig
 * via SYSCFG_DL_init(). This module provides runtime read/write helpers:
 *  - Sensor power-up delay
 *  - I2C read/write helpers
 *  - UART0 (PA10/PA11) init and blocking I/O
 *  - ADC12 single-conversion read
 *
 * @version V1.0 2026-6-17
 *
 * @note 1 tab == 4 spaces!
 *****************************************************************************/

#include "board_init.h"

void Board_Sensor_Init(void)
{
    /* Wait for sensors to power up after enable pins go low (~1ms) */
    delay_cycles(32000);
}

/* ---- I2C helpers ---- */

void Board_I2C_Write(uint8_t slaveAddr, const uint8_t *data, uint8_t len)
{
    while (DL_I2C_getControllerStatus(SENSOR_I2C)
           & DL_I2C_CONTROLLER_STATUS_BUSY) {}

    DL_I2C_fillControllerTXFIFO(SENSOR_I2C, data, len);
    DL_I2C_startControllerTransfer(SENSOR_I2C, slaveAddr,
        DL_I2C_CONTROLLER_DIRECTION_TX, len);

    while (DL_I2C_getControllerStatus(SENSOR_I2C)
           & DL_I2C_CONTROLLER_STATUS_BUSY) {}
}

void Board_I2C_Read(uint8_t slaveAddr, uint8_t *data, uint8_t len)
{
    while (DL_I2C_getControllerStatus(SENSOR_I2C)
           & DL_I2C_CONTROLLER_STATUS_BUSY) {}

    DL_I2C_startControllerTransfer(SENSOR_I2C, slaveAddr,
        DL_I2C_CONTROLLER_DIRECTION_RX, len);

    while (DL_I2C_getControllerStatus(SENSOR_I2C)
           & DL_I2C_CONTROLLER_STATUS_BUSY) {}

    uint8_t i;
    for (i = 0; i < len; i++) {
        data[i] = DL_I2C_receiveControllerData(SENSOR_I2C);
    }
}

void Board_I2C_WriteReg(uint8_t slaveAddr, uint8_t reg, uint8_t value)
{
    uint8_t buf[2];
    buf[0] = reg;
    buf[1] = value;
    Board_I2C_Write(slaveAddr, buf, 2);
}

void Board_I2C_ReadReg(uint8_t slaveAddr, uint8_t reg, uint8_t *data, uint8_t len)
{
    /* Write register address */
    while (DL_I2C_getControllerStatus(SENSOR_I2C)
           & DL_I2C_CONTROLLER_STATUS_BUSY) {}

    DL_I2C_fillControllerTXFIFO(SENSOR_I2C, &reg, 1);
    DL_I2C_startControllerTransfer(SENSOR_I2C, slaveAddr,
        DL_I2C_CONTROLLER_DIRECTION_TX, 1);

    while (DL_I2C_getControllerStatus(SENSOR_I2C)
           & DL_I2C_CONTROLLER_STATUS_BUSY) {}

    /* Restart as read */
    DL_I2C_startControllerTransfer(SENSOR_I2C, slaveAddr,
        DL_I2C_CONTROLLER_DIRECTION_RX, len);

    while (DL_I2C_getControllerStatus(SENSOR_I2C)
           & DL_I2C_CONTROLLER_STATUS_BUSY) {}

    uint8_t i;
    for (i = 0; i < len; i++) {
        data[i] = DL_I2C_receiveControllerData(SENSOR_I2C);
    }
}

/* ---- UART helpers ---- */

/**
 * @brief  Shared UART peripheral initialization sequence.
 *
 * Resets, powers on, and configures a UART peripheral with BUSCLK@32MHz,
 * 8N1 framing, FIFOs enabled, and RX FIFO threshold = 1 entry.
 * The caller is responsible for GPIO pinmux configuration and any
 * interrupt enables.
 *
 * This helper eliminates code duplication between UART0 (XDS110 debug)
 * and UART1 (ESP8266) initialization.
 *
 * @param[in] uart      UART peripheral instance (e.g. UART0, UART1).
 * @param[in] baudRate  Desired baud rate (e.g. 115200).
 */
static void uart_periph_init(UART_Regs *uart, uint32_t baudRate)
{
    /**
     * Reset UART registers to defaults, then power on the peripheral.
     * Both operations are idempotent and return void.
     */
    DL_UART_Main_reset(uart);
    DL_UART_Main_enablePower(uart);

    /**
     * Clock source: BUSCLK @ 32 MHz with divide ratio 1.
     * This is the standard configuration for 32 MHz CPUCLK operation.
     */
    static const DL_UART_Main_ClockConfig clockCfg = {
        .clockSel    = DL_UART_MAIN_CLOCK_BUSCLK,
        .divideRatio = DL_UART_MAIN_CLOCK_DIVIDE_RATIO_1
    };
    DL_UART_Main_setClockConfig(uart, &clockCfg);

    /**
     * Base UART configuration: normal mode, full-duplex, 8N1,
     * no flow control. These settings match typical terminal
     * defaults and ESP8266 AT command requirements.
     */
    static const DL_UART_Main_Config uartCfg = {
        .mode        = DL_UART_MAIN_MODE_NORMAL,
        .direction   = DL_UART_MAIN_DIRECTION_TX_RX,
        .flowControl = DL_UART_MAIN_FLOW_CONTROL_NONE,
        .parity      = DL_UART_MAIN_PARITY_NONE,
        .wordLength  = DL_UART_MAIN_WORD_LENGTH_8_BITS,
        .stopBits    = DL_UART_MAIN_STOP_BITS_ONE
    };
    DL_UART_Main_init(uart, &uartCfg);

    /**
     * Set baud rate. DriverLib auto-calculates the oversampling
     * ratio and divisor from CPUCLK_FREQ and the requested baud rate.
     * If the exact rate is not achievable, the nearest valid rate is used.
     */
    DL_UART_Main_configBaudRate(uart, CPUCLK_FREQ, baudRate);

    /**
     * Enable RX and TX FIFOs. RX FIFO threshold is set to 1 entry
     * so that Board_UART_RXAvailable() returns true as soon as a
     * single byte arrives, and to trigger the UART RX interrupt
     * on every received byte for instant echo.
     */
    DL_UART_Main_enableFIFOs(uart);
    DL_UART_Main_setRXFIFOThreshold(uart,
        DL_UART_MAIN_RX_FIFO_LEVEL_ONE_ENTRY);

    /**
     * Enable the UART peripheral. After this call, TX/RX operations
     * are ready via blocking read/write helpers.
     */
    DL_UART_Main_enable(uart);
}

/**
 * @brief  Initialize UART0 (XDS110 debug) on PA10(TX)/PA11(RX).
 *
 * Configures GPIO pinmux, then delegates peripheral configuration
 * to uart_periph_init(). Also enables the UART RX interrupt at the
 * peripheral level so that received bytes are echoed immediately
 * via UART0_IRQHandler().
 *
 * The caller must separately call NVIC_EnableIRQ(UART0_INT_IRQn)
 * after this function returns.
 *
 * @param[in] baudRate  Desired baud rate (e.g. 115200).
 */
void Board_UART0_Init(uint32_t baudRate)
{
    /**
     * Configure PA10 as UART0 TX (peripheral output).
     * PA10 = IOMUX PINCM21, alternate function UART0_TX.
     */
    DL_GPIO_initPeripheralOutputFunction(UART_DEBUG_IOMUX_TX,
        UART_DEBUG_IOMUX_TX_FUNC);

    /**
     * Configure PA11 as UART0 RX (peripheral input).
     * PA11 = IOMUX PINCM22, alternate function UART0_RX.
     */
    DL_GPIO_initPeripheralInputFunction(UART_DEBUG_IOMUX_RX,
        UART_DEBUG_IOMUX_RX_FUNC);

    uart_periph_init(UART_DEBUG_INST, baudRate);

    /**
     * Enable UART RX interrupt at the peripheral level.
     * The NVIC must be separately enabled by the caller via
     * NVIC_EnableIRQ(UART0_INT_IRQn) to start receiving interrupts.
     */
    DL_UART_Main_enableInterrupt(UART_DEBUG_INST,
        DL_UART_MAIN_INTERRUPT_RX);
}

/**
 * @brief  Initialize UART1 (ESP8266 WiFi) on PB6(TX)/PB7(RX).
 *
 * Configures GPIO pinmux, then delegates peripheral configuration to
 * uart_periph_init(). Enables the UART RX interrupt at the peripheral
 * level so that ESP8266 responses are forwarded to UART0 in real time
 * via UART1_IRQHandler().
 *
 * The caller must separately call NVIC_EnableIRQ(UART1_INT_IRQn)
 * after this function returns.
 *
 * @param[in] baudRate  Desired baud rate (e.g. 115200).
 */
void Board_UART1_Init(uint32_t baudRate)
{
    /**
     * Configure PB6 as UART1 TX (peripheral output).
     * PB6 = IOMUX PINCM23, alternate function UART1_TX.
     */
    DL_GPIO_initPeripheralOutputFunction(UART_ESP_IOMUX_TX,
        UART_ESP_IOMUX_TX_FUNC);

    /**
     * Configure PB7 as UART1 RX (peripheral input).
     * PB7 = IOMUX PINCM24, alternate function UART1_RX.
     */
    DL_GPIO_initPeripheralInputFunction(UART_ESP_IOMUX_RX,
        UART_ESP_IOMUX_RX_FUNC);

    uart_periph_init(UART_ESP_INST, baudRate);

    /**
     * Enable UART RX interrupt at the peripheral level for
     * bidirectional bridge forwarding. The NVIC must be separately
     * enabled by the caller via NVIC_EnableIRQ(UART1_INT_IRQn).
     */
    DL_UART_Main_enableInterrupt(UART_ESP_INST,
        DL_UART_MAIN_INTERRUPT_RX);
}

/**
 * @brief  Initialize UART3 (Voice module) on PB12(TX)/PB13(RX).
 *
 * Configures GPIO pinmux, then delegates peripheral configuration to
 * uart_periph_init(). Enables the UART RX interrupt at the peripheral
 * level so that received protocol packets are processed in real time
 * via UART3_IRQHandler().
 *
 * The caller must separately call NVIC_EnableIRQ(UART3_INT_IRQn)
 * after this function returns.
 *
 * @param[in] baudRate  Desired baud rate (e.g. 115200).
 */
void Board_UART3_Init(uint32_t baudRate)
{
    /**
     * Configure PB12 as UART3 TX (peripheral output).
     * PB12 = IOMUX PINCM29, alternate function UART3_TX.
     */
    DL_GPIO_initPeripheralOutputFunction(UART_VOICE_IOMUX_TX,
        UART_VOICE_IOMUX_TX_FUNC);

    /**
     * Configure PB13 as UART3 RX (peripheral input).
     * PB13 = IOMUX PINCM30, alternate function UART3_RX.
     */
    DL_GPIO_initPeripheralInputFunction(UART_VOICE_IOMUX_RX,
        UART_VOICE_IOMUX_RX_FUNC);

    uart_periph_init(UART_VOICE_INST, baudRate);

    /**
     * Enable UART RX interrupt at the peripheral level for
     * real-time voice-module protocol processing. The NVIC must
     * be separately enabled by the caller via
     * NVIC_EnableIRQ(UART3_INT_IRQn).
     */
    DL_UART_Main_enableInterrupt(UART_VOICE_INST,
        DL_UART_MAIN_INTERRUPT_RX);
}

/**
 * @brief  Check whether a byte is available in the UART RX FIFO.
 *
 * Returns immediately — does not block. For polling-based RX,
 * call this before Board_UART_Read() to avoid blocking.
 *
 * @param[in] uart  UART peripheral instance (e.g. UART0, UART1).
 * @return true if at least one byte is ready to read, false otherwise.
 */
bool Board_UART_RXAvailable(const UART_Regs *uart)
{
    return !DL_UART_isRXFIFOEmpty(uart);
}

/**
 * @brief  Read one byte from the UART RX FIFO (blocking if empty).
 *
 * This call blocks until a byte is available. For non-blocking
 * operation, call Board_UART_RXAvailable() first to check.
 *
 * @param[in] uart  UART peripheral instance (e.g. UART0, UART1).
 * @return The received byte.
 */
uint8_t Board_UART_Read(const UART_Regs *uart)
{
    return DL_UART_receiveDataBlocking(uart);
}

/**
 * @brief  Transmit one byte over UART (blocking until TX FIFO has space).
 *
 * @param[in] uart  UART peripheral instance (e.g. UART0, UART1).
 * @param[in] data  Byte to transmit.
 */
void Board_UART_Write(UART_Regs *uart, uint8_t data)
{
    DL_UART_transmitDataBlocking(uart, data);
}

/**
 * @brief  Transmit a null-terminated string over UART, one byte at a time.
 *
 * Each character is cast to uint8_t for the blocking TX call.
 * No line ending is appended — the caller must include \r\n if needed.
 *
 * @param[in] uart  UART peripheral instance (e.g. UART0, UART1).
 * @param[in] str   Null-terminated string to transmit.
 */
void Board_UART_WriteString(UART_Regs *uart, const char *str)
{
    while (*str) {
        Board_UART_Write(uart, (uint8_t)*str++);
    }
}

/* ---- ADC helper ---- */

uint16_t Board_ADC_Read(void)
{
    DL_ADC12_startConversion(DRV5055_ADC);

    while (DL_ADC12_getStatus(DRV5055_ADC)
           == DL_ADC12_STATUS_CONVERSION_ACTIVE) {}

    return DL_ADC12_getMemResult(DRV5055_ADC, DRV5055_ADC_MEM_IDX);
}
