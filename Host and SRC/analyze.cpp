#include "hb/dsp.hpp"
#include "hb/packet.hpp"

#include <algorithm>
#include <array>
#include <cerrno>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

namespace {
bool parse_number(const char *text, double &value) {
    char *end = nullptr;
    errno = 0;
    value = std::strtod(text, &end);
    return errno == 0 && end != text && *end == '\0' && std::isfinite(value);
}
}

int main(int argc, char **argv) {
    if (argc < 5 || argc > 6) {
        std::cerr << "Usage: hb_analyze <input.hbci> <features.csv> <sample_rate_hz> <notch_hz:50|60> [window_power_of_two]\n";
        return 2;
    }
    double sample_rate = 0.0, notch_hz = 0.0;
    if (!parse_number(argv[3], sample_rate) || !parse_number(argv[4], notch_hz) ||
        sample_rate <= 0.0 || (notch_hz != 50.0 && notch_hz != 60.0) || notch_hz >= sample_rate * 0.5) {
        std::cerr << "Invalid sample rate or notch frequency\n";
        return 2;
    }
    std::size_t window_size = 256U;
    if (argc == 6) {
        char *end = nullptr;
        const unsigned long parsed = std::strtoul(argv[5], &end, 10);
        if (end == argv[5] || *end != '\0' || parsed < 16UL || (parsed & (parsed - 1UL)) != 0UL) {
            std::cerr << "Window must be a power of two and at least 16 samples\n";
            return 2;
        }
        window_size = static_cast<std::size_t>(parsed);
    }

    std::ifstream input(argv[1], std::ios::binary);
    std::ofstream output(argv[2], std::ios::out | std::ios::trunc);
    if (!input || !output) {
        std::cerr << "Cannot open input or output file\n";
        return 2;
    }
    output << "sequence_end,timestamp_us,channel,window_samples,mean_counts,rms_counts,peak_to_peak_counts,line_length_counts,"
              "power_1_4,power_4_8,power_8_13,power_13_30,power_30_80,artifact\n";

    std::array<hb::dsp::Biquad, 16> notch{};
    std::array<hb::dsp::Biquad, 16> lowpass{};
    for (std::size_t ch = 0; ch < notch.size(); ++ch) {
        notch[ch] = hb::dsp::Biquad::notch(sample_rate, notch_hz, 30.0);
        lowpass[ch] = hb::dsp::Biquad::lowpass(sample_rate, std::min(100.0, sample_rate * 0.4), 0.70710678);
    }
    std::array<std::vector<float>, 16> frames;
    for (auto &frame : frames) frame.reserve(window_size);
    hb::StreamParser parser;
    std::array<char, 4096> bytes{};
    std::uint64_t decoded_samples = 0U, feature_windows = 0U;
    while (input) {
        input.read(bytes.data(), static_cast<std::streamsize>(bytes.size()));
        const auto count = input.gcount();
        if (count <= 0) break;
        const auto *data = reinterpret_cast<const std::uint8_t *>(bytes.data());
        for (const auto &sample : parser.feed(data, static_cast<std::size_t>(count))) {
            ++decoded_samples;
            for (std::size_t ch = 0; ch < sample.channel_count; ++ch)
                frames[ch].push_back(lowpass[ch].process(notch[ch].process(static_cast<float>(sample.channel[ch]))));
            const bool ready = sample.channel_count != 0U && frames[sample.channel_count - 1U].size() == window_size;
            if (!ready) continue;
            for (std::size_t ch = 0; ch < sample.channel_count; ++ch) {
                hb::dsp::FeatureVector f;
                if (!hb::dsp::extract_feature_vector(frames[ch].data(), frames[ch].size(), sample_rate,
                                                     {500000.0F, 250000.0F}, f)) {
                    std::cerr << "Feature extraction failed; check sample rate/window size\n";
                    return 3;
                }
                output << sample.sequence << ',' << sample.timestamp_us << ',' << (ch + 1U) << ',' << window_size << ','
                       << f.time.mean << ',' << f.time.rms << ',' << f.time.peak_to_peak << ',' << f.time.line_length << ','
                       << f.band_power_1_4 << ',' << f.band_power_4_8 << ',' << f.band_power_8_13 << ','
                       << f.band_power_13_30 << ',' << f.band_power_30_80 << ',' << (f.artifact ? 1 : 0) << '\n';
                frames[ch].clear();
            }
            ++feature_windows;
        }
    }
    output.flush();
    if (!output) { std::cerr << "Error writing feature CSV\n"; return 3; }
    std::cerr << "Decoded samples: " << decoded_samples << "; complete feature windows: " << feature_windows
              << "; corrupt/rejected packets: " << parser.rejected_frames() << "\n";
    return decoded_samples == 0U ? 1 : 0;
}
