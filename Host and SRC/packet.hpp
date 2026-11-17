#ifndef HB_HOST_PACKET_HPP
#define HB_HOST_PACKET_HPP
#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace hb {
constexpr std::size_t kPacketBytes = 45;
constexpr std::size_t kDualPacketBytes = 72;
struct SamplePacket {
    std::uint8_t version{};
    std::uint8_t channel_count{};
    std::uint32_t sequence{};
    std::uint32_t timestamp_ms{};
    std::uint32_t timestamp_us{};
    std::uint32_t status24{};
    std::uint32_t status24_secondary{};
    std::array<std::int32_t, 16> channel{};
};
std::uint16_t crc16_ccitt(const std::uint8_t *data, std::size_t length);
bool decode_packet(const std::uint8_t *data, std::size_t length, SamplePacket &out);

/* Incremental parser for USB/serial/file byte streams. It seeks HBCI framing,
   rejects bad version/length/CRC, and resynchronizes after corruption. */
class StreamParser {
public:
    std::vector<SamplePacket> feed(const std::uint8_t *data, std::size_t length);
    std::uint64_t rejected_frames() const noexcept { return rejected_frames_; }
private:
    std::vector<std::uint8_t> buffer_;
    std::uint64_t rejected_frames_{0};
};

class SampleRateMonitor {
public:
    bool observe(const SamplePacket &sample) noexcept;
    std::uint64_t sample_count() const noexcept { return sample_count_; }
    std::uint64_t sequence_gaps() const noexcept { return sequence_gaps_; }
    double measured_hz() const noexcept;
private:
    bool have_first_{false};
    std::uint32_t first_time_us_{0};
    std::uint32_t last_time_us_{0};
    std::uint32_t previous_sequence_{0};
    std::uint64_t sample_count_{0};
    std::uint64_t sequence_gaps_{0};
};
}
#endif
