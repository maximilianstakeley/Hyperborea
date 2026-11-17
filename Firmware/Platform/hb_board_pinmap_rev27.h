/*
 * Rev27 U50 STM32H743ZIT6 LQFP144 pin map.
 * Net names are taken from the actual Rev27 PCB U50 pad assignments;
 * package pad-to-port names and AFs are checked against ST DS12110, Table 9.
 * This header assumes the PCB footprint and assembly orientation are unchanged.
 */
#ifndef HB_BOARD_PINMAP_REV27_H
#define HB_BOARD_PINMAP_REV27_H
#include "main.h"
#include "spi.h"
#define HB_BOARD_PINMAP_APPROVED 1
#define HB_ADS1299_SPI_HANDLE hspi4

/* Implement these in the board project. VCAP1 is not routed to an MCU ADC in
   the Rev27 schematic; do not claim readiness without a characterized method. */
extern bool hb_board_afe_rails_good(void);
extern bool hb_board_afe_vcap1_ready(void);
#define HB_BOARD_AFE_RAILS_GOOD() hb_board_afe_rails_good()
#define HB_BOARD_AFE_VCAP1_READY() hb_board_afe_vcap1_ready()

/* ADS1299 SPI4: LQFP144 pad 1=PE2/SPI4_SCK AF5; pad 4=PE5/MISO AF5;
   pad 5=PE6/MOSI AF5. Pad 3=PE4 is GPIO chip-select. */
#define HB_AFE_CS_GPIO_Port GPIOE
#define HB_AFE_CS_Pin GPIO_PIN_4
#define HB_AFE_DRDY_GPIO_Port GPIOA
#define HB_AFE_DRDY_Pin GPIO_PIN_0       /* U50 pad 34, PA0 */
#define HB_AFE_START_GPIO_Port GPIOF
#define HB_AFE_START_Pin GPIO_PIN_11    /* U50 pad 49, PF11 */
#define HB_AFE_RESET_GPIO_Port GPIOF
#define HB_AFE_RESET_Pin GPIO_PIN_12    /* U50 pad 50, PF12 */
#define HB_AFE_PWDN_GPIO_Port GPIOF
#define HB_AFE_PWDN_Pin GPIO_PIN_13     /* U50 pad 53, PF13 */

/* The AFE CLKSEL net is hard-strapped low for the 2.048 MHz external clock.
   It is not connected to an STM32 GPIO. */

#endif
