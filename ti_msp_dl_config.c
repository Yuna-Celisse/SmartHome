/*
 * Copyright (c) 2023, Texas Instruments Incorporated
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * *  Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * *  Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * *  Neither the name of Texas Instruments Incorporated nor the names of
 *    its contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
 * EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 *  ============ ti_msp_dl_config.c =============
 *  Configured MSPM0 DriverLib module definitions
 *
 *  DO NOT EDIT - This file is generated for the MSPM0G350X
 *  by the SysConfig tool.
 */

#include "ti_msp_dl_config.h"

DL_TimerA_backupConfig gPWM_FANBackup;

/*
 *  ======== SYSCFG_DL_init ========
 *  Perform any initialization needed before using any board APIs
 */
SYSCONFIG_WEAK void SYSCFG_DL_init(void)
{
    SYSCFG_DL_initPower();
    SYSCFG_DL_GPIO_init();
    /* Module-Specific Initializations*/
    SYSCFG_DL_SYSCTL_init();
    SYSCFG_DL_PWM_FAN_init();
    SYSCFG_DL_I2C1_init();
    SYSCFG_DL_UART_0_XDSDEBUG_init();
    SYSCFG_DL_UART_1_ESP8266_init();
    SYSCFG_DL_ADC12_0_init();
    /* Ensure backup structures have no valid state */
	gPWM_FANBackup.backupRdy 	= false;


}
/*
 * User should take care to save and restore register configuration in application.
 * See Retention Configuration section for more details.
 */
SYSCONFIG_WEAK bool SYSCFG_DL_saveConfiguration(void)
{
    bool retStatus = true;

	retStatus &= DL_TimerA_saveConfiguration(PWM_FAN_INST, &gPWM_FANBackup);

    return retStatus;
}


SYSCONFIG_WEAK bool SYSCFG_DL_restoreConfiguration(void)
{
    bool retStatus = true;

	retStatus &= DL_TimerA_restoreConfiguration(PWM_FAN_INST, &gPWM_FANBackup, false);

    return retStatus;
}

SYSCONFIG_WEAK void SYSCFG_DL_initPower(void)
{
    DL_GPIO_reset(GPIOA);
    DL_GPIO_reset(GPIOB);
    DL_TimerA_reset(PWM_FAN_INST);
    DL_I2C_reset(I2C1_INST);
    DL_UART_Main_reset(UART_0_XDSDEBUG_INST);
    DL_UART_Main_reset(UART_1_ESP8266_INST);
    DL_ADC12_reset(ADC12_0_INST);

    DL_GPIO_enablePower(GPIOA);
    DL_GPIO_enablePower(GPIOB);
    DL_TimerA_enablePower(PWM_FAN_INST);
    DL_I2C_enablePower(I2C1_INST);
    DL_UART_Main_enablePower(UART_0_XDSDEBUG_INST);
    DL_UART_Main_enablePower(UART_1_ESP8266_INST);
    DL_ADC12_enablePower(ADC12_0_INST);
    delay_cycles(POWER_STARTUP_DELAY);
}

SYSCONFIG_WEAK void SYSCFG_DL_GPIO_init(void)
{

    DL_GPIO_initPeripheralOutputFunction(GPIO_PWM_FAN_C0_IOMUX,GPIO_PWM_FAN_C0_IOMUX_FUNC);
    DL_GPIO_enableOutput(GPIO_PWM_FAN_C0_PORT, GPIO_PWM_FAN_C0_PIN);
    DL_GPIO_initPeripheralOutputFunction(GPIO_PWM_FAN_C1_IOMUX,GPIO_PWM_FAN_C1_IOMUX_FUNC);
    DL_GPIO_enableOutput(GPIO_PWM_FAN_C1_PORT, GPIO_PWM_FAN_C1_PIN);

    DL_GPIO_initPeripheralInputFunctionFeatures(GPIO_I2C1_IOMUX_SDA,
        GPIO_I2C1_IOMUX_SDA_FUNC, DL_GPIO_INVERSION_DISABLE,
        DL_GPIO_RESISTOR_NONE, DL_GPIO_HYSTERESIS_DISABLE,
        DL_GPIO_WAKEUP_DISABLE);
    DL_GPIO_initPeripheralInputFunctionFeatures(GPIO_I2C1_IOMUX_SCL,
        GPIO_I2C1_IOMUX_SCL_FUNC, DL_GPIO_INVERSION_DISABLE,
        DL_GPIO_RESISTOR_NONE, DL_GPIO_HYSTERESIS_DISABLE,
        DL_GPIO_WAKEUP_DISABLE);
    DL_GPIO_enableHiZ(GPIO_I2C1_IOMUX_SDA);
    DL_GPIO_enableHiZ(GPIO_I2C1_IOMUX_SCL);

    DL_GPIO_initPeripheralOutputFunction(
        GPIO_UART_0_XDSDEBUG_IOMUX_TX, GPIO_UART_0_XDSDEBUG_IOMUX_TX_FUNC);
    DL_GPIO_initPeripheralInputFunction(
        GPIO_UART_0_XDSDEBUG_IOMUX_RX, GPIO_UART_0_XDSDEBUG_IOMUX_RX_FUNC);
    DL_GPIO_initPeripheralOutputFunction(
        GPIO_UART_1_ESP8266_IOMUX_TX, GPIO_UART_1_ESP8266_IOMUX_TX_FUNC);
    DL_GPIO_initPeripheralInputFunction(
        GPIO_UART_1_ESP8266_IOMUX_RX, GPIO_UART_1_ESP8266_IOMUX_RX_FUNC);

    DL_GPIO_initDigitalOutput(GPIO_LED_USER_LED_IOMUX);

    DL_GPIO_initDigitalOutput(GPIO_SENSOR_EN_HDC2010_EN_IOMUX);

    DL_GPIO_initDigitalOutput(GPIO_SENSOR_EN_DRV5055_EN_IOMUX);

    DL_GPIO_setPins(GPIO_LED_PORT, GPIO_LED_USER_LED_PIN);
    DL_GPIO_enableOutput(GPIO_LED_PORT, GPIO_LED_USER_LED_PIN);
    DL_GPIO_clearPins(GPIO_SENSOR_EN_PORT, GPIO_SENSOR_EN_HDC2010_EN_PIN |
		GPIO_SENSOR_EN_DRV5055_EN_PIN);
    DL_GPIO_enableOutput(GPIO_SENSOR_EN_PORT, GPIO_SENSOR_EN_HDC2010_EN_PIN |
		GPIO_SENSOR_EN_DRV5055_EN_PIN);

}


SYSCONFIG_WEAK void SYSCFG_DL_SYSCTL_init(void)
{

	//Low Power Mode is configured to be SLEEP0
    DL_SYSCTL_setBORThreshold(DL_SYSCTL_BOR_THRESHOLD_LEVEL_0);

    DL_SYSCTL_setSYSOSCFreq(DL_SYSCTL_SYSOSC_FREQ_BASE);
    /* Set default configuration */
    DL_SYSCTL_disableHFXT();
    DL_SYSCTL_disableSYSPLL();
    DL_SYSCTL_setULPCLKDivider(DL_SYSCTL_ULPCLK_DIV_1);
    DL_SYSCTL_setMCLKDivider(DL_SYSCTL_MCLK_DIVIDER_DISABLE);

}


/*
 * Timer clock configuration to be sourced by  / 1 (32000000 Hz)
 * timerClkFreq = (timerClkSrc / (timerClkDivRatio * (timerClkPrescale + 1)))
 *   32000000 Hz = 32000000 Hz / (1 * (0 + 1))
 */
static const DL_TimerA_ClockConfig gPWM_FANClockConfig = {
    .clockSel = DL_TIMER_CLOCK_BUSCLK,
    .divideRatio = DL_TIMER_CLOCK_DIVIDE_1,
    .prescale = 0U
};

static const DL_TimerA_PWMConfig gPWM_FANConfig = {
    .pwmMode = DL_TIMER_PWM_MODE_EDGE_ALIGN,
    .period = 1280,
    .isTimerWithFourCC = true,
    .startTimer = DL_TIMER_STOP,
};

SYSCONFIG_WEAK void SYSCFG_DL_PWM_FAN_init(void) {

    DL_TimerA_setClockConfig(
        PWM_FAN_INST, (DL_TimerA_ClockConfig *) &gPWM_FANClockConfig);

    DL_TimerA_initPWMMode(
        PWM_FAN_INST, (DL_TimerA_PWMConfig *) &gPWM_FANConfig);

    // Set Counter control to the smallest CC index being used
    DL_TimerA_setCounterControl(PWM_FAN_INST,DL_TIMER_CZC_CCCTL0_ZCOND,DL_TIMER_CAC_CCCTL0_ACOND,DL_TIMER_CLC_CCCTL0_LCOND);

    DL_TimerA_setCaptureCompareOutCtl(PWM_FAN_INST, DL_TIMER_CC_OCTL_INIT_VAL_LOW,
		DL_TIMER_CC_OCTL_INV_OUT_DISABLED, DL_TIMER_CC_OCTL_SRC_FUNCVAL,
		DL_TIMERA_CAPTURE_COMPARE_0_INDEX);

    DL_TimerA_setCaptCompUpdateMethod(PWM_FAN_INST, DL_TIMER_CC_UPDATE_METHOD_IMMEDIATE, DL_TIMERA_CAPTURE_COMPARE_0_INDEX);
    DL_TimerA_setCaptureCompareValue(PWM_FAN_INST, 1280, DL_TIMER_CC_0_INDEX);

    DL_TimerA_setCaptureCompareOutCtl(PWM_FAN_INST, DL_TIMER_CC_OCTL_INIT_VAL_LOW,
		DL_TIMER_CC_OCTL_INV_OUT_DISABLED, DL_TIMER_CC_OCTL_SRC_FUNCVAL,
		DL_TIMERA_CAPTURE_COMPARE_1_INDEX);

    DL_TimerA_setCaptCompUpdateMethod(PWM_FAN_INST, DL_TIMER_CC_UPDATE_METHOD_IMMEDIATE, DL_TIMERA_CAPTURE_COMPARE_1_INDEX);
    DL_TimerA_setCaptureCompareValue(PWM_FAN_INST, 1280, DL_TIMER_CC_1_INDEX);

    DL_TimerA_enableClock(PWM_FAN_INST);


    
    DL_TimerA_setCCPDirection(PWM_FAN_INST , DL_TIMER_CC0_OUTPUT | DL_TIMER_CC1_OUTPUT );


}


static const DL_I2C_ClockConfig gI2C1ClockConfig = {
    .clockSel = DL_I2C_CLOCK_BUSCLK,
    .divideRatio = DL_I2C_CLOCK_DIVIDE_1,
};

SYSCONFIG_WEAK void SYSCFG_DL_I2C1_init(void) {

    DL_I2C_setClockConfig(I2C1_INST,
        (DL_I2C_ClockConfig *) &gI2C1ClockConfig);
    DL_I2C_disableAnalogGlitchFilter(I2C1_INST);

    /* Configure Controller Mode */
    DL_I2C_resetControllerTransfer(I2C1_INST);
    /* Set frequency to 400000 Hz*/
    DL_I2C_setTimerPeriod(I2C1_INST, 7);
    DL_I2C_setControllerTXFIFOThreshold(I2C1_INST, DL_I2C_TX_FIFO_LEVEL_EMPTY);
    DL_I2C_setControllerRXFIFOThreshold(I2C1_INST, DL_I2C_RX_FIFO_LEVEL_BYTES_1);
    DL_I2C_enableControllerClockStretching(I2C1_INST);


    /* Enable module */
    DL_I2C_enableController(I2C1_INST);


}

static const DL_UART_Main_ClockConfig gUART_0_XDSDEBUGClockConfig = {
    .clockSel    = DL_UART_MAIN_CLOCK_BUSCLK,
    .divideRatio = DL_UART_MAIN_CLOCK_DIVIDE_RATIO_1
};

static const DL_UART_Main_Config gUART_0_XDSDEBUGConfig = {
    .mode        = DL_UART_MAIN_MODE_NORMAL,
    .direction   = DL_UART_MAIN_DIRECTION_TX_RX,
    .flowControl = DL_UART_MAIN_FLOW_CONTROL_NONE,
    .parity      = DL_UART_MAIN_PARITY_NONE,
    .wordLength  = DL_UART_MAIN_WORD_LENGTH_8_BITS,
    .stopBits    = DL_UART_MAIN_STOP_BITS_ONE
};

SYSCONFIG_WEAK void SYSCFG_DL_UART_0_XDSDEBUG_init(void)
{
    DL_UART_Main_setClockConfig(UART_0_XDSDEBUG_INST, (DL_UART_Main_ClockConfig *) &gUART_0_XDSDEBUGClockConfig);

    DL_UART_Main_init(UART_0_XDSDEBUG_INST, (DL_UART_Main_Config *) &gUART_0_XDSDEBUGConfig);
    /*
     * Configure baud rate by setting oversampling and baud rate divisors.
     *  Target baud rate: 115200
     *  Actual baud rate: 115211.52
     */
    DL_UART_Main_setOversampling(UART_0_XDSDEBUG_INST, DL_UART_OVERSAMPLING_RATE_16X);
    DL_UART_Main_setBaudRateDivisor(UART_0_XDSDEBUG_INST, UART_0_XDSDEBUG_IBRD_32_MHZ_115200_BAUD, UART_0_XDSDEBUG_FBRD_32_MHZ_115200_BAUD);



    DL_UART_Main_enable(UART_0_XDSDEBUG_INST);
}
static const DL_UART_Main_ClockConfig gUART_1_ESP8266ClockConfig = {
    .clockSel    = DL_UART_MAIN_CLOCK_BUSCLK,
    .divideRatio = DL_UART_MAIN_CLOCK_DIVIDE_RATIO_1
};

static const DL_UART_Main_Config gUART_1_ESP8266Config = {
    .mode        = DL_UART_MAIN_MODE_NORMAL,
    .direction   = DL_UART_MAIN_DIRECTION_TX_RX,
    .flowControl = DL_UART_MAIN_FLOW_CONTROL_NONE,
    .parity      = DL_UART_MAIN_PARITY_NONE,
    .wordLength  = DL_UART_MAIN_WORD_LENGTH_8_BITS,
    .stopBits    = DL_UART_MAIN_STOP_BITS_ONE
};

SYSCONFIG_WEAK void SYSCFG_DL_UART_1_ESP8266_init(void)
{
    DL_UART_Main_setClockConfig(UART_1_ESP8266_INST, (DL_UART_Main_ClockConfig *) &gUART_1_ESP8266ClockConfig);

    DL_UART_Main_init(UART_1_ESP8266_INST, (DL_UART_Main_Config *) &gUART_1_ESP8266Config);
    /*
     * Configure baud rate by setting oversampling and baud rate divisors.
     *  Target baud rate: 115200
     *  Actual baud rate: 115211.52
     */
    DL_UART_Main_setOversampling(UART_1_ESP8266_INST, DL_UART_OVERSAMPLING_RATE_16X);
    DL_UART_Main_setBaudRateDivisor(UART_1_ESP8266_INST, UART_1_ESP8266_IBRD_32_MHZ_115200_BAUD, UART_1_ESP8266_FBRD_32_MHZ_115200_BAUD);



    DL_UART_Main_enable(UART_1_ESP8266_INST);
}

/* ADC12_0 Initialization */
static const DL_ADC12_ClockConfig gADC12_0ClockConfig = {
    .clockSel       = DL_ADC12_CLOCK_SYSOSC,
    .divideRatio    = DL_ADC12_CLOCK_DIVIDE_1,
    .freqRange      = DL_ADC12_CLOCK_FREQ_RANGE_24_TO_32,
};
SYSCONFIG_WEAK void SYSCFG_DL_ADC12_0_init(void)
{
    DL_ADC12_setClockConfig(ADC12_0_INST, (DL_ADC12_ClockConfig *) &gADC12_0ClockConfig);
    DL_ADC12_configConversionMem(ADC12_0_INST, ADC12_0_ADCMEM_0,
        DL_ADC12_INPUT_CHAN_2, DL_ADC12_REFERENCE_VOLTAGE_VDDA, DL_ADC12_SAMPLE_TIMER_SOURCE_SCOMP0, DL_ADC12_AVERAGING_MODE_DISABLED,
        DL_ADC12_BURN_OUT_SOURCE_DISABLED, DL_ADC12_TRIGGER_MODE_AUTO_NEXT, DL_ADC12_WINDOWS_COMP_MODE_DISABLED);
    DL_ADC12_setPowerDownMode(ADC12_0_INST,DL_ADC12_POWER_DOWN_MODE_MANUAL);
    DL_ADC12_setSampleTime0(ADC12_0_INST,2);
    DL_ADC12_enableConversions(ADC12_0_INST);
}

