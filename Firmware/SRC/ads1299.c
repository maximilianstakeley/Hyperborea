#include "hb/ads1299.h"
#include <string.h>

enum {
    CMD_RESET = 0x06, CMD_RDATAC = 0x10, CMD_SDATAC = 0x11,
    REG_ID = 0x00, REG_CONFIG1 = 0x01, REG_CONFIG2 = 0x02,
    REG_CONFIG3 = 0x03, REG_CH1SET = 0x05, REG_CH8SET = 0x0C
};

#define CONFIG2_DEFAULT_TEST_DISABLED 0xC0u
#define CONFIG3_RESERVED_AND_REFBUF_ON 0xE0u

static bool transfer(hb_ads1299_t *afe, const uint8_t *tx, uint8_t *rx,
                     size_t length) {
    if (afe == NULL || afe->port == NULL || tx == NULL ||
        afe->port->spi_transfer == NULL || afe->port->chip_select == NULL) {
        return false;
    }
    afe->port->chip_select(afe->port->context, true);
    const bool ok = afe->port->spi_transfer(afe->port->context, tx, rx,
                                            length, 20u);
    afe->port->chip_select(afe->port->context, false);
    return ok;
}

static hb_afe_result_t send_command(hb_ads1299_t *afe, uint8_t command) {
    uint8_t tx[1] = {command};
    uint8_t rx[1] = {0u};
    return transfer(afe, tx, rx, sizeof tx) ? HB_AFE_OK : HB_AFE_ERR_SPI;
}

static bool required_port_callbacks_present(const hb_hal_port_t *port) {
    return port != NULL && port->spi_transfer != NULL &&
           port->chip_select != NULL && port->afe_reset != NULL &&
           port->afe_power_down != NULL && port->data_ready_is_low != NULL &&
           port->rails_good != NULL && port->vcap1_is_ready != NULL &&
           port->set_start_active != NULL &&
           port->delay_ms != NULL && port->millis != NULL && port->idle != NULL;
}

static bool sample_rate_bits(hb_afe_rate_t rate, uint8_t *bits) {
    if (bits == NULL) return false;
    switch (rate) {
        case HB_AFE_RATE_16000_SPS: *bits = 0u; return true;
        case HB_AFE_RATE_8000_SPS:  *bits = 1u; return true;
        case HB_AFE_RATE_4000_SPS:  *bits = 2u; return true;
        case HB_AFE_RATE_2000_SPS:  *bits = 3u; return true;
        case HB_AFE_RATE_1000_SPS:  *bits = 4u; return true;
        case HB_AFE_RATE_500_SPS:   *bits = 5u; return true;
        case HB_AFE_RATE_250_SPS:   *bits = 6u; return true;
        default: return false;
    }
}

static bool gain_bits(uint8_t gain, uint8_t *bits) {
    if (bits == NULL) return false;
    switch (gain) {
        case 1u: *bits = 0u; return true;
        case 2u: *bits = 1u; return true;
        case 4u: *bits = 2u; return true;
        case 6u: *bits = 3u; return true;
        case 8u: *bits = 4u; return true;
        case 12u: *bits = 5u; return true;
        case 24u: *bits = 6u; return true;
        default: return false;
    }
}

hb_afe_config_t hb_ads1299_default_config(void) {
    hb_afe_config_t config;
    config.sample_rate = HB_AFE_RATE_1000_SPS;
    config.pga_gain = 24u;
    config.enable_internal_reference_buffer = true;
    config.reference_settle_ms = 150u;
    config.vcap1_timeout_ms = 1000u;
    return config;
}

hb_afe_result_t hb_ads1299_read_register(hb_ads1299_t *afe, uint8_t address,
                                         uint8_t *value) {
    if (afe == NULL || value == NULL || address > 0x17u) {
        return HB_AFE_ERR_ARGUMENT;
    }
    if (afe->continuous_read) return HB_AFE_ERR_STATE;
    const uint8_t tx[3] = {(uint8_t)(0x20u | (address & 0x1Fu)), 0x00u, 0x00u};
    uint8_t rx[3] = {0u, 0u, 0u};
    if (!transfer(afe, tx, rx, sizeof tx)) return HB_AFE_ERR_SPI;
    *value = rx[2];
    return HB_AFE_OK;
}

hb_afe_result_t hb_ads1299_write_register(hb_ads1299_t *afe, uint8_t address,
                                          uint8_t value) {
    if (afe == NULL || address > 0x17u) return HB_AFE_ERR_ARGUMENT;
    if (afe->continuous_read) return HB_AFE_ERR_STATE;
    const uint8_t tx[3] = {(uint8_t)(0x40u | (address & 0x1Fu)), 0x00u, value};
    return transfer(afe, tx, NULL, sizeof tx) ? HB_AFE_OK : HB_AFE_ERR_SPI;
}

static hb_afe_result_t fail_initialization(hb_ads1299_t *afe,
                                               hb_afe_result_t result,
                                               const char *message) {
    if (afe != NULL && afe->port != NULL) {
        hb_hal_port_t *port = afe->port;
        hb_hal_set_start(port, false);
        if (port->chip_select != NULL)
            port->chip_select(port->context, false);
        if (port->afe_reset != NULL)
            port->afe_reset(port->context, true);
        if (port->afe_power_down != NULL)
            port->afe_power_down(port->context, true);
        afe->initialized = false;
        afe->continuous_read = false;
        afe->streaming = false;
        if (port->fault != NULL) port->fault(port->context, message);
    }
    return result;
}

static hb_afe_result_t initialize(hb_ads1299_t *afe, hb_hal_port_t *port, bool cycle_lines,
                                const hb_afe_config_t *requested) {
    if (afe == NULL || !required_port_callbacks_present(port))
        return HB_AFE_ERR_ARGUMENT;

    hb_afe_config_t config = requested != NULL ? *requested : hb_ads1299_default_config();
    uint8_t rate = 0u, gain = 0u;
    if (!sample_rate_bits(config.sample_rate, &rate) || !gain_bits(config.pga_gain, &gain))
        return HB_AFE_ERR_ARGUMENT;

    memset(afe, 0, sizeof *afe);
    afe->port = port;
    if (config.reference_settle_ms == 0u || config.vcap1_timeout_ms == 0u)
        return HB_AFE_ERR_ARGUMENT;
    afe->config = config;

    /* Active-low RESET and PWDN: hold both asserted until rails are good. */
    if (cycle_lines) {
        port->afe_power_down(port->context, true);
        port->afe_reset(port->context, true);
    }
    port->idle(port->context);
    if (!port->rails_good(port->context))
        return fail_initialization(afe, HB_AFE_ERR_RAILS, "AFE rails outside limits");

    /* TI power-up sequence: CLKSEL=0 for the external clock; release PWDN and
       RESET after rails are stable; wait >2^18 tCLK (128 ms at 2.048 MHz),
       then require VCAP1 >= 1.1 V before issuing RESET. */
    /* CLKSEL is strapped low by the Rev27 schematic; no MCU pin drives it. */
    hb_hal_set_start(port, false);
    port->afe_power_down(port->context, false);
    port->afe_reset(port->context, false);
    port->delay_ms(port->context, 130u);
    const uint32_t vcap_wait_started = port->millis(port->context);
    while (!port->vcap1_is_ready(port->context)) {
        if ((uint32_t)(port->millis(port->context) - vcap_wait_started) >=
            config.vcap1_timeout_ms)
            return fail_initialization(afe, HB_AFE_ERR_TIMEOUT, "AFE VCAP1 startup threshold not reached");
        port->idle(port->context);
    }
    if (send_command(afe, CMD_RESET) != HB_AFE_OK)
        return fail_initialization(afe, HB_AFE_ERR_SPI, "ADS1299 RESET command failed");
    port->delay_ms(port->context, 1u); /* >18 tCLK after reset */
    if (send_command(afe, CMD_SDATAC) != HB_AFE_OK)
        return fail_initialization(afe, HB_AFE_ERR_SPI, "ADS1299 SDATAC command failed");
    port->delay_ms(port->context, 1u);

    uint8_t id = 0u;
    if (hb_ads1299_read_register(afe, REG_ID, &id) != HB_AFE_OK)
        return fail_initialization(afe, HB_AFE_ERR_SPI, "ADS1299 ID register read failed");
    /* ID[4:2] = 111 identifies ADS1299; ID[1:0] = 10 denotes eight channels.
       Revision bits [7:5] are intentionally not fixed. */
    if ((id & 0x1Fu) != 0x1Eu)
        return fail_initialization(afe, HB_AFE_ERR_ID, "ADS1299 device ID mismatch");
    afe->device_id = id;

    /* TI table 13: bits 7 and 4 must be one. Single frame per CS. */
    const uint8_t config1 = (uint8_t)(0x90u | rate);
    const uint8_t config2 = CONFIG2_DEFAULT_TEST_DISABLED;
    const uint8_t config3 = config.enable_internal_reference_buffer
                                ? CONFIG3_RESERVED_AND_REFBUF_ON : 0x60u;
    if (hb_ads1299_write_register(afe, REG_CONFIG3, config3) != HB_AFE_OK)
        return fail_initialization(afe, HB_AFE_ERR_SPI, "ADS1299 CONFIG3 write failed");
    if (config.enable_internal_reference_buffer) {
        /* TI requires the internal reference to settle after CONFIG3=E0h. */
        port->delay_ms(port->context, config.reference_settle_ms);
    }
    if (hb_ads1299_write_register(afe, REG_CONFIG1, config1) != HB_AFE_OK ||
        hb_ads1299_write_register(afe, REG_CONFIG2, config2) != HB_AFE_OK) {
        return fail_initialization(afe, HB_AFE_ERR_SPI, "ADS1299 configuration write failed");
    }

    const uint8_t channel_setting = (uint8_t)((gain << 4) | 0x00u);
    for (uint8_t reg = REG_CH1SET; reg <= REG_CH8SET; ++reg) {
        if (hb_ads1299_write_register(afe, reg, channel_setting) != HB_AFE_OK)
            return fail_initialization(afe, HB_AFE_ERR_SPI, "ADS1299 channel configuration failed");
    }

    const uint8_t registers[] = {
        REG_CONFIG1, REG_CONFIG2, REG_CONFIG3,
        REG_CH1SET, 0x06u, 0x07u, 0x08u, 0x09u, 0x0Au, 0x0Bu, REG_CH8SET
    };
    const uint8_t expected[] = {
        config1, config2, config3,
        channel_setting, channel_setting, channel_setting, channel_setting,
        channel_setting, channel_setting, channel_setting, channel_setting
    };
    for (size_t i = 0u; i < sizeof registers; ++i) {
        uint8_t actual = 0u;
        if (hb_ads1299_read_register(afe, registers[i], &actual) != HB_AFE_OK)
            return fail_initialization(afe, HB_AFE_ERR_SPI, "ADS1299 register readback failed");
        if (actual != expected[i])
            return fail_initialization(afe, HB_AFE_ERR_REGISTER_VERIFY, "ADS1299 register readback mismatch");
    }

    /* Enter continuous-read mode while conversions remain stopped. The START
       pin is the hardware conversion control, so no command is sent while
       RDATAC is active. This avoids the ADS1299 command-mode restriction. */
    if (send_command(afe, CMD_RDATAC) != HB_AFE_OK)
        return fail_initialization(afe, HB_AFE_ERR_SPI, "ADS1299 RDATAC command failed");
    port->delay_ms(port->context, 1u);
    afe->continuous_read = true;
    afe->initialized = true;
    return HB_AFE_OK;
}

hb_afe_result_t hb_ads1299_start(hb_ads1299_t *afe) {
    if (afe == NULL || !afe->initialized || afe->streaming)
        return HB_AFE_ERR_STATE;
    /* Conversion control uses the dedicated START pin. Commands other than
       SDATAC are not legal while RDATAC mode is active. */
    hb_hal_set_start(afe->port, true);
    afe->streaming = true;
    return HB_AFE_OK;
}

hb_afe_result_t hb_ads1299_stop(hb_ads1299_t *afe) {
    if (afe == NULL || !afe->initialized || !afe->streaming)
        return HB_AFE_ERR_STATE;
    /* Pulling START low halts conversions without issuing STOP in RDATAC. */
    hb_hal_set_start(afe->port, false);
    afe->streaming = false;
    return HB_AFE_OK;
}

static int32_t sign_extend_24(const uint8_t *bytes) {
    uint32_t value = ((uint32_t)bytes[0] << 16) |
                     ((uint32_t)bytes[1] << 8) | (uint32_t)bytes[2];
    if ((value & 0x00800000u) != 0u) value |= 0xFF000000u;
    return (int32_t)value;
}

hb_afe_result_t hb_ads1299_read_sample(hb_ads1299_t *afe,
                                       hb_ads1299_sample_t *sample,
                                       uint32_t timeout_ms) {
    if (afe == NULL || sample == NULL || !afe->initialized || !afe->streaming)
        return HB_AFE_ERR_STATE;

    const uint32_t started = afe->port->millis(afe->port->context);
    while (!afe->port->data_ready_is_low(afe->port->context)) {
        if ((uint32_t)(afe->port->millis(afe->port->context) - started) >= timeout_ms)
            return HB_AFE_ERR_TIMEOUT;
        afe->port->idle(afe->port->context);
    }

    return hb_ads1299_read_ready_frame(afe, sample);
}

hb_afe_result_t hb_ads1299_read_ready_frame(hb_ads1299_t *afe, hb_ads1299_sample_t *sample) {
    if (!afe || !sample || !afe->streaming) return HB_AFE_ERR_STATE;
    uint8_t tx[HB_ADS1299_FRAME_BYTES] = {0u};
    uint8_t rx[HB_ADS1299_FRAME_BYTES] = {0u};
    if (!transfer(afe, tx, rx, sizeof tx)) return HB_AFE_ERR_SPI;
    sample->status24 = ((uint32_t)rx[0] << 16) |
                       ((uint32_t)rx[1] << 8) | (uint32_t)rx[2];
    for (size_t channel = 0u; channel < HB_ADS1299_CHANNELS; ++channel) {
        sample->channel[channel] = sign_extend_24(&rx[3u + channel * 3u]);
    }
    sample->timestamp_ms = afe->port->millis(afe->port->context);
    return HB_AFE_OK;
}

hb_afe_result_t hb_ads1299_init(hb_ads1299_t *afe, hb_hal_port_t *port,
                                const hb_afe_config_t *config) {
    return initialize(afe, port, true, config);
}

hb_afe_result_t hb_ads1299_init_released(hb_ads1299_t *afe, hb_hal_port_t *port,
                                         const hb_afe_config_t *config) {
    return initialize(afe, port, false, config);
}
