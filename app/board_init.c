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

/**
 * @brief  Initialize I2C1 on PB2(SCL) / PB3(SDA) at 400kHz.
 *
 * The SysConfig-generated ti_msp_dl_config.c does not include I2C
 * initialization (the I2C module is configured in the .syscfg but
 * the init function call is absent from the generated SYSCFG_DL_init).
 * This function fills that gap by performing the full I2C1 bring-up
 * sequence: pinmux, reset, power, clock config, timer period for
 * 400kHz, and controller enable.
 */
void Board_I2C_Init(void)
{
    /* ---- GPIO pinmux: PB2 = I2C1 SCL, PB3 = I2C1 SDA ---- */
    DL_GPIO_initPeripheralInputFunctionFeatures(
        IOMUX_PINCM15, IOMUX_PINCM15_PF_I2C1_SCL,
        DL_GPIO_INVERSION_DISABLE, DL_GPIO_RESISTOR_NONE,
        DL_GPIO_HYSTERESIS_DISABLE, DL_GPIO_WAKEUP_DISABLE);
    DL_GPIO_initPeripheralInputFunctionFeatures(
        IOMUX_PINCM16, IOMUX_PINCM16_PF_I2C1_SDA,
        DL_GPIO_INVERSION_DISABLE, DL_GPIO_RESISTOR_NONE,
        DL_GPIO_HYSTERESIS_DISABLE, DL_GPIO_WAKEUP_DISABLE);
    DL_GPIO_enableHiZ(IOMUX_PINCM15);
    DL_GPIO_enableHiZ(IOMUX_PINCM16);

    /* ---- Reset and power on the I2C1 peripheral ---- */
    DL_I2C_reset(SENSOR_I2C);
    DL_I2C_enablePower(SENSOR_I2C);

    /* ---- Clock: BUSCLK @ 32 MHz, divide-by-1 ---- */
    static const DL_I2C_ClockConfig i2cClkCfg = {
        .clockSel    = DL_I2C_CLOCK_BUSCLK,
        .divideRatio = DL_I2C_CLOCK_DIVIDE_1
    };
    DL_I2C_setClockConfig(SENSOR_I2C, &i2cClkCfg);

    /* ---- Analog glitch filter (kept disabled) ---- */
    DL_I2C_disableAnalogGlitchFilter(SENSOR_I2C);

    /* ---- Controller mode, 100 kHz bus speed ---- */
    DL_I2C_resetControllerTransfer(SENSOR_I2C);
    DL_I2C_setTimerPeriod(SENSOR_I2C, 31);
    DL_I2C_setControllerTXFIFOThreshold(SENSOR_I2C,
        DL_I2C_TX_FIFO_LEVEL_BYTES_1);
    DL_I2C_setControllerRXFIFOThreshold(SENSOR_I2C,
        DL_I2C_RX_FIFO_LEVEL_BYTES_1);
    DL_I2C_disableControllerClockStretching(SENSOR_I2C);

    /* Enable the I2C controller */
    DL_I2C_enableController(SENSOR_I2C);
}

/**
 * @brief  Drive sensor enable pins low to power on BoosterPack sensors.
 *
 * The BOOSTXL-BASSENSORS uses active-low load switches (TPS22918) to
 * control power to each sensor. Driving the EN pin low turns on the
 * LDO; floating or high leaves the sensor unpowered.
 *
 * - HDC2010_EN: PB24, IOMUX_PINCM52
 * - DRV5055_EN: PB15, IOMUX_PINCM32
 *
 * Both pins are configured as digital outputs driven LOW, then a
 * 10ms delay allows LDO ramp and sensor startup.
 */
void Board_Sensor_Enable(void)
{
    /**
     * BOOSTXL-BASSENSORS sensor power enables (per TI data_sensor_aggregator):
     *   HDC_V  → PB24 (PINCM52)  active LOW
     *   DRV_V  → PA22 (PINCM47)  active LOW
     *   OPT_V  → PA24 (PINCM54)  active HIGH (!)
     */

    /* HDC2010: PB24, drive LOW to power on */
    DL_GPIO_initDigitalOutput(IOMUX_PINCM52);
    DL_GPIO_clearPins(GPIOB, DL_GPIO_PIN_24);
    DL_GPIO_enableOutput(GPIOB, DL_GPIO_PIN_24);

    /* DRV5055: PA22, drive LOW to power on */
    DL_GPIO_initDigitalOutput(IOMUX_PINCM47);
    DL_GPIO_clearPins(GPIOA, DL_GPIO_PIN_22);
    DL_GPIO_enableOutput(GPIOA, DL_GPIO_PIN_22);

    /* OPT3001: PA24, drive HIGH to power on */
    DL_GPIO_initDigitalOutput(IOMUX_PINCM54);
    DL_GPIO_setPins(GPIOA, DL_GPIO_PIN_24);
    DL_GPIO_enableOutput(GPIOA, DL_GPIO_PIN_24);

    /* Allow LDOs to ramp and sensors to power up (~10ms) */
    delay_cycles(320000);
}

/* ---- I2C helpers ---- */

#define I2C_TIMEOUT_CYCLES  (320000U)  /* ~10ms at 32MHz */

void Board_I2C_Write(uint8_t slaveAddr, const uint8_t *data, uint8_t len)
{
    DL_I2C_fillControllerTXFIFO(SENSOR_I2C, data, len);

    /* Wait for controller to be idle before starting */
    uint32_t timeout = I2C_TIMEOUT_CYCLES;
    while (!(DL_I2C_getControllerStatus(SENSOR_I2C)
             & DL_I2C_CONTROLLER_STATUS_IDLE)) {
        if (--timeout == 0) return;
    }

    DL_I2C_startControllerTransfer(SENSOR_I2C, slaveAddr,
        DL_I2C_CONTROLLER_DIRECTION_TX, len);

    /* Poll BUSY_BUS until the bus is free (STOP detected) */
    timeout = I2C_TIMEOUT_CYCLES;
    while (DL_I2C_getControllerStatus(SENSOR_I2C)
           & DL_I2C_CONTROLLER_STATUS_BUSY_BUS) {
        if (--timeout == 0) return;
    }
}

void Board_I2C_Read(uint8_t slaveAddr, uint8_t *data, uint8_t len)
{
    /* Wait for controller to be idle */
    uint32_t timeout = I2C_TIMEOUT_CYCLES;
    while (!(DL_I2C_getControllerStatus(SENSOR_I2C)
             & DL_I2C_CONTROLLER_STATUS_IDLE)) {
        if (--timeout == 0) return;
    }

    DL_I2C_startControllerTransfer(SENSOR_I2C, slaveAddr,
        DL_I2C_CONTROLLER_DIRECTION_RX, len);

    /* Poll RX FIFO and read byte by byte (per TI polling example) */
    uint8_t i;
    for (i = 0; i < len; i++) {
        timeout = I2C_TIMEOUT_CYCLES;
        while (DL_I2C_isControllerRXFIFOEmpty(SENSOR_I2C)) {
            if (--timeout == 0) return;
        }
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
    uint32_t timeout;

    /* Step 1: Write register address */
    DL_I2C_fillControllerTXFIFO(SENSOR_I2C, &reg, 1);

    timeout = I2C_TIMEOUT_CYCLES;
    while (!(DL_I2C_getControllerStatus(SENSOR_I2C)
             & DL_I2C_CONTROLLER_STATUS_IDLE)) {
        if (--timeout == 0) return;
    }

    DL_I2C_startControllerTransfer(SENSOR_I2C, slaveAddr,
        DL_I2C_CONTROLLER_DIRECTION_TX, 1);

    timeout = I2C_TIMEOUT_CYCLES;
    while (DL_I2C_getControllerStatus(SENSOR_I2C)
           & DL_I2C_CONTROLLER_STATUS_BUSY_BUS) {
        if (--timeout == 0) return;
    }

    /* Step 2: Read data */
    timeout = I2C_TIMEOUT_CYCLES;
    while (!(DL_I2C_getControllerStatus(SENSOR_I2C)
             & DL_I2C_CONTROLLER_STATUS_IDLE)) {
        if (--timeout == 0) return;
    }

    DL_I2C_startControllerTransfer(SENSOR_I2C, slaveAddr,
        DL_I2C_CONTROLLER_DIRECTION_RX, len);

    uint8_t i;
    for (i = 0; i < len; i++) {
        timeout = I2C_TIMEOUT_CYCLES;
        while (DL_I2C_isControllerRXFIFOEmpty(SENSOR_I2C)) {
            if (--timeout == 0) return;
        }
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

/* ---- Simple busy-wait delay ---- */

/**
 * @brief  Simple busy-wait delay (approximate milliseconds at 32 MHz).
 */
void delay_ms(uint32_t ms)
{
    uint32_t i;
    for (i = 0; i < ms; i++) {
        delay_cycles(32000);
    }
}

/* ---- Fan PWM (TB6612) helpers ---- */

/**
 * @brief  Initialize TB6612 direction GPIOs and start the PWM timer.
 *
 * TIMA0 CCP0 PWM on PA8 is configured by SysConfig's
 * SYSCFG_DL_PWM_FAN_init() — called automatically by SYSCFG_DL_init().
 * This function:
 *   1. Configures PB0 (AIN1) and PB1 (AIN2) as digital outputs for
 *      TB6612 direction control and sets forward rotation
 *      (AIN1 = HIGH, AIN2 = LOW).
 *   2. Starts the timer counter. SysConfig leaves the counter stopped
 *      so that duty cycle can be set before the first PWM cycle.
 *
 * @note  SYSCFG_DL_init() must have been called before this function.
 */
void Board_Fan_Init(void)
{
    /**
     * Configure PB0 and PB1 as digital outputs for TB6612 direction
     * control. AIN1 = HIGH + AIN2 = LOW → forward rotation.
     */
    DL_GPIO_initDigitalOutput(FAN_AIN1_IOMUX);
    DL_GPIO_initDigitalOutput(FAN_AIN2_IOMUX);

    DL_GPIO_setPins(FAN_AIN1_PORT, FAN_AIN1_PIN);
    DL_GPIO_clearPins(FAN_AIN2_PORT, FAN_AIN2_PIN);

    /**
     * Start the timer counter. SysConfig initializes TIMA0 in PWM
     * mode with the clock enabled but counter stopped (startTimer =
     * DL_TIMER_STOP). The initial CCP0 compare value is set by
     * SysConfig to produce 0% duty cycle (fan off).
     */
    DL_TimerA_startCounter(FAN_PWM_INST);

    /**
     * After the counter starts, the SysConfig initial CC value
     * (1280, which is > LOAD=1279) never matches — the output
     * would go HIGH on the first zero-event and stay stuck there
     * (100 % duty).  Force a correct 0 % compare value immediately.
     */
    Board_Fan_SetSpeed(0);
}

/**
 * @brief  Set fan speed by updating the CCP0 PWM compare value.
 *
 * In edge-aligned down-counting mode, the PWM output goes HIGH at
 * counter = 0 and LOW when the counter matches CCP0 on the way down.
 * duty = (period - CC) / period (without inversion).  Therefore:
 *   - speedPercent = 0%  → CC = period   (output stays LOW)
 *   - speedPercent = 100% → CC = 0        (output stays HIGH)
 *   - speedPercent = X%  → CC = period * (100 - X) / 100
 *
 * For 0% speed the timer keeps running with CC = period, ensuring
 * a clean LOW output (vs. stopping the timer, which may glitch).
 *
 * @param[in] speedPercent  Fan speed as a percentage (0 = off, 100 = full).
 */
void Board_Fan_SetSpeed(uint8_t speedPercent)
{
    uint32_t ccValue;

    if (speedPercent > 100) {
        speedPercent = 100;
    }

    /**
     * Map percentage to compare value.
     *
     * Edge-aligned down-counting (LOAD = period - 1):
     *   CC = LOAD → 0% duty (match at reload, output stays LOW)
     *   CC = 0    → 100% duty (HIGH entire cycle)
     *
     * Linear interpolation:
     *   CC = (period - 1) * (100 - speed%) / 100
     */
    ccValue = (uint32_t)(FAN_PWM_PERIOD - 1U) * (100UL - speedPercent)
              / 100UL;

    DL_TimerA_setCaptureCompareValue(FAN_PWM_INST,
        ccValue, DL_TIMERA_CAPTURE_COMPARE_0_INDEX);
}

/* ---- Buzzer helpers (PB8, active low) ---- */

/**
 * @brief  Initialize buzzer GPIO (PB8) as digital output, initially off.
 *
 * PB8 = IOMUX PINCM25. The buzzer is active LOW, so the pin is set
 * HIGH during init to keep the buzzer silent.
 */
void Board_Buzzer_Init(void)
{
    DL_GPIO_initDigitalOutput(BUZZER_IOMUX);
    DL_GPIO_setPins(BUZZER_PORT, BUZZER_PIN); /* HIGH = off */
}

/**
 * @brief  Turn buzzer on — drive PB8 LOW.
 */
void Board_Buzzer_On(void)
{
    DL_GPIO_clearPins(BUZZER_PORT, BUZZER_PIN);
}

/**
 * @brief  Turn buzzer off — drive PB8 HIGH.
 */
void Board_Buzzer_Off(void)
{
    DL_GPIO_setPins(BUZZER_PORT, BUZZER_PIN);
}
