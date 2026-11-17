#ifndef HB_ACQUISITION_H
#define HB_ACQUISITION_H
#include "hb/ads1299.h"
#include "hb/protocol.h"
#ifdef __cplusplus
extern "C" {
#endif

typedef bool (*hb_packet_sink_fn)(void *context, const uint8_t *packet,
                                  size_t length);

typedef struct {
    hb_ads1299_t afe;
    hb_packet_sink_fn sink;
    void *sink_context;
    uint32_t next_sequence;
    uint32_t samples_emitted;
    uint32_t transport_errors;
    bool running;
    bool faulted;
} hb_acquisition_t;

hb_afe_result_t hb_acquisition_init(hb_acquisition_t *acquisition,
                                    hb_hal_port_t *port,
                                    const hb_afe_config_t *config,
                                    hb_packet_sink_fn sink,
                                    void *sink_context);
hb_afe_result_t hb_acquisition_start(hb_acquisition_t *acquisition);
hb_afe_result_t hb_acquisition_stop(hb_acquisition_t *acquisition);
/* Invoke from a cooperative task/main loop, not an interrupt handler. */
hb_afe_result_t hb_acquisition_poll(hb_acquisition_t *acquisition,
                                    uint32_t drdy_timeout_ms);
#ifdef __cplusplus
}
#endif
#endif
