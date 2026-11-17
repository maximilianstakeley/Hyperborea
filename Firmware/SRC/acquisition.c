#include "hb/acquisition.h"
#include <string.h>

static hb_afe_result_t latch_fault(hb_acquisition_t *acquisition,
                                   hb_afe_result_t reason,
                                   const char *message);

hb_afe_result_t hb_acquisition_init(hb_acquisition_t *acquisition,
                                    hb_hal_port_t *port,
                                    const hb_afe_config_t *config,
                                    hb_packet_sink_fn sink,
                                    void *sink_context) {
    if (acquisition == NULL || sink == NULL) return HB_AFE_ERR_ARGUMENT;
    memset(acquisition, 0, sizeof *acquisition);
    acquisition->sink = sink;
    acquisition->sink_context = sink_context;
    return hb_ads1299_init(&acquisition->afe, port, config);
}

hb_afe_result_t hb_acquisition_start(hb_acquisition_t *acquisition) {
    if (acquisition == NULL || acquisition->running || acquisition->faulted)
        return HB_AFE_ERR_STATE;
    const hb_afe_result_t result = hb_ads1299_start(&acquisition->afe);
    if (result == HB_AFE_OK) {
        acquisition->running = true;
        return HB_AFE_OK;
    }
    return latch_fault(acquisition, result, "ADS1299 START command failed");
}

hb_afe_result_t hb_acquisition_stop(hb_acquisition_t *acquisition) {
    if (acquisition == NULL || !acquisition->running) return HB_AFE_ERR_STATE;
    const hb_afe_result_t result = hb_ads1299_stop(&acquisition->afe);
    if (result == HB_AFE_OK) {
        acquisition->running = false;
        return HB_AFE_OK;
    }
    return latch_fault(acquisition, result, "ADS1299 STOP command failed");
}

static hb_afe_result_t latch_fault(hb_acquisition_t *acquisition,
                                   hb_afe_result_t reason,
                                   const char *message) {
    acquisition->faulted = true;
    acquisition->running = false;
    if (acquisition->afe.port != NULL) {
        hb_hal_port_t *port = acquisition->afe.port;
        if (acquisition->afe.streaming) (void)hb_ads1299_stop(&acquisition->afe);
        /* Do not depend on a successful SPI STOP when entering a fault state. */
        hb_hal_set_start(port, false);
        if (port->chip_select != NULL) port->chip_select(port->context, false);
        if (port->afe_reset != NULL) port->afe_reset(port->context, true);
        if (port->afe_power_down != NULL) port->afe_power_down(port->context, true);
        acquisition->afe.streaming = false;
        acquisition->afe.initialized = false;
        acquisition->afe.continuous_read = false;
        if (port->fault != NULL) port->fault(port->context, message);
    }
    return reason;
}

hb_afe_result_t hb_acquisition_poll(hb_acquisition_t *acquisition,
                                    uint32_t drdy_timeout_ms) {
    if (acquisition == NULL || !acquisition->running || acquisition->faulted)
        return HB_AFE_ERR_STATE;

    hb_ads1299_sample_t sample;
    hb_afe_result_t result = hb_ads1299_read_sample(&acquisition->afe, &sample,
                                                     drdy_timeout_ms);
    if (result != HB_AFE_OK)
        return latch_fault(acquisition, result, "AFE sample acquisition failed; stream stopped");

    uint8_t packet[HB_PACKET_BYTES];
    const size_t packet_length = hb_encode_sample_packet(&sample,
                                                          acquisition->next_sequence,
                                                          packet, sizeof packet);
    if (packet_length != HB_PACKET_BYTES)
        return latch_fault(acquisition, HB_AFE_ERR_ARGUMENT, "Sample packet encoding failed");
    if (!acquisition->sink(acquisition->sink_context, packet, packet_length)) {
        acquisition->transport_errors++;
        return latch_fault(acquisition, HB_AFE_ERR_SPI, "Sample transport rejected packet; stream stopped");
    }
    acquisition->next_sequence++;
    acquisition->samples_emitted++;
    return HB_AFE_OK;
}
