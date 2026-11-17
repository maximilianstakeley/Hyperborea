#ifndef HB_USB_CDC_BRIDGE_H
#define HB_USB_CDC_BRIDGE_H
#include "hb/packet_queue.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
void hb_usb_cdc_bridge_init(void);
bool hb_usb_cdc_packet_sink(void *context, const uint8_t *packet, size_t length);
void hb_usb_cdc_receive_bytes(const uint8_t *data, size_t length);
bool hb_usb_cdc_take_start_request(void);
bool hb_usb_cdc_take_stop_request(void);
void hb_usb_cdc_service_tx(void);
void hb_usb_cdc_transmit_complete(void);
unsigned hb_usb_cdc_queue_depth(void);
#endif
