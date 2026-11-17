#ifndef HB_HOST_DSP_HPP
#define HB_HOST_DSP_HPP

#include <cstddef>
#include <cstdint>
#include <vector>

namespace hb::dsp {

/* Normalized floating-point biquad. Coefficients use a0=1 form. */
class Biquad {
public:
    static Biquad lowpass(double sample_rate_hz, double cutoff_hz, double q);
    static Biquad notch(double sample_rate_hz, double center_hz, double q);
    float process(float input) noexcept;
    void reset() noexcept;
private:
    double b0_{1.0}, b1_{0.0}, b2_{0.0}, a1_{0.0}, a2_{0.0};
    double z1_{0.0}, z2_{0.0};
};

/* Odd-length Blackman-windowed low-pass FIR followed by integer decimation.
   The cutoff is selected below the new Nyquist frequency. */
class FirDecimator {
public:
    FirDecimator(std::size_t factor, std::size_t tap_count = 63);
    bool process(float input, float &output) noexcept;
    std::size_t factor() const noexcept { return factor_; }
    void reset() noexcept;
private:
    std::size_t factor_;
    std::size_t phase_{0};
    std::size_t cursor_{0};
    std::vector<double> coefficients_;
    std::vector<double> history_;
};

/* In-place radix-2 FFT. Returns false for mismatched arrays or non-power-of-two N. */
bool fft(std::vector<double> &real, std::vector<double> &imag) noexcept;

struct TimeFeatures {
    double mean{};
    double rms{};
    double peak_to_peak{};
    double line_length{};
};
struct ArtifactLimits {
    float absolute_limit{500000.0F};
    float max_step{250000.0F};
};
TimeFeatures time_features(const float *samples, std::size_t count) noexcept;
double band_power(const std::vector<double> &real, const std::vector<double> &imag,
                  double sample_rate_hz, double low_hz, double high_hz) noexcept;

struct FeatureVector {
    TimeFeatures time;
    double band_power_1_4{};
    double band_power_4_8{};
    double band_power_8_13{};
    double band_power_13_30{};
    double band_power_30_80{};
    bool artifact{};
};
bool extract_feature_vector(const float *samples, std::size_t count, double sample_rate_hz,
                            ArtifactLimits limits, FeatureVector &out) noexcept;

/* Stable boundary for a future classifier implementation. No trained model is
   bundled; callers must version and validate their own model and feature units. */
class ClassifierInterface {
public:
    virtual ~ClassifierInterface() = default;
    virtual int classify(const FeatureVector &features) noexcept = 0;
};

class ArtifactDetector {
public:
    explicit ArtifactDetector(ArtifactLimits limits = {}) noexcept : limits_(limits) {}
    bool observe(float sample) noexcept;
    void reset() noexcept { have_previous_ = false; previous_ = 0.0F; }
private:
    ArtifactLimits limits_;
    float previous_{0.0F};
    bool have_previous_{false};
};

} // namespace hb::dsp
#endif
