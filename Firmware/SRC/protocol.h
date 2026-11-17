#ifndef HB_PROTOCOL_H
#define HB_PROTOCOL_H
#include "hb/ads1299.h"
#include "hb/dual_ads1299.h"
#ifdef __cplusplus
extern "C" {
#endif

#define HB_PACKET_VERSION 1u
#define HB_PACKET_TYPE_SAMPLE 1u
#define HB_PACKET_DUAL_VERSION 2u
#define HB_PACKET_TYPE_DUAL_SAMPLE 2u
#define HB_PACKET_HEADER_BYTES 16u
#define HB_PACKET_PAYLOAD_BYTES 27u
#define HB_PACKET_CRC_BYTES 2u
#define HB_PACKET_BYTES (HB_PACKET_HEADER_BYTES + HB_PACKET_PAYLOAD_BYTES + HB_PACKET_CRC_BYTES)
#define HB_DUAL_PACKET_PAYLOAD_BYTES 54u
#define HB_DUAL_PACKET_BYTES (HB_PACKET_HEADER_BYTES + HB_DUAL_PACKET_PAYLOAD_BYTES + HB_PACKET_CRC_BYTES)

uint16_t hb_crc16_ccitt(const uint8_t *data, size_t length);
/* Returns HB_PACKET_BYTES, or zero for an invalid argument/sample. */
size_t hb_encode_sample_packet(const hb_ads1299_sample_t *sample,
                               uint32_t sequence, uint8_t *output,
                               size_t capacity);
/* Version 2 uses a microsecond timestamp, two 24-bit status words and 16
   signed 24-bit channels. */
size_t hb_encode_dual_sample_packet(const hb_dual_ads1299_sample_t *sample,
                                    uint32_t sequence, uint8_t *output,
                                    size_t capacity);
#ifdef __cplusplus
}
#endif
#endif
