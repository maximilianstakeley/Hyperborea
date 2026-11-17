#include "hb/packet.hpp"
#include "hb/protocol.h"
#include <algorithm>

namespace hb {
namespace {
std::uint16_t read_u16_le(const std::uint8_t *p) {
    return static_cast<std::uint16_t>(p[0]) |
           static_cast<std::uint16_t>(static_cast<std::uint16_t>(p[1]) << 8);
}
std::uint32_t read_u32_le(const std::uint8_t *p) {
    return static_cast<std::uint32_t>(p[0]) |
           (static_cast<std::uint32_t>(p[1]) << 8) |
           (static_cast<std::uint32_t>(p[2]) << 16) |
           (static_cast<std::uint32_t>(p[3]) << 24);
}
std::int32_t sign_extend_24(const std::uint8_t *p) {
    std::uint32_t value = (static_cast<std::uint32_t>(p[0]) << 16) |
                          (static_cast<std::uint32_t>(p[1]) << 8) |
                          static_cast<std::uint32_t>(p[2]);
    if ((value & 0x00800000u) != 0u) value |= 0xFF000000u;
    return static_cast<std::int32_t>(value);
}
}

std::uint16_t crc16_ccitt(const std::uint8_t *data, std::size_t length) {
    return hb_crc16_ccitt(data, length);
}

bool decode_packet(const std::uint8_t *p, std::size_t n, SamplePacket &out) {
    if (p == nullptr || (n != kPacketBytes && n != kDualPacketBytes) ||
        p[0]!='H' || p[1]!='B' || p[2]!='C' || p[3]!='I' ||
        read_u16_le(p+6) != n) return false;
    const bool single = p[4] == HB_PACKET_VERSION && p[5] == HB_PACKET_TYPE_SAMPLE && n == kPacketBytes;
    const bool dual = p[4] == HB_PACKET_DUAL_VERSION && p[5] == HB_PACKET_TYPE_DUAL_SAMPLE && n == kDualPacketBytes;
    if (!single && !dual) return false;
    const std::uint16_t expected = read_u16_le(p + n - 2u);
    if (crc16_ccitt(p, n - 2u) != expected) return false;
    out.version = p[4];
    out.channel_count = single ? 8u : 16u;
    out.sequence = read_u32_le(p+8);
    if (single) {
        out.timestamp_ms = read_u32_le(p+12);
        out.timestamp_us = out.timestamp_ms * 1000u;
        out.status24 = (static_cast<std::uint32_t>(p[16]) << 16) |
                       (static_cast<std::uint32_t>(p[17]) << 8) | p[18];
        out.status24_secondary = 0u;
        for (std::size_t i=0; i<8; ++i) out.channel[i] = sign_extend_24(p+19+i*3);
    } else {
        out.timestamp_us = read_u32_le(p+12);
        out.timestamp_ms = out.timestamp_us / 1000u;
        out.status24 = (static_cast<std::uint32_t>(p[16]) << 16) |
                       (static_cast<std::uint32_t>(p[17]) << 8) | p[18];
        out.status24_secondary = (static_cast<std::uint32_t>(p[19]) << 16) |
                                 (static_cast<std::uint32_t>(p[20]) << 8) | p[21];
        for (std::size_t i=0; i<16; ++i) out.channel[i] = sign_extend_24(p+22+i*3);
    }
    return true;
}

std::vector<SamplePacket> StreamParser::feed(const std::uint8_t *data,
                                             std::size_t length) {
    std::vector<SamplePacket> packets;
    if (data == nullptr && length != 0) return packets;
    if (length == 0) return packets;
    buffer_.insert(buffer_.end(), data, data + length);
    for (;;) {
        auto it = std::search(buffer_.begin(), buffer_.end(),
                              std::begin("HBCI") , std::begin("HBCI") + 4);
        if (it == buffer_.end()) {
            if (buffer_.size() > 3) buffer_.erase(buffer_.begin(), buffer_.end()-3);
            break;
        }
        if (it != buffer_.begin()) buffer_.erase(buffer_.begin(), it);
        if (buffer_.size() < 8u) break;
        const std::size_t packet_size = read_u16_le(buffer_.data() + 6);
        if (packet_size != kPacketBytes && packet_size != kDualPacketBytes) {
            ++rejected_frames_;
            buffer_.erase(buffer_.begin());
            continue;
        }
        if (buffer_.size() < packet_size) break;
        SamplePacket packet;
        if (decode_packet(buffer_.data(), packet_size, packet)) {
            packets.push_back(packet);
            buffer_.erase(buffer_.begin(), buffer_.begin()+static_cast<std::ptrdiff_t>(packet_size));
        } else {
            ++rejected_frames_;
            buffer_.erase(buffer_.begin());
        }
    }
    return packets;
}

bool SampleRateMonitor::observe(const SamplePacket &sample) noexcept {
    if (sample.version != HB_PACKET_DUAL_VERSION || sample.channel_count != 16u)
        return false;
    if (have_first_ && sample.sequence != previous_sequence_ + 1u)
        sequence_gaps_ += static_cast<std::uint32_t>(sample.sequence - previous_sequence_ - 1u);
    if (!have_first_) {
        first_time_us_ = sample.timestamp_us;
        have_first_ = true;
    }
    last_time_us_ = sample.timestamp_us;
    previous_sequence_ = sample.sequence;
    ++sample_count_;
    return true;
}

double SampleRateMonitor::measured_hz() const noexcept {
    if (sample_count_ < 2u) return 0.0;
    const std::uint32_t elapsed_us = last_time_us_ - first_time_us_;
    if (elapsed_us == 0u) return 0.0;
    return static_cast<double>(sample_count_ - 1u) * 1000000.0 /
           static_cast<double>(elapsed_us);
}
}
