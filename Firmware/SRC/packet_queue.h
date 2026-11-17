#ifndef HB_PACKET_QUEUE_H
#define HB_PACKET_QUEUE_H
#include "hb/protocol.h"
#include <stdatomic.h>
#ifdef __cplusplus
extern "C" {
#endif

#define HB_PACKET_QUEUE_SLOTS 16u
/* SPSC queue: acquisition/main context produces; USB completion context consumes.
   One slot is reserved to distinguish full from empty (15 packets usable). */
typedef struct {
    uint8_t packets[HB_PACKET_QUEUE_SLOTS][HB_DUAL_PACKET_BYTES];
    uint8_t lengths[HB_PACKET_QUEUE_SLOTS];
    atomic_uint head;
    atomic_uint tail;
    atomic_bool tx_in_flight;
} hb_packet_queue_t;

void hb_packet_queue_init(hb_packet_queue_t *queue);
bool hb_packet_queue_push(hb_packet_queue_t *queue,
                          const uint8_t *packet, size_t length);
/* Begin TX returns a stable pointer retained until tx_complete(). */
bool hb_packet_queue_begin_tx(hb_packet_queue_t *queue,
                              const uint8_t **packet, size_t *length);
void hb_packet_queue_submit_failed(hb_packet_queue_t *queue);
void hb_packet_queue_tx_complete(hb_packet_queue_t *queue);
unsigned hb_packet_queue_depth(const hb_packet_queue_t *queue);
#ifdef __cplusplus
}
#endif
#endif
