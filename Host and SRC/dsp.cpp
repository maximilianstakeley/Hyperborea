#include "hb/dsp.hpp"

#include <algorithm>
#include <cmath>
#include <limits>

namespace hb::dsp {
namespace {
constexpr double kPi = 3.141592653589793238462643383279502884;
bool valid_frequency(double sr, double f) noexcept {
    return std::isfinite(sr) && std::isfinite(f) && sr > 0.0 && f > 0.0 && f < sr * 0.5;
}
}

Biquad Biquad::lowpass(double sr, double cutoff, double q) {
    Biquad f;
    if (!valid_frequency(sr, cutoff) || !std::isfinite(q) || q <= 0.0) return f;
    const double w = 2.0 * kPi * cutoff / sr;
    const double c = std::cos(w), alpha = std::sin(w) / (2.0 * q);
    const double a0 = 1.0 + alpha;
    f.b0_ = ((1.0 - c) * 0.5) / a0;
    f.b1_ = (1.0 - c) / a0;
    f.b2_ = f.b0_;
    f.a1_ = (-2.0 * c) / a0;
    f.a2_ = (1.0 - alpha) / a0;
    return f;
}

Biquad Biquad::notch(double sr, double center, double q) {
    Biquad f;
    if (!valid_frequency(sr, center) || !std::isfinite(q) || q <= 0.0) return f;
    const double w = 2.0 * kPi * center / sr;
    const double c = std::cos(w), alpha = std::sin(w) / (2.0 * q);
    const double a0 = 1.0 + alpha;
    f.b0_ = 1.0 / a0;
    f.b1_ = (-2.0 * c) / a0;
    f.b2_ = 1.0 / a0;
    f.a1_ = f.b1_;
    f.a2_ = (1.0 - alpha) / a0;
    return f;
}

float Biquad::process(float x) noexcept {
    const double y = b0_ * static_cast<double>(x) + z1_;
    z1_ = b1_ * static_cast<double>(x) - a1_ * y + z2_;
    z2_ = b2_ * static_cast<double>(x) - a2_ * y;
    return static_cast<float>(y);
}
void Biquad::reset() noexcept { z1_ = 0.0; z2_ = 0.0; }

FirDecimator::FirDecimator(std::size_t factor, std::size_t taps)
    : factor_(std::max<std::size_t>(1U, factor)), coefficients_(taps), history_(taps, 0.0) {
    if (taps < 3U) {
        coefficients_ = {1.0};
        history_ = {0.0};
        return;
    }
    if ((taps % 2U) == 0U) ++taps;
    coefficients_.resize(taps);
    history_.assign(taps, 0.0);
    const double fc = 0.45 / static_cast<double>(factor_);
    const double mid = static_cast<double>(taps - 1U) * 0.5;
    double sum = 0.0;
    for (std::size_t i = 0; i < taps; ++i) {
        const double n = static_cast<double>(i) - mid;
        const double sinc = std::abs(n) < 1e-12 ? 2.0 * fc : std::sin(2.0 * kPi * fc * n) / (kPi * n);
        const double phase = 2.0 * kPi * static_cast<double>(i) / static_cast<double>(taps - 1U);
        const double window = 0.42 - 0.5 * std::cos(phase) + 0.08 * std::cos(2.0 * phase);
        coefficients_[i] = sinc * window;
        sum += coefficients_[i];
    }
    if (std::abs(sum) > std::numeric_limits<double>::epsilon())
        for (double &c : coefficients_) c /= sum;
}

bool FirDecimator::process(float input, float &output) noexcept {
    if (history_.empty()) return false;
    history_[cursor_] = static_cast<double>(input);
    cursor_ = (cursor_ + 1U) % history_.size();
    phase_ = (phase_ + 1U) % factor_;
    if (phase_ != 0U) return false;
    double y = 0.0;
    std::size_t idx = cursor_;
    for (std::size_t i = 0; i < history_.size(); ++i) {
        idx = (idx == 0U) ? history_.size() - 1U : idx - 1U;
        y += coefficients_[i] * history_[idx];
    }
    output = static_cast<float>(y);
    return true;
}
void FirDecimator::reset() noexcept {
    std::fill(history_.begin(), history_.end(), 0.0);
    phase_ = 0U;
    cursor_ = 0U;
}

bool fft(std::vector<double> &re, std::vector<double> &im) noexcept {
    const std::size_t n = re.size();
    if (n < 2U || im.size() != n || (n & (n - 1U)) != 0U) return false;
    for (std::size_t i = 1U, j = 0U; i < n; ++i) {
        std::size_t bit = n >> 1U;
        for (; (j & bit) != 0U; bit >>= 1U) j ^= bit;
        j ^= bit;
        if (i < j) { std::swap(re[i], re[j]); std::swap(im[i], im[j]); }
    }
    for (std::size_t len = 2U; len <= n; len <<= 1U) {
        const double angle = -2.0 * kPi / static_cast<double>(len);
        const double wlen_r = std::cos(angle), wlen_i = std::sin(angle);
        for (std::size_t base = 0U; base < n; base += len) {
            double wr = 1.0, wi = 0.0;
            for (std::size_t j = 0U; j < len / 2U; ++j) {
                const std::size_t u = base + j, v = u + len / 2U;
                const double vr = re[v] * wr - im[v] * wi;
                const double vi = re[v] * wi + im[v] * wr;
                re[v] = re[u] - vr; im[v] = im[u] - vi;
                re[u] += vr; im[u] += vi;
                const double next_wr = wr * wlen_r - wi * wlen_i;
                wi = wr * wlen_i + wi * wlen_r; wr = next_wr;
            }
        }
        if (len == n) break; // avoid size_t overflow on the final shift
    }
    return true;
}

TimeFeatures time_features(const float *x, std::size_t n) noexcept {
    TimeFeatures f{};
    if (x == nullptr || n == 0U) return f;
    double sum = 0.0, squares = 0.0, length = 0.0;
    float lo = x[0], hi = x[0];
    for (std::size_t i = 0U; i < n; ++i) {
        const double v = x[i]; sum += v; squares += v * v;
        lo = std::min(lo, x[i]); hi = std::max(hi, x[i]);
        if (i != 0U) length += std::abs(v - static_cast<double>(x[i - 1U]));
    }
    f.mean = sum / static_cast<double>(n);
    f.rms = std::sqrt(squares / static_cast<double>(n));
    f.peak_to_peak = static_cast<double>(hi) - static_cast<double>(lo);
    f.line_length = length;
    return f;
}

double band_power(const std::vector<double> &re, const std::vector<double> &im,
                  double sr, double low, double high) noexcept {
    if (re.size() < 2U || re.size() != im.size() || !std::isfinite(sr) || sr <= 0.0 ||
        !std::isfinite(low) || !std::isfinite(high) || low < 0.0 || high <= low || low >= sr * 0.5) return 0.0;
    const std::size_t n = re.size();
    const double bin_hz = sr / static_cast<double>(n);
    const double bounded_high = std::min(high, sr * 0.5);
    const std::size_t first = static_cast<std::size_t>(std::ceil(low / bin_hz));
    const std::size_t last = static_cast<std::size_t>(std::floor(bounded_high / bin_hz));
    double power = 0.0;
    if (last < first) return 0.0;
    for (std::size_t k = first; k <= last; ++k) power += re[k] * re[k] + im[k] * im[k];
    return power / (static_cast<double>(n) * static_cast<double>(n));
}

bool extract_feature_vector(const float *samples, std::size_t count, double sr,
                            ArtifactLimits limits, FeatureVector &out) noexcept {
    out = {};
    if (samples == nullptr || count < 4U || (count & (count - 1U)) != 0U ||
        !std::isfinite(sr) || sr <= 0.0) return false;
    out.time = time_features(samples, count);
    ArtifactDetector detector(limits);
    std::vector<double> re(count), im(count, 0.0);
    for (std::size_t i = 0U; i < count; ++i) {
        out.artifact = detector.observe(samples[i]) || out.artifact;
        const double window = (count > 1U) ?
            0.5 - 0.5 * std::cos(2.0 * kPi * static_cast<double>(i) / static_cast<double>(count - 1U)) : 1.0;
        re[i] = (static_cast<double>(samples[i]) - out.time.mean) * window;
    }
    if (!fft(re, im)) return false;
    const double max_hz = sr * 0.5;
    const auto band = [&](double lo, double hi) noexcept {
        if (lo >= max_hz) return 0.0;
        return band_power(re, im, sr, lo, std::min(hi, max_hz));
    };
    out.band_power_1_4 = band(1.0, 4.0);
    out.band_power_4_8 = band(4.0, 8.0);
    out.band_power_8_13 = band(8.0, 13.0);
    out.band_power_13_30 = band(13.0, 30.0);
    out.band_power_30_80 = band(30.0, 80.0);
    return true;
}

bool ArtifactDetector::observe(float x) noexcept {
    const bool flagged = !std::isfinite(x) || std::abs(x) > limits_.absolute_limit ||
        (have_previous_ && std::abs(x - previous_) > limits_.max_step);
    previous_ = x;
    have_previous_ = true;
    return flagged;
}
} // namespace hb::dsp
