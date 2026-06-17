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

/* ---- UART0 helpers ---- */

/**
 * @brief  Initialize UART0 on PA10 (TX) / PA11 (RX) with 8N1, FIFO enabled.
 *
 * Configures GPIO IOMUX, resets and powers on the UART peripheral, sets
 * BUSCLK clock source, applies 8N1 framing, and enables RX/TX FIFOs.
 * The baud rate is auto-calculated from @p baudRate and CPUCLK_FREQ.
 *
 * @param[in] baudRate  Desired baud rate (e.g. 115200).
 */
void Board_UART_Init(uint32_t baudRate)
{
    /**
     * Configure PA10 as UART0 TX (peripheral output).
     * PA10 = IOMUX PINCM21, alternate function UART0_TX.
     */
    DL_GPIO_initPeripheralOutputFunction(GPIO_UART_IOMUX_TX,
        GPIO_UART_IOMUX_TX_FUNC);

    /**
     * Configure PA11 as UART0 RX (peripheral input).
     * PA11 = IOMUX PINCM22, alternate function UART0_RX.
     */
    DL_GPIO_initPeripheralInputFunction(GPIO_UART_IOMUX_RX,
        GPIO_UART_IOMUX_RX_FUNC);

    /**
     * Reset UART0 registers to defaults, then power on the peripheral.
     * Both operations are idempotent and return void.
     */
    DL_UART_Main_reset(UART_TEST_INST);
    DL_UART_Main_enablePower(UART_TEST_INST);

    /**
     * Clock source: BUSCLK @ 32 MHz with divide ratio 1.
     * This is the standard configuration for 32 MHz CPUCLK operation.
     */
    static const DL_UART_Main_ClockConfig clockCfg = {
        .clockSel    = DL_UART_MAIN_CLOCK_BUSCLK,
        .divideRatio = DL_UART_MAIN_CLOCK_DIVIDE_RATIO_1
    };
    DL_UART_Main_setClockConfig(UART_TEST_INST, &clockCfg);

    /**
     * Base UART configuration: normal mode, full-duplex, 8N1, no flow control.
     * These settings match the typical terminal / serial monitor defaults.
     */
    static const DL_UART_Main_Config uartCfg = {
        .mode        = DL_UART_MAIN_MODE_NORMAL,
        .direction   = DL_UART_MAIN_DIRECTION_TX_RX,
        .flowControl = DL_UART_MAIN_FLOW_CONTROL_NONE,
        .parity      = DL_UART_MAIN_PARITY_NONE,
        .wordLength  = DL_UART_MAIN_WORD_LENGTH_8_BITS,
        .stopBits    = DL_UART_MAIN_STOP_BITS_ONE
    };
    DL_UART_Main_init(UART_TEST_INST, &uartCfg);

    /**
     * Set baud rate. The DriverLib auto-calculates the oversampling
     * ratio and divisor from CPUCLK_FREQ and the requested baud rate.
     * If the exact rate is not achievable, the nearest valid rate is used.
     */
    DL_UART_Main_configBaudRate(UART_TEST_INST, CPUCLK_FREQ, baudRate);

    /**
     * Enable RX and TX FIFOs. RX FIFO threshold is set to 1 entry
     * so that Board_UART_RXAvailable() returns true as soon as a
     * single byte arrives.
     */
    DL_UART_Main_enableFIFOs(UART_TEST_INST);
    DL_UART_Main_setRXFIFOThreshold(UART_TEST_INST,
        DL_UART_MAIN_RX_FIFO_LEVEL_ONE_ENTRY);

    /**
     * Enable the UART peripheral. After this call, the UART is
     * ready for TX/RX via the blocking read/write helpers.
     */
    DL_UART_Main_enable(UART_TEST_INST);
}

/**
 * @brief  Check whether a byte is available in the UART RX FIFO.
 *
 * @return true if at least one byte is ready to read, false otherwise.
 */
bool Board_UART_RXAvailable(void)
{
    return !DL_UART_isRXFIFOEmpty(UART_TEST_INST);
}

/**
 * @brief  Read one byte from the UART RX FIFO (blocking if empty).
 *
 * This call blocks until a byte is available. Call
 * Board_UART_RXAvailable() first to avoid blocking.
 *
 * @return The received byte.
 */
uint8_t Board_UART_Read(void)
{
    return DL_UART_receiveDataBlocking(UART_TEST_INST);
}

/**
 * @brief  Transmit one byte over UART (blocking until TX FIFO has space).
 *
 * @param[in] data  Byte to transmit.
 */
void Board_UART_Write(uint8_t data)
{
    DL_UART_transmitDataBlocking(UART_TEST_INST, data);
}

/**
 * @brief  Transmit a null-terminated string over UART, one byte at a time.
 *
 * Each character is cast to uint8_t for the blocking TX call.
 * No line ending is appended — the caller must include \r\n if needed.
 *
 * @param[in] str  Null-terminated string to transmit.
 */
void Board_UART_WriteString(const char *str)
{
    while (*str) {
        Board_UART_Write((uint8_t)*str++);
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
