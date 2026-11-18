#ifndef HB_BOARD_PINMAP_REV16_H
#define HB_BOARD_PINMAP_REV16_H
#include "main.h"
#include "spi.h"
#define HB_BOARD_PINMAP_REV16_APPROVED 1
#define HB_ADS1299_SPI_HANDLE hspi4

/* U1: existing ADS1299. */
#define HB_AFE1_CS_GPIO_Port GPIOE
#define HB_AFE1_CS_Pin GPIO_PIN_4       /* U50 pad 3, PE4 */
#define HB_AFE1_DRDY_GPIO_Port GPIOA
#define HB_AFE1_DRDY_Pin GPIO_PIN_0     /* U50 pad 34, PA0 */
#define HB_AFE1_RESET_GPIO_Port GPIOF
#define HB_AFE1_RESET_Pin GPIO_PIN_12   /* U50 pad 50, PF12 */
#define HB_AFE1_PWDN_GPIO_Port GPIOF
#define HB_AFE1_PWDN_Pin GPIO_PIN_13    /* U50 pad 53, PF13 */

#define HB_AFE2_CS_GPIO_Port GPIOE
#define HB_AFE2_CS_Pin GPIO_PIN_3
#define HB_AFE2_RESET_GPIO_Port GPIOF
#define HB_AFE2_RESET_Pin GPIO_PIN_12
#define HB_AFE2_PWDN_GPIO_Port GPIOF
#define HB_AFE2_PWDN_Pin GPIO_PIN_13
#define HB_AFE2_DRDY_GPIO_Port GPIOA
#define HB_AFE2_DRDY_Pin GPIO_PIN_1
#define HB_BOARD_AFE_RAILS_GOOD() (HAL_GPIO_ReadPin(GPIOE, GPIO_PIN_1) == GPIO_PIN_SET)
/* Shared SPI4: PE2=SCK (pad 1), PE5=MISO (pad 4), PE6=MOSI (pad 5). */
#define HB_AFE_SPI_GPIO_Port GPIOE
#define HB_AFE_SPI_SCK_Pin GPIO_PIN_2
#define HB_AFE_SPI_MISO_Pin GPIO_PIN_5
#define HB_AFE_SPI_MOSI_Pin GPIO_PIN_6

/* Shared START: one physical GPIO drives both START pins. */
#define HB_AFE_START_GPIO_Port GPIOF
#define HB_AFE_START_Pin GPIO_PIN_11  /* U50 pad 49, PF11 */

/* X51 supplies common external 2.048 MHz to both ADS1299 CLK pins.
 * CLKSEL is hard-strapped low on each ADC for external clock mode.
 */
#define HB_AFE_EXTERNAL_CLOCK_HZ 2048000UL
#define HB_AFE_CHANNEL_COUNT 16U
#endif
