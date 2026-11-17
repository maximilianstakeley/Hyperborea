#include "hb/dual_acquisition.h"
#include <string.h>

static void enter_safe_state(hb_dual_acquisition_t *a) {
    if (!a) return;
    for (size_t i = 0u; i < 2u; ++i) {
        hb_hal_port_t *p = a->dual.afe[i].port;
        if (!p) continue;
        hb_hal_set_start(p, false);
        if (p->chip_select) p->chip_select(p->context, false);
        if (p->afe_reset) p->afe_reset(p->context, true);
        if (p->afe_power_down) p->afe_power_down(p->context, true);
        a->dual.afe[i].streaming = false;
        a->dual.afe[i].initialized = false;
        a->dual.afe[i].continuous_read = false;
    }
    a->dual.streaming = false;
    a->running = false;
}

static hb_afe_result_t latch_fault(hb_dual_acquisition_t *a,
                                   hb_afe_result_t result,
                                   const char *message) {
    if (!a) return result;
    a->faulted = true;
    enter_safe_state(a);
    for (size_t i = 0u; i < 2u; ++i) {
        hb_hal_port_t *p = a->dual.afe[i].port;
        if (p && p->fault) p->fault(p->context, message);
    }
    return result;
}

hb_afe_result_t hb_dual_acquisition_init(hb_dual_acquisition_t *a,
    hb_hal_port_t *p1, hb_hal_port_t *p2, const hb_afe_config_t *config,
    hb_dual_packet_sink_fn sink, void *sink_context) {
    if (!a || !sink) return HB_AFE_ERR_ARGUMENT;
    memset(a, 0, sizeof *a);
    a->sink = sink;
    a->sink_context = sink_context;
    const hb_afe_result_t r = hb_dual_ads1299_init(&a->dual, p1, p2, config);
    if (r != HB_AFE_OK) {
        a->dual.afe[0].port = p1;
        a->dual.afe[1].port = p2;
        return latch_fault(a, r, "Dual ADS1299 initialization failed; both AFEs held in reset");
    }
    return HB_AFE_OK;
}

hb_afe_result_t hb_dual_acquisition_start(hb_dual_acquisition_t *a) {
    if (!a || !a->dual.initialized || a->running || a->faulted)
        return HB_AFE_ERR_STATE;
    const hb_afe_result_t r = hb_dual_ads1299_start(&a->dual);
    if (r != HB_AFE_OK) return latch_fault(a, r, "Shared ADS1299 START failed");
    a->running = true;
    return HB_AFE_OK;
}

hb_afe_result_t hb_dual_acquisition_stop(hb_dual_acquisition_t *a) {
    if (!a || !a->running) return HB_AFE_ERR_STATE;
    const hb_afe_result_t r = hb_dual_ads1299_stop(&a->dual);
    if (r != HB_AFE_OK) return latch_fault(a, r, "Shared ADS1299 STOP failed");
    a->running = false;
    return HB_AFE_OK;
}

hb_afe_result_t hb_dual_acquisition_poll(hb_dual_acquisition_t *a,
                                         uint32_t drdy_timeout_ms) {
    if (!a || !a->running || a->faulted || drdy_timeout_ms == 0u)
        return HB_AFE_ERR_STATE;
    hb_dual_ads1299_sample_t sample;
    hb_afe_result_t r = hb_dual_ads1299_read(&a->dual, &sample, drdy_timeout_ms);
    if (r != HB_AFE_OK)
        return latch_fault(a, r, "Paired ADS1299 sample read failed; acquisition stopped");
    uint8_t packet[HB_DUAL_PACKET_BYTES];
    if (hb_encode_dual_sample_packet(&sample, a->next_sequence, packet,
                                     sizeof packet) != sizeof packet)
        return latch_fault(a, HB_AFE_ERR_ARGUMENT, "Dual sample packet encoding failed");
    if (!a->sink(a->sink_context, packet, sizeof packet)) {
        ++a->transport_errors;
        return latch_fault(a, HB_AFE_ERR_SPI, "Dual sample transport rejected packet");
    }
    ++a->next_sequence;
    ++a->samples_emitted;
    return HB_AFE_OK;
}
