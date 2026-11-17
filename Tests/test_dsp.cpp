#include "hb/dsp.hpp"
#include <cassert>
#include <cmath>
#include <cstddef>
#include <limits>
#include <vector>

int main() {
    constexpr double pi = 3.14159265358979323846;
    constexpr double fs = 1000.0;

    auto notch = hb::dsp::Biquad::notch(fs, 60.0, 30.0);
    double sum_in = 0.0, sum_out = 0.0;
    for (std::size_t n = 0; n < 10000; ++n) {
        const float x = static_cast<float>(std::sin(2.0 * pi * 60.0 * static_cast<double>(n) / fs));
        const float y = notch.process(x);
        if (n >= 1000) { sum_in += static_cast<double>(x) * x; sum_out += static_cast<double>(y) * y; }
    }
    assert(sum_out / sum_in < 0.01);

    hb::dsp::FirDecimator decimator(4U, 63U);
    std::size_t outputs = 0U;
    float y = 0.0F;
    for (std::size_t n = 0; n < 1000U; ++n) if (decimator.process(1.0F, y)) ++outputs;
    assert(outputs == 250U);
    assert(std::abs(y - 1.0F) < 0.02F);

    std::vector<double> re(1024U), im(1024U, 0.0);
    for (std::size_t n = 0; n < re.size(); ++n)
        re[n] = std::sin(2.0 * pi * 64.0 * static_cast<double>(n) / 1024.0);
    assert(hb::dsp::fft(re, im));
    assert(hb::dsp::band_power(re, im, 1024.0, 63.0, 65.0) > 0.12);
    std::vector<float> window(1024U);
    for (std::size_t n = 0; n < window.size(); ++n)
        window[n] = static_cast<float>(std::sin(2.0 * pi * 10.0 * static_cast<double>(n) / fs));
    hb::dsp::FeatureVector vector;
    assert(hb::dsp::extract_feature_vector(window.data(), window.size(), fs, {5.0F, 2.0F}, vector));
    assert(vector.time.rms > 0.6 && vector.band_power_8_13 > 0.01);
    assert(hb::dsp::band_power(re, im, 1024.0, 1e9, 1e10) == 0.0);
    float non_power_of_two[6] = {0.0F, 1.0F, 0.0F, -1.0F, 0.0F, 1.0F};
    assert(!hb::dsp::extract_feature_vector(non_power_of_two, 6U, fs, {}, vector));
    std::vector<double> invalid(3U, 0.0);
    assert(!hb::dsp::fft(invalid, invalid));

    const float signal[] = {-1.0F, 0.0F, 1.0F, 0.0F};
    const auto features = hb::dsp::time_features(signal, 4U);
    assert(std::abs(features.mean) < 1e-12);
    assert(std::abs(features.rms - std::sqrt(0.5)) < 1e-6);
    assert(std::abs(features.peak_to_peak - 2.0) < 1e-12);
    assert(std::abs(features.line_length - 3.0) < 1e-12);

    hb::dsp::ArtifactDetector detector({10.0F, 4.0F});
    assert(!detector.observe(1.0F));
    assert(detector.observe(7.0F));
    assert(detector.observe(std::numeric_limits<float>::quiet_NaN()));
    return 0;
}
