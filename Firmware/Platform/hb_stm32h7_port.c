/*
 * STM32H743 HAL adapter for the Rev30 dual-ADS1299 control allocation.
 * Requires the project's CubeH7 HAL startup and GPIO/SPI initialization.
 * The second ADC is not yet physically present in the PCB design; this adapter
 * implements the planned Rev30 GPIO assignment for the next board revision.
 */
#include "hb_board_pinmap_rev30.h"
#include "hb/hal_port.h"

/* Supplied by the CubeMX project using a free-running 1 MHz TIM2 counter. */
extern uint32_t hb_board_micros(void);

#ifndef HB_BOARD_PINMAP_REV30_APPROVED
#error "Review the exact Rev30 KiCad netlist and STM32H743 package before enabling this adapter"
#endif
#ifndef HB_BOARD_AFE_RAILS_GOOD
#error "Implement HB_BOARD_AFE_RAILS_GOOD() from verified rail monitors"
#endif
#ifndef HB_BOARD_AFE_VCAP1_READY
#error "Implement HB_BOARD_AFE_VCAP1_READY() from characterized startup timing/measurement"
#endif

typedef struct {
    GPIO_TypeDef *cs_port; uint16_t cs_pin;
    GPIO_TypeDef *drdy_port; uint16_t drdy_pin;
    GPIO_TypeDef *reset_port; uint16_t reset_pin;
    GPIO_TypeDef *pwdn_port; uint16_t pwdn_pin;
} hb_afe_gpio_context_t;

static hb_afe_gpio_context_t afe1_gpio = {
    HB_AFE1_CS_GPIO_Port, HB_AFE1_CS_Pin,
    HB_AFE1_DRDY_GPIO_Port, HB_AFE1_DRDY_Pin,
    HB_AFE1_RESET_GPIO_Port, HB_AFE1_RESET_Pin,
    HB_AFE1_PWDN_GPIO_Port, HB_AFE1_PWDN_Pin
};
static hb_afe_gpio_context_t afe2_gpio = {
    HB_AFE2_CS_GPIO_Port, HB_AFE2_CS_Pin,
    HB_AFE2_DRDY_GPIO_Port, HB_AFE2_DRDY_Pin,
    HB_AFE2_RESET_GPIO_Port, HB_AFE2_RESET_Pin,
    HB_AFE2_PWDN_GPIO_Port, HB_AFE2_PWDN_Pin
};
static uint8_t shared_start_context;

static bool hal_spi_transfer(void *ctx, const uint8_t *tx, uint8_t *rx,
                             size_t length, uint32_t timeout_ms) {
    (void)ctx;
    uint8_t discard[64] = {0};
    if (length == 0U || length > sizeof discard) return false;
    uint8_t *rx_buffer = (rx != NULL) ? rx : discard;
    return HAL_SPI_TransmitReceive(&HB_ADS1299_SPI_HANDLE,
        (uint8_t *)tx, rx_buffer, (uint16_t)length, timeout_ms) == HAL_OK;
}
static void hal_chip_select(void *ctx, bool asserted) {
    hb_afe_gpio_context_t *g = (hb_afe_gpio_context_t *)ctx;
    HAL_GPIO_WritePin(g->cs_port, g->cs_pin, asserted ? GPIO_PIN_RESET : GPIO_PIN_SET);
}
static void hal_reset(void *ctx, bool asserted) {
    hb_afe_gpio_context_t *g = (hb_afe_gpio_context_t *)ctx;
    HAL_GPIO_WritePin(g->reset_port, g->reset_pin, asserted ? GPIO_PIN_RESET : GPIO_PIN_SET);
}
static void hal_power_down(void *ctx, bool asserted) {
    hb_afe_gpio_context_t *g = (hb_afe_gpio_context_t *)ctx;
    HAL_GPIO_WritePin(g->pwdn_port, g->pwdn_pin, asserted ? GPIO_PIN_RESET : GPIO_PIN_SET);
}
static bool hal_drdy_low(void *ctx) {
    hb_afe_gpio_context_t *g = (hb_afe_gpio_context_t *)ctx;
    return HAL_GPIO_ReadPin(g->drdy_port, g->drdy_pin) == GPIO_PIN_RESET;
}
static bool hal_rails_good(void *ctx) { (void)ctx; return HB_BOARD_AFE_RAILS_GOOD(); }
static bool hal_vcap1_ready(void *ctx) { (void)ctx; return HB_BOARD_AFE_VCAP1_READY(); }
static void hal_set_start_active(void *ctx, bool active) {
    (void)ctx;
    HAL_GPIO_WritePin(HB_AFE_START_GPIO_Port, HB_AFE_START_Pin,
                      active ? GPIO_PIN_SET : GPIO_PIN_RESET);
}
static void hal_delay_ms(void *ctx, uint32_t ms) { (void)ctx; HAL_Delay(ms); }
static uint32_t hal_millis(void *ctx) { (void)ctx; return HAL_GetTick(); }
static void hal_idle(void *ctx) { (void)ctx; __NOP(); }
static void hal_fault(void *ctx, const char *reason) {
    hb_afe_gpio_context_t *g = (hb_afe_gpio_context_t *)ctx;
    HAL_GPIO_WritePin(HB_AFE_START_GPIO_Port, HB_AFE_START_Pin, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(g->cs_port, g->cs_pin, GPIO_PIN_SET);
    HAL_GPIO_WritePin(g->reset_port, g->reset_pin, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(g->pwdn_port, g->pwdn_pin, GPIO_PIN_RESET);
    extern void hb_board_fault(const char *message);
    hb_board_fault(reason);
}
static hb_hal_port_t make_port(hb_afe_gpio_context_t *g) {
    hb_hal_port_t p = {0};
    p.context = g;
    p.spi_transfer = hal_spi_transfer;
    p.chip_select = hal_chip_select;
    p.afe_reset = hal_reset;
    p.afe_power_down = hal_power_down;
    p.data_ready_is_low = hal_drdy_low;
    p.rails_good = hal_rails_good;
    p.vcap1_is_ready = hal_vcap1_ready;
    p.set_start_active = hal_set_start_active;
    p.delay_ms = hal_delay_ms;
    p.millis = hal_millis;
    p.idle = hal_idle;
    p.fault = hal_fault;
    p.micros = hb_board_micros;
    p.start_context = &shared_start_context;
    return p;
}
hb_hal_port_t hb_stm32h7_make_afe1_port(void) { return make_port(&afe1_gpio); }
hb_hal_port_t hb_stm32h7_make_afe2_port(void) { return make_port(&afe2_gpio); }
hb_hal_port_t hb_stm32h7_make_afe_port(void) { return hb_stm32h7_make_afe1_port(); }
