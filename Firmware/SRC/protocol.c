#include "hb/protocol.h"

static void put_u16_le(uint8_t *output, uint16_t value) {
    output[0] = (uint8_t)(value & 0xFFu);
    output[1] = (uint8_t)(value >> 8);
}
static void put_u32_le(uint8_t *output, uint32_t value) {
    output[0] = (uint8_t)(value & 0xFFu);
    output[1] = (uint8_t)((value >> 8) & 0xFFu);
    output[2] = (uint8_t)((value >> 16) & 0xFFu);
    output[3] = (uint8_t)((value >> 24) & 0xFFu);
}

uint16_t hb_crc16_ccitt(const uint8_t *data, size_t length) {
    if (data == NULL && length != 0u) return 0u;
    uint16_t crc = 0xFFFFu;
    for (size_t i = 0u; i < length; ++i) {
        crc ^= (uint16_t)data[i] << 8;
        for (unsigned bit = 0u; bit < 8u; ++bit) {
            crc = (crc & 0x8000u) != 0u
                ? (uint16_t)((crc << 1) ^ 0x1021u)
                : (uint16_t)(crc << 1);
        }
    }
    return crc;
}

size_t hb_encode_sample_packet(const hb_ads1299_sample_t *sample,
                               uint32_t sequence, uint8_t *output,
                               size_t capacity) {
    if (sample == NULL || output == NULL || capacity < HB_PACKET_BYTES ||
        sample->status24 > 0x00FFFFFFu) return 0u;
    for (size_t i = 0u; i < HB_ADS1299_CHANNELS; ++i) {
        if (sample->channel[i] < -8388608 || sample->channel[i] > 8388607)
            return 0u;
    }

    output[0] = 'H'; output[1] = 'B'; output[2] = 'C'; output[3] = 'I';
    output[4] = HB_PACKET_VERSION;
    output[5] = HB_PACKET_TYPE_SAMPLE;
    put_u16_le(&output[6], (uint16_t)HB_PACKET_BYTES);
    put_u32_le(&output[8], sequence);
    put_u32_le(&output[12], sample->timestamp_ms);

    output[16] = (uint8_t)(sample->status24 >> 16);
    output[17] = (uint8_t)(sample->status24 >> 8);
    output[18] = (uint8_t)sample->status24;
    for (size_t i = 0u; i < HB_ADS1299_CHANNELS; ++i) {
        const uint32_t value = (uint32_t)sample->channel[i] & 0x00FFFFFFu;
        const size_t offset = 19u + i * 3u;
        output[offset] = (uint8_t)(value >> 16);
        output[offset + 1u] = (uint8_t)(value >> 8);
        output[offset + 2u] = (uint8_t)value;
    }
    const uint16_t crc = hb_crc16_ccitt(output, HB_PACKET_HEADER_BYTES + HB_PACKET_PAYLOAD_BYTES);
    put_u16_le(&output[HB_PACKET_BYTES - HB_PACKET_CRC_BYTES], crc);
    return HB_PACKET_BYTES;
}

size_t hb_encode_dual_sample_packet(const hb_dual_ads1299_sample_t *sample,
                                    uint32_t sequence, uint8_t *output,
                                    size_t capacity) {
    if (sample == NULL || output == NULL || capacity < HB_DUAL_PACKET_BYTES ||
        sample->status24[0] > 0x00FFFFFFu || sample->status24[1] > 0x00FFFFFFu)
        return 0u;
    for (size_t i = 0u; i < HB_DUAL_AFE_CHANNELS; ++i) {
        if (sample->channel[i] < -8388608 || sample->channel[i] > 8388607)
            return 0u;
    }
    output[0] = 'H'; output[1] = 'B'; output[2] = 'C'; output[3] = 'I';
    output[4] = HB_PACKET_DUAL_VERSION;
    output[5] = HB_PACKET_TYPE_DUAL_SAMPLE;
    put_u16_le(&output[6], (uint16_t)HB_DUAL_PACKET_BYTES);
    put_u32_le(&output[8], sequence);
    put_u32_le(&output[12], sample->timestamp_us);
    for (size_t i = 0u; i < 2u; ++i) {
        const size_t offset = 16u + i * 3u;
        output[offset] = (uint8_t)(sample->status24[i] >> 16);
        output[offset + 1u] = (uint8_t)(sample->status24[i] >> 8);
        output[offset + 2u] = (uint8_t)sample->status24[i];
    }
    for (size_t i = 0u; i < HB_DUAL_AFE_CHANNELS; ++i) {
        const uint32_t value = (uint32_t)sample->channel[i] & 0x00FFFFFFu;
        const size_t offset = 22u + i * 3u;
        output[offset] = (uint8_t)(value >> 16);
        output[offset + 1u] = (uint8_t)(value >> 8);
        output[offset + 2u] = (uint8_t)value;
    }
    const uint16_t crc = hb_crc16_ccitt(output,
                                        HB_PACKET_HEADER_BYTES + HB_DUAL_PACKET_PAYLOAD_BYTES);
    put_u16_le(&output[HB_DUAL_PACKET_BYTES - HB_PACKET_CRC_BYTES], crc);
    return HB_DUAL_PACKET_BYTES;
}
