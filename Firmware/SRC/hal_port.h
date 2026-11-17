#ifndef HB_HAL_PORT_H
#define HB_HAL_PORT_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    void *context;
    /* Must be SPI mode 1, MSB-first, 8-bit; board configuration must meet the
       ADS1299 datasheet timing. The Rev28 board template uses <= 1 MHz SCLK. */
    bool (*spi_transfer)(void *context, const uint8_t *tx, uint8_t *rx,
                         size_t length, uint32_t timeout_ms);
    /* true means assert the active-low CS pin. */
    void (*chip_select)(void *context, bool asserted);
    /* true means assert the active-low RESET pin. */
    void (*afe_reset)(void *context, bool asserted);
    /* true means assert the active-low PWDN pin. */
    void (*afe_power_down)(void *context, bool asserted);
    /* Return true only when DRDY is physically LOW. */
    bool (*data_ready_is_low)(void *context);
    /* Must return true only after the required AFE rails are within limits. */
    bool (*rails_good)(void *context);
    /* Return true only after VCAP1 has reached the ADS1299 startup threshold. */
    bool (*vcap1_is_ready)(void *context);
    /* CLKSEL is a hardware strap in this design, not an MCU-controlled GPIO. */
    /* Drive START low to stop conversions; high to start conversions. */
    void (*set_start_active)(void *context, bool active);
    void (*delay_ms)(void *context, uint32_t milliseconds);
    uint32_t (*millis)(void *context);
    /* Cooperative yield; must allow the millisecond timebase to advance. */
    void (*idle)(void *context);
    void (*fault)(void *context, const char *reason);
    /* Monotonic microsecond counter for paired-ADC timing and rate checks.
       Dual-ADC firmware requires this to be hardware-backed and wrap-safe. */
    uint32_t (*micros)(void *context);
    /* Optional context for the shared START output; when NULL, context is used. */
    void *start_context;
} hb_hal_port_t;

static inline void hb_hal_set_start(hb_hal_port_t *port, bool active) {
    if (port != NULL && port->set_start_active != NULL) {
        void *context = port->start_context != NULL ? port->start_context : port->context;
        port->set_start_active(context, active);
    }
}

#ifdef __cplusplus
}
#endif
#endif
