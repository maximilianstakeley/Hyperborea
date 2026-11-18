#ifdef HB_BOARD_REV16
#include "hb_rev16_control.h"
#include "hb_board_pinmap_rev16.h"

volatile uint32_t hb_last_reset_flags;
volatile uint32_t hb_last_afe_faults;
void hb_rev16_acknowledge(void) {
    HAL_GPIO_WritePin(GPIOD, GPIO_PIN_5, GPIO_PIN_SET);
    HAL_GPIO_WritePin(GPIOD, GPIO_PIN_5, GPIO_PIN_RESET);
}
static void outputs(GPIO_TypeDef *port, uint16_t pins) {
    GPIO_InitTypeDef cfg = {0};
    cfg.Pin = pins;
    cfg.Mode = GPIO_MODE_OUTPUT_PP;
    cfg.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(port, &cfg);
}
static void inputs(GPIO_TypeDef *port, uint16_t pins) {
    GPIO_InitTypeDef cfg = {0};
    cfg.Pin = pins;
    cfg.Mode = GPIO_MODE_INPUT;
    HAL_GPIO_Init(port, &cfg);
}
void hb_rev16_bias(bool enabled) {
    if (enabled) {
        HAL_GPIO_WritePin(GPIOE, GPIO_PIN_10, GPIO_PIN_SET);
        HAL_GPIO_WritePin(GPIOE, GPIO_PIN_10, GPIO_PIN_RESET);
    }
    HAL_GPIO_WritePin(GPIOD, GPIO_PIN_6, enabled ? GPIO_PIN_SET : GPIO_PIN_RESET);
}
void hb_rev16_heartbeat(void) {
    HAL_GPIO_WritePin(GPIOE, GPIO_PIN_9, GPIO_PIN_SET);
    HAL_GPIO_WritePin(GPIOE, GPIO_PIN_9, GPIO_PIN_RESET);
}
bool hb_rev16_prepare(void) {
    __HAL_RCC_GPIOC_CLK_ENABLE();
    __HAL_RCC_GPIOD_CLK_ENABLE();
    __HAL_RCC_GPIOE_CLK_ENABLE();
    HAL_GPIO_WritePin(GPIOE, GPIO_PIN_11, GPIO_PIN_SET);
    HAL_GPIO_WritePin(GPIOE, GPIO_PIN_9 | GPIO_PIN_10, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(GPIOD, GPIO_PIN_5 | GPIO_PIN_6, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(GPIOC, GPIO_PIN_4, GPIO_PIN_RESET);
    outputs(GPIOC, GPIO_PIN_4);
    outputs(GPIOE, GPIO_PIN_9 | GPIO_PIN_10 | GPIO_PIN_11);
    outputs(GPIOD, GPIO_PIN_5 | GPIO_PIN_6);
    inputs(GPIOD, 0x1fu);
    inputs(GPIOC, GPIO_PIN_6 | GPIO_PIN_7);
    inputs(GPIOE, GPIO_PIN_1);
    hb_last_reset_flags = RCC->RSR;
    hb_last_afe_faults = GPIOD->IDR & 0x1fu;
    /* Retain hardware flags for service inspection. Acknowledge only explicitly. */
    if (!HB_BOARD_AFE_RAILS_GOOD()) return false;
    uint32_t kernel = HAL_RCCEx_GetPeriphCLKFreq(RCC_PERIPHCLK_SPI4);
    uint32_t divisor = 2u;
    unsigned setting = 0;
    while (divisor < 256u && kernel / divisor > 4000000u) {
        divisor *= 2u;
        ++setting;
    }
    if (kernel / divisor < 3200000u || kernel / divisor > 4000000u) return false;
    hspi4.Init.BaudRatePrescaler = setting << SPI_CFG1_MBR_Pos;
    hspi4.Init.CLKPolarity = SPI_POLARITY_LOW;
    hspi4.Init.CLKPhase = SPI_PHASE_2EDGE;
    if (HAL_SPI_Init(&hspi4) != HAL_OK) return false;
    HAL_GPIO_WritePin(GPIOE, GPIO_PIN_11, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(GPIOC, GPIO_PIN_4, GPIO_PIN_SET);
    hb_rev16_heartbeat();
    return true;
}
#endif
