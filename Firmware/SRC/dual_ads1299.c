#include "hb/dual_ads1299.h"
#include <string.h>

static bool ports_are_synchronous(const hb_hal_port_t *a,
                                  const hb_hal_port_t *b) {
    if (!a || !b || !a->context || !b->context || a->context == b->context ||
        !a->set_start_active || !b->set_start_active ||
        a->set_start_active != b->set_start_active ||
        !a->chip_select || !b->chip_select || !a->afe_reset || !b->afe_reset ||
        !a->afe_power_down || !b->afe_power_down ||
        !a->data_ready_is_low || !b->data_ready_is_low || !a->micros || !b->micros)
        return false;
    const void *start_a = a->start_context != NULL ? a->start_context : a->context;
    const void *start_b = b->start_context != NULL ? b->start_context : b->context;
    return start_a == start_b;
}

hb_afe_result_t hb_dual_ads1299_init(hb_dual_ads1299_t *dual,
    hb_hal_port_t *p1, hb_hal_port_t *p2, const hb_afe_config_t *config) {
    if (!dual || !ports_are_synchronous(p1, p2)) return HB_AFE_ERR_ARGUMENT;
    memset(dual, 0, sizeof *dual);
    /* Assert both banks first. Their reset and power-down pins may be shared. */
    p1->afe_power_down(p1->context, true);
    p2->afe_power_down(p2->context, true);
    p1->afe_reset(p1->context, true);
    p2->afe_reset(p2->context, true);
    if (!p1->rails_good || !p2->rails_good || !p1->rails_good(p1->context) ||
        !p2->rails_good(p2->context)) return HB_AFE_ERR_RAILS;
    p1->afe_power_down(p1->context, false);
    p2->afe_power_down(p2->context, false);
    p1->afe_reset(p1->context, false);
    p2->afe_reset(p2->context, false);
    hb_afe_result_t r = hb_ads1299_init_released(&dual->afe[0], p1, config);
    if (r != HB_AFE_OK) return r;
    r = hb_ads1299_init_released(&dual->afe[1], p2, config);
    if (r != HB_AFE_OK) {
        (void)hb_ads1299_stop(&dual->afe[0]);
        dual->afe[0].initialized = false;
        return r;
    }
    dual->initialized = true;
    return HB_AFE_OK;
}

hb_afe_result_t hb_dual_ads1299_start(hb_dual_ads1299_t *dual) {
    if (!dual || !dual->initialized || dual->streaming) return HB_AFE_ERR_STATE;
    /* Both START callbacks drive the same hardware net. The first rising edge
       starts both converters; the second call simply reasserts that level. */
    hb_afe_result_t a = hb_ads1299_start(&dual->afe[0]);
    if (a != HB_AFE_OK) return a;
    hb_afe_result_t b = hb_ads1299_start(&dual->afe[1]);
    if (b != HB_AFE_OK) {
        (void)hb_ads1299_stop(&dual->afe[0]);
        return b;
    }
    dual->streaming = true;
    return HB_AFE_OK;
}

hb_afe_result_t hb_dual_ads1299_stop(hb_dual_ads1299_t *dual) {
    if (!dual || !dual->initialized || !dual->streaming) return HB_AFE_ERR_STATE;
    /* A single shared START line stops both converters at the same edge. */
    hb_hal_set_start(dual->afe[0].port, false);
    dual->afe[0].streaming = false;
    dual->afe[1].streaming = false;
    dual->streaming = false;
    return HB_AFE_OK;
}

hb_afe_result_t hb_dual_ads1299_read(hb_dual_ads1299_t *dual,
    hb_dual_ads1299_sample_t *out, uint32_t timeout_ms) {
    if (!dual || !out || !dual->streaming || timeout_ms == 0u)
        return HB_AFE_ERR_STATE;
    hb_hal_port_t *p1 = dual->afe[0].port;
    hb_hal_port_t *p2 = dual->afe[1].port;
    if (!p1->micros) return HB_AFE_ERR_ARGUMENT;
    const uint32_t start = p1->millis(p1->context);
    while (!p1->data_ready_is_low(p1->context) ||
           !p2->data_ready_is_low(p2->context)) {
        if ((uint32_t)(p1->millis(p1->context) - start) >= timeout_ms)
            return HB_AFE_ERR_TIMEOUT;
        p1->idle(p1->context);
    }
    const uint32_t paired_timestamp = p1->micros(p1->context);
    hb_ads1299_sample_t s1, s2;
    hb_afe_result_t r = hb_ads1299_read_ready_frame(&dual->afe[0], &s1);
    if (r != HB_AFE_OK) return r;
    r = hb_ads1299_read_ready_frame(&dual->afe[1], &s2);
    if (r != HB_AFE_OK) return r;
    if ((uint32_t)(p1->micros(p1->context) - paired_timestamp) >=
        1000000u / dual->afe[0].config.sample_rate) return HB_AFE_ERR_TIMEOUT;
    out->timestamp_us = paired_timestamp;
    out->status24[0] = s1.status24;
    out->status24[1] = s2.status24;
    memcpy(&out->channel[0], &s1.channel[0], sizeof s1.channel);
    memcpy(&out->channel[8], &s2.channel[0], sizeof s2.channel);
    return HB_AFE_OK;
}
