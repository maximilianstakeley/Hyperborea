#include "hb/packet_queue.h"
#include <string.h>

void hb_packet_queue_init(hb_packet_queue_t *queue) {
    if (queue == NULL) return;
    memset(queue->packets, 0, sizeof queue->packets);
    memset(queue->lengths, 0, sizeof queue->lengths);
    atomic_init(&queue->head, 0u);
    atomic_init(&queue->tail, 0u);
    atomic_init(&queue->tx_in_flight, false);
}

bool hb_packet_queue_push(hb_packet_queue_t *queue,
                          const uint8_t *packet, size_t length) {
    if (queue == NULL || packet == NULL ||
        (length != HB_PACKET_BYTES && length != HB_DUAL_PACKET_BYTES) ||
        packet[0] != 'H' || packet[1] != 'B' || packet[2] != 'C' || packet[3] != 'I')
        return false;
    const uint16_t frame_length = (uint16_t)packet[6] | ((uint16_t)packet[7] << 8u);
    if (frame_length != length) return false;
    const unsigned head = atomic_load_explicit(&queue->head, memory_order_relaxed);
    const unsigned tail = atomic_load_explicit(&queue->tail, memory_order_acquire);
    const unsigned next = (head + 1u) % HB_PACKET_QUEUE_SLOTS;
    if (next == tail) return false;
    memcpy(queue->packets[head], packet, length);
    queue->lengths[head] = (uint8_t)length;
    atomic_store_explicit(&queue->head, next, memory_order_release);
    return true;
}

bool hb_packet_queue_begin_tx(hb_packet_queue_t *queue, const uint8_t **packet,
                              size_t *length) {
    if (queue == NULL || packet == NULL || length == NULL) return false;
    bool expected = false;
    if (!atomic_compare_exchange_strong_explicit(&queue->tx_in_flight, &expected, true,
                                                  memory_order_acq_rel,
                                                  memory_order_relaxed)) return false;
    const unsigned tail = atomic_load_explicit(&queue->tail, memory_order_relaxed);
    const unsigned head = atomic_load_explicit(&queue->head, memory_order_acquire);
    if (tail == head) {
        atomic_store_explicit(&queue->tx_in_flight, false, memory_order_release);
        return false;
    }
    *packet = queue->packets[tail];
    *length = queue->lengths[tail];
    return true;
}

void hb_packet_queue_submit_failed(hb_packet_queue_t *queue) {
    if (queue != NULL)
        atomic_store_explicit(&queue->tx_in_flight, false, memory_order_release);
}

void hb_packet_queue_tx_complete(hb_packet_queue_t *queue) {
    if (queue == NULL ||
        !atomic_load_explicit(&queue->tx_in_flight, memory_order_acquire)) return;
    unsigned tail = atomic_load_explicit(&queue->tail, memory_order_relaxed);
    tail = (tail + 1u) % HB_PACKET_QUEUE_SLOTS;
    atomic_store_explicit(&queue->tail, tail, memory_order_release);
    atomic_store_explicit(&queue->tx_in_flight, false, memory_order_release);
}

unsigned hb_packet_queue_depth(const hb_packet_queue_t *queue) {
    if (queue == NULL) return 0u;
    const unsigned head = atomic_load_explicit(&queue->head, memory_order_acquire);
    const unsigned tail = atomic_load_explicit(&queue->tail, memory_order_acquire);
    return (head + HB_PACKET_QUEUE_SLOTS - tail) % HB_PACKET_QUEUE_SLOTS;
}
