#include "hb/packet_queue.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

int main(void) {
    hb_packet_queue_t queue;
    hb_packet_queue_init(&queue);
    uint8_t packet[HB_DUAL_PACKET_BYTES] = {'H','B','C','I'};
    packet[6] = (uint8_t)HB_PACKET_BYTES;
    packet[7] = (uint8_t)(HB_PACKET_BYTES >> 8u);
    for (unsigned i=0; i<HB_PACKET_QUEUE_SLOTS-1u; ++i) {
        packet[8]=(uint8_t)i;
        assert(hb_packet_queue_push(&queue,packet,HB_PACKET_BYTES));
    }
    assert(hb_packet_queue_depth(&queue)==HB_PACKET_QUEUE_SLOTS-1u);
    assert(!hb_packet_queue_push(&queue,packet,HB_PACKET_BYTES));

    const uint8_t *front=NULL;
    size_t length=0u;
    assert(hb_packet_queue_begin_tx(&queue,&front,&length));
    assert(front != NULL && front[0]=='H' && front[8]==0u && length==HB_PACKET_BYTES);
    assert(!hb_packet_queue_begin_tx(&queue,&front,&length)); /* one in-flight only */
    hb_packet_queue_submit_failed(&queue);
    assert(hb_packet_queue_begin_tx(&queue,&front,&length));
    assert(front[8]==0u); /* rejected submit did not discard the item */
    hb_packet_queue_tx_complete(&queue);
    assert(hb_packet_queue_depth(&queue)==HB_PACKET_QUEUE_SLOTS-2u);
    assert(hb_packet_queue_begin_tx(&queue,&front,&length));
    assert(front[8]==1u);
    hb_packet_queue_tx_complete(&queue);
    assert(hb_packet_queue_depth(&queue)==HB_PACKET_QUEUE_SLOTS-3u);

    while (hb_packet_queue_begin_tx(&queue,&front,&length)) hb_packet_queue_tx_complete(&queue);
    assert(hb_packet_queue_depth(&queue)==0u);
    assert(!hb_packet_queue_begin_tx(&queue,&front,&length));

    packet[6] = (uint8_t)HB_DUAL_PACKET_BYTES;
    packet[7] = (uint8_t)(HB_DUAL_PACKET_BYTES >> 8u);
    packet[4] = HB_PACKET_DUAL_VERSION;
    packet[5] = HB_PACKET_TYPE_DUAL_SAMPLE;
    assert(hb_packet_queue_push(&queue, packet, HB_DUAL_PACKET_BYTES));
    assert(hb_packet_queue_begin_tx(&queue, &front, &length));
    assert(length == HB_DUAL_PACKET_BYTES);
    assert(front[4] == HB_PACKET_DUAL_VERSION);
    hb_packet_queue_tx_complete(&queue);

    assert(!hb_packet_queue_push(&queue, packet, HB_PACKET_BYTES + 1u));
    puts("packet queue tests passed");
    return 0;
}
