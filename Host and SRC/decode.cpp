#include "hb/packet.hpp"
#include <array>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <limits>
#include <string>

int main(int argc, char **argv) {
    if (argc != 3) {
        std::cerr << "Usage: hb_decode <input.hbci> <output.csv>\n";
        return 2;
    }
    std::ifstream input(argv[1], std::ios::binary);
    if (!input) { std::cerr << "Cannot open input: " << argv[1] << "\n"; return 2; }
    std::ofstream output(argv[2], std::ios::out | std::ios::trunc);
    if (!output) { std::cerr << "Cannot open output: " << argv[2] << "\n"; return 2; }
    output << "version,sequence,timestamp_us,channel_count,status24_afe1,status24_afe2";
    for (unsigned i = 1; i <= 16; ++i) output << ",CH" << i;
    output << '\n';

    hb::StreamParser parser;
    std::array<char, 4096> bytes{};
    std::uint64_t samples = 0, gaps = 0;
    std::uint32_t previous = 0;
    bool have_previous = false;
    hb::SampleRateMonitor rate_monitor;
    while (input) {
        input.read(bytes.data(), static_cast<std::streamsize>(bytes.size()));
        const std::streamsize count = input.gcount();
        if (count <= 0) break;
        const auto *data = reinterpret_cast<const std::uint8_t *>(bytes.data());
        for (const auto &sample : parser.feed(data, static_cast<std::size_t>(count))) {
            if (have_previous && sample.sequence != previous + 1u) ++gaps;
            previous = sample.sequence;
            have_previous = true;
            (void)rate_monitor.observe(sample);
            output << static_cast<unsigned>(sample.version) << ',' << sample.sequence << ','
                   << sample.timestamp_us << ',' << static_cast<unsigned>(sample.channel_count)
                   << ',' << sample.status24 << ',' << sample.status24_secondary;
            for (std::size_t i = 0; i < sample.channel.size(); ++i) {
                if (i < sample.channel_count) output << ',' << sample.channel[i];
                else output << ',';
            }
            output << '\n';
            ++samples;
        }
    }
    output.flush();
    if (!output) { std::cerr << "Error writing CSV output\n"; return 3; }
    std::cerr << "Decoded samples: " << samples
              << "; sequence gaps: " << gaps
              << "; rejected/corrupt frames: " << parser.rejected_frames() << "\n";
    if (rate_monitor.sample_count() > 1u) {
        std::cerr << "Dual-ADC rate from device timestamps: " << rate_monitor.measured_hz()
                  << " samples/s; missing sequence values: "
                  << rate_monitor.sequence_gaps() << "\n";
    } else {
        std::cerr << "Dual-ADC sample-rate estimate unavailable; capture v2 dual-ADC packets.\n";
    }
    return samples == 0 ? 1 : 0;
}
