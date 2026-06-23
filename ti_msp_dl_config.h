/*
 * Copyright (c) 2023, Texas Instruments Incorporated - http://www.ti.com
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
 *  ============ ti_msp_dl_config.h =============
 *  Configured MSPM0 DriverLib module declarations
 *
 *  DO NOT EDIT - This file is generated for the MSPM0G350X
 *  by the SysConfig tool.
 */
#ifndef ti_msp_dl_config_h
#define ti_msp_dl_config_h

#define CONFIG_MSPM0G350X
#define CONFIG_MSPM0G3507

#if defined(__ti_version__) || defined(__TI_COMPILER_VERSION__)
#define SYSCONFIG_WEAK __attribute__((weak))
#elif defined(__IAR_SYSTEMS_ICC__)
#define SYSCONFIG_WEAK __weak
#elif defined(__GNUC__)
#define SYSCONFIG_WEAK __attribute__((weak))
#endif

#include <ti/devices/msp/msp.h>
#include <ti/driverlib/driverlib.h>
#include <ti/driverlib/m0p/dl_core.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 *  ======== SYSCFG_DL_init ========
 *  Perform all required MSP DL initialization
 *
 *  This function should be called once at a point before any use of
 *  MSP DL.
 */


/* clang-format off */

#define POWER_STARTUP_DELAY                                                (16)


#define CPUCLK_FREQ                                                     32000000



/* Defines for PWM_FAN */
#define PWM_FAN_INST                                                       TIMA0
#define PWM_FAN_INST_IRQHandler                                 TIMA0_IRQHandler
#define PWM_FAN_INST_INT_IRQN                                   (TIMA0_INT_IRQn)
#define PWM_FAN_INST_CLK_FREQ                                           32000000
/* GPIO defines for channel 0 */
#define GPIO_PWM_FAN_C0_PORT                                               GPIOA
#define GPIO_PWM_FAN_C0_PIN                                        DL_GPIO_PIN_8
#define GPIO_PWM_FAN_C0_IOMUX                                    (IOMUX_PINCM19)
#define GPIO_PWM_FAN_C0_IOMUX_FUNC                   IOMUX_PINCM19_PF_TIMA0_CCP0
#define GPIO_PWM_FAN_C0_IDX                                  DL_TIMER_CC_0_INDEX
/* GPIO defines for channel 1 */
#define GPIO_PWM_FAN_C1_PORT                                               GPIOA
#define GPIO_PWM_FAN_C1_PIN                                        DL_GPIO_PIN_1
#define GPIO_PWM_FAN_C1_IOMUX                                     (IOMUX_PINCM2)
#define GPIO_PWM_FAN_C1_IOMUX_FUNC                    IOMUX_PINCM2_PF_TIMA0_CCP1
#define GPIO_PWM_FAN_C1_IDX                                  DL_TIMER_CC_1_INDEX




/* Defines for I2C1 */
#define I2C1_INST                                                           I2C1
#define I2C1_INST_IRQHandler                                     I2C1_IRQHandler
#define I2C1_INST_INT_IRQN                                         I2C1_INT_IRQn
#define I2C1_BUS_SPEED_HZ                                                 400000
#define GPIO_I2C1_SDA_PORT                                                 GPIOB
#define GPIO_I2C1_SDA_PIN                                          DL_GPIO_PIN_3
#define GPIO_I2C1_IOMUX_SDA                                      (IOMUX_PINCM16)
#define GPIO_I2C1_IOMUX_SDA_FUNC                       IOMUX_PINCM16_PF_I2C1_SDA
#define GPIO_I2C1_SCL_PORT                                                 GPIOB
#define GPIO_I2C1_SCL_PIN                                          DL_GPIO_PIN_2
#define GPIO_I2C1_IOMUX_SCL                                      (IOMUX_PINCM15)
#define GPIO_I2C1_IOMUX_SCL_FUNC                       IOMUX_PINCM15_PF_I2C1_SCL


/* Defines for UART_0_XDSDEBUG */
#define UART_0_XDSDEBUG_INST                                               UART0
#define UART_0_XDSDEBUG_INST_FREQUENCY                                  32000000
#define UART_0_XDSDEBUG_INST_IRQHandler                         UART0_IRQHandler
#define UART_0_XDSDEBUG_INST_INT_IRQN                             UART0_INT_IRQn
#define GPIO_UART_0_XDSDEBUG_RX_PORT                                       GPIOA
#define GPIO_UART_0_XDSDEBUG_TX_PORT                                       GPIOA
#define GPIO_UART_0_XDSDEBUG_RX_PIN                               DL_GPIO_PIN_11
#define GPIO_UART_0_XDSDEBUG_TX_PIN                               DL_GPIO_PIN_10
#define GPIO_UART_0_XDSDEBUG_IOMUX_RX                            (IOMUX_PINCM22)
#define GPIO_UART_0_XDSDEBUG_IOMUX_TX                            (IOMUX_PINCM21)
#define GPIO_UART_0_XDSDEBUG_IOMUX_RX_FUNC               IOMUX_PINCM22_PF_UART0_RX
#define GPIO_UART_0_XDSDEBUG_IOMUX_TX_FUNC               IOMUX_PINCM21_PF_UART0_TX
#define UART_0_XDSDEBUG_BAUD_RATE                                       (115200)
#define UART_0_XDSDEBUG_IBRD_32_MHZ_115200_BAUD                             (17)
#define UART_0_XDSDEBUG_FBRD_32_MHZ_115200_BAUD                             (23)
/* Defines for UART_1_ESP8266 */
#define UART_1_ESP8266_INST                                                UART1
#define UART_1_ESP8266_INST_FREQUENCY                                   32000000
#define UART_1_ESP8266_INST_IRQHandler                          UART1_IRQHandler
#define UART_1_ESP8266_INST_INT_IRQN                              UART1_INT_IRQn
#define GPIO_UART_1_ESP8266_RX_PORT                                        GPIOB
#define GPIO_UART_1_ESP8266_TX_PORT                                        GPIOB
#define GPIO_UART_1_ESP8266_RX_PIN                                 DL_GPIO_PIN_7
#define GPIO_UART_1_ESP8266_TX_PIN                                 DL_GPIO_PIN_6
#define GPIO_UART_1_ESP8266_IOMUX_RX                             (IOMUX_PINCM24)
#define GPIO_UART_1_ESP8266_IOMUX_TX                             (IOMUX_PINCM23)
#define GPIO_UART_1_ESP8266_IOMUX_RX_FUNC               IOMUX_PINCM24_PF_UART1_RX
#define GPIO_UART_1_ESP8266_IOMUX_TX_FUNC               IOMUX_PINCM23_PF_UART1_TX
#define UART_1_ESP8266_BAUD_RATE                                        (115200)
#define UART_1_ESP8266_IBRD_32_MHZ_115200_BAUD                              (17)
#define UART_1_ESP8266_FBRD_32_MHZ_115200_BAUD                              (23)





/* Defines for ADC12_0 */
#define ADC12_0_INST                                                        ADC0
#define ADC12_0_INST_IRQHandler                                  ADC0_IRQHandler
#define ADC12_0_INST_INT_IRQN                                    (ADC0_INT_IRQn)
#define ADC12_0_ADCMEM_0                                      DL_ADC12_MEM_IDX_0
#define ADC12_0_ADCMEM_0_REF                     DL_ADC12_REFERENCE_VOLTAGE_VDDA
#define ADC12_0_ADCMEM_0_REF_VOLTAGE_V                                       3.3
#define GPIO_ADC12_0_C2_PORT                                               GPIOA
#define GPIO_ADC12_0_C2_PIN                                       DL_GPIO_PIN_25



/* Port definition for Pin Group GPIO_LED */
#define GPIO_LED_PORT                                                    (GPIOA)

/* Defines for USER_LED: GPIOA.0 with pinCMx 1 on package pin 33 */
#define GPIO_LED_USER_LED_PIN                                    (DL_GPIO_PIN_0)
#define GPIO_LED_USER_LED_IOMUX                                   (IOMUX_PINCM1)
/* Port definition for Pin Group GPIO_SENSOR_EN */
#define GPIO_SENSOR_EN_PORT                                              (GPIOB)

/* Defines for HDC2010_EN: GPIOB.24 with pinCMx 52 on package pin 23 */
#define GPIO_SENSOR_EN_HDC2010_EN_PIN                           (DL_GPIO_PIN_24)
#define GPIO_SENSOR_EN_HDC2010_EN_IOMUX                          (IOMUX_PINCM52)
/* Defines for DRV5055_EN: GPIOB.15 with pinCMx 32 on package pin 3 */
#define GPIO_SENSOR_EN_DRV5055_EN_PIN                           (DL_GPIO_PIN_15)
#define GPIO_SENSOR_EN_DRV5055_EN_IOMUX                          (IOMUX_PINCM32)

/* clang-format on */

void SYSCFG_DL_init(void);
void SYSCFG_DL_initPower(void);
void SYSCFG_DL_GPIO_init(void);
void SYSCFG_DL_SYSCTL_init(void);
void SYSCFG_DL_PWM_FAN_init(void);
void SYSCFG_DL_I2C1_init(void);
void SYSCFG_DL_UART_0_XDSDEBUG_init(void);
void SYSCFG_DL_UART_1_ESP8266_init(void);
void SYSCFG_DL_ADC12_0_init(void);


bool SYSCFG_DL_saveConfiguration(void);
bool SYSCFG_DL_restoreConfiguration(void);

#ifdef __cplusplus
}
#endif

#endif /* ti_msp_dl_config_h */
