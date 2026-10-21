#pragma once

// INVERSO EQ: portable DSP engine; C++17, no dependencies.
// Sample peak protection is NOT a true-peak limiter.

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <vector>

namespace inverso {

constexpr int kBands = 8;
constexpr int kMaxSections = 4;
constexpr double kPi = 3.14159265358979323846;

inline double dbToLinear(double db) noexcept { return std::pow(10.0, db / 20.0); }
inline double linearToDb(double level) noexcept { return 20.0 * std::log10(std::max(level, 1.0e-12)); }

struct BandSettings {
    bool enabled = false;
    double frequency = 1000.0;
    double gainDb = 0.0;
    double q = 1.0;
};

struct Settings {
    bool highPassEnabled = false;
    double highPassHz = 35.0;
    int highPassSlope = 24;
    bool lowPassEnabled = false;
    double lowPassHz = 18000.0;
    int lowPassSlope = 12;
    std::array<BandSettings, kBands> bands {};
    double outputGainDb = 0.0;
    bool protectionEnabled = true;
    double protectionCeilingDb = -1.0; // sample-peak dBFS
};

struct BlockMeter {
    float inputPeak = 0.0f;
    float preProtectionPeak = 0.0f;
    float outputPeak = 0.0f;
    float maximumReductionDb = 0.0f;
};

struct Coefficients {
    double b0 = 1., b1 = 0., b2 = 0., a1 = 0., a2 = 0.;

    static Coefficients make(double sampleRate, double frequency, double q, double gainDb, int type) noexcept {
        const double freq = std::clamp(frequency, 10.0, sampleRate * 0.49);
        const double w0 = 2.0 * kPi * freq / sampleRate;
        const double cs = std::cos(w0), sn = std::sin(w0);
        const double alpha = sn / (2.0 * std::max(q, 0.05));
        const double A = std::pow(10.0, gainDb / 40.0);
        double b0 = 1., b1 = 0., b2 = 0., a0 = 1., a1 = 0., a2 = 0.;
        if (type == 0) { // peak (RBJ audio EQ cookbook)
            b0 = 1.0 + alpha * A; b1 = -2.0 * cs; b2 = 1.0 - alpha * A;
            a0 = 1.0 + alpha / A; a1 = -2.0 * cs; a2 = 1.0 - alpha / A;
        } else if (type == 1) { // high-pass
            b0 = (1.0 + cs) * 0.5; b1 = -(1.0 + cs); b2 = b0;
            a0 = 1.0 + alpha; a1 = -2.0 * cs; a2 = 1.0 - alpha;
        } else { // low-pass
            b0 = (1.0 - cs) * 0.5; b1 = 1.0 - cs; b2 = b0;
            a0 = 1.0 + alpha; a1 = -2.0 * cs; a2 = 1.0 - alpha;
        }
        return { b0 / a0, b1 / a0, b2 / a0, a1 / a0, a2 / a0 };
    }

    double magnitude(double normalizedRad) const noexcept {
        const double c1 = std::cos(normalizedRad), s1 = std::sin(normalizedRad);
        const double c2 = std::cos(2.0 * normalizedRad), s2 = std::sin(2.0 * normalizedRad);
        const double nr = b0 + b1 * c1 + b2 * c2, ni = -(b1 * s1 + b2 * s2);
        const double dr = 1.0 + a1 * c1 + a2 * c2, di = -(a1 * s1 + a2 * s2);
        return std::sqrt((nr * nr + ni * ni) / std::max(dr * dr + di * di, 1e-24));
    }
};

inline int sectionCount(int slope) noexcept { return std::clamp(slope / 12, 1, kMaxSections); }
inline double butterworthQ(int numberOfSections, int index) noexcept {
    const int order = 2 * numberOfSections;
    return 1.0 / (2.0 * std::cos(kPi * (2.0 * index + 1.0) / (2.0 * order)));
}

// Approximate magnitude response of the exact biquad coefficients used in processing.
inline double responseDb(const Settings& settings, double sampleRate, double hz) noexcept {
    if (sampleRate <= 0) return 0.0;
    const double w = 2.0 * kPi * std::clamp(hz, 0.0, sampleRate * 0.499) / sampleRate;
    double magnitude = dbToLinear(settings.outputGainDb);
    if (settings.highPassEnabled) {
        const auto n = sectionCount(settings.highPassSlope);
        for (int i = 0; i < n; ++i)
            magnitude *= Coefficients::make(sampleRate, settings.highPassHz, butterworthQ(n, i), 0.0, 1).magnitude(w);
    }
    for (const auto& b : settings.bands)
        if (b.enabled) magnitude *= Coefficients::make(sampleRate, b.frequency, b.q, b.gainDb, 0).magnitude(w);
    if (settings.lowPassEnabled) {
        const auto n = sectionCount(settings.lowPassSlope);
        for (int i = 0; i < n; ++i)
            magnitude *= Coefficients::make(sampleRate, settings.lowPassHz, butterworthQ(n, i), 0.0, 2).magnitude(w);
    }
    return std::clamp(linearToDb(magnitude), -144.0, 48.0);
}

class Biquad {
public:
    void set(Coefficients coefficients) noexcept { coeff = coefficients; }
    void reset() noexcept { z1 = z2 = 0.0; }
    float process(float in) noexcept {
        const double x = static_cast<double>(in);
        const double y = coeff.b0 * x + z1;
        z1 = coeff.b1 * x - coeff.a1 * y + z2;
        z2 = coeff.b2 * x - coeff.a2 * y;
        // Keep state away from subnormal denormals on CPUs.
        if (std::abs(z1) < 1e-24) z1 = 0.;
        if (std::abs(z2) < 1e-24) z2 = 0.;
        return static_cast<float>(y);
    }
private:
    Coefficients coeff {};
    double z1 = 0., z2 = 0.;
};

class EQCore {
public:
    void prepare(double newSampleRate, int newChannels) {
        sampleRate = std::max(8000.0, newSampleRate);
        channels.resize(static_cast<std::size_t>(std::max(1, newChannels)));
        reset();
        updateCoefficients();
    }

    void reset() noexcept {
        for (auto& c : channels) {
            for (auto& f : c.hp) f.reset();
            for (auto& f : c.bands) f.reset();
            for (auto& f : c.lp) f.reset();
        }
        protectionGain = 1.0;
    }

    void setSettings(const Settings& next) noexcept {
        bool coefficientsChanged = next.highPassEnabled != settings.highPassEnabled
            || next.lowPassEnabled != settings.lowPassEnabled
            || next.highPassHz != settings.highPassHz
            || next.lowPassHz != settings.lowPassHz
            || next.highPassSlope != settings.highPassSlope
            || next.lowPassSlope != settings.lowPassSlope;
        for (int i = 0; i < kBands; ++i) {
            const auto& a = settings.bands[static_cast<std::size_t>(i)];
            const auto& b = next.bands[static_cast<std::size_t>(i)];
            coefficientsChanged |= a.enabled != b.enabled || a.frequency != b.frequency
                                || a.gainDb != b.gainDb || a.q != b.q;
        }
        settings = next;
        if (coefficientsChanged) updateCoefficients();
    }

    const Settings& getSettings() const noexcept { return settings; }

    BlockMeter process(float* const* buffer, int numberOfChannels, int numberOfSamples) noexcept {
        BlockMeter meter;
        if (!buffer || numberOfSamples <= 0 || numberOfChannels <= 0 || channels.empty()) return meter;
        const int count = std::min(numberOfChannels, static_cast<int>(channels.size()));
        const double outGain = dbToLinear(settings.outputGainDb);
        const double ceiling = dbToLinear(settings.protectionCeilingDb);
        const double release = std::exp(-1.0 / (sampleRate * 0.120));
        for (int n = 0; n < numberOfSamples; ++n) {
            float prePeak = 0.0f;
            for (int ch = 0; ch < count; ++ch) {
                float x = buffer[ch][n];
                meter.inputPeak = std::max(meter.inputPeak, std::abs(x));
                auto& state = channels[static_cast<std::size_t>(ch)];
                if (settings.highPassEnabled)
                    for (int s = 0; s < sectionCount(settings.highPassSlope); ++s) x = state.hp[static_cast<std::size_t>(s)].process(x);
                for (int s = 0; s < kBands; ++s)
                    if (settings.bands[static_cast<std::size_t>(s)].enabled) x = state.bands[static_cast<std::size_t>(s)].process(x);
                if (settings.lowPassEnabled)
                    for (int s = 0; s < sectionCount(settings.lowPassSlope); ++s) x = state.lp[static_cast<std::size_t>(s)].process(x);
                buffer[ch][n] = static_cast<float>(x * outGain);
                prePeak = std::max(prePeak, std::abs(buffer[ch][n]));
            }
            meter.preProtectionPeak = std::max(meter.preProtectionPeak, prePeak);
            if (settings.protectionEnabled) {
                // Linked L/R instantaneous sample-peak ceiling with slow release.
                // This is a deliberately simple safety guard, not a transparent lookahead
                // or inter-sample true peak limiter. Strong limiting may be audible.
                protectionGain = std::min(1.0, protectionGain * release + (1.0 - release));
                if (prePeak > 0.0f)
                    protectionGain = std::min(protectionGain, ceiling / static_cast<double>(prePeak));
            } else {
                protectionGain = 1.0;
            }
            meter.maximumReductionDb = std::max(meter.maximumReductionDb,
                                                 static_cast<float>(-linearToDb(protectionGain)));
            for (int ch = 0; ch < count; ++ch) {
                buffer[ch][n] = static_cast<float>(buffer[ch][n] * protectionGain);
                meter.outputPeak = std::max(meter.outputPeak, std::abs(buffer[ch][n]));
            }
        }
        return meter;
    }

private:
    struct Channel {
        std::array<Biquad, kMaxSections> hp {};
        std::array<Biquad, kBands> bands {};
        std::array<Biquad, kMaxSections> lp {};
    };

    void updateCoefficients() noexcept {
        for (auto& ch : channels) {
            const int hpN = sectionCount(settings.highPassSlope);
            const int lpN = sectionCount(settings.lowPassSlope);
            for (int i = 0; i < hpN; ++i)
                ch.hp[static_cast<std::size_t>(i)].set(Coefficients::make(sampleRate, settings.highPassHz, butterworthQ(hpN, i), 0., 1));
            for (int i = 0; i < lpN; ++i)
                ch.lp[static_cast<std::size_t>(i)].set(Coefficients::make(sampleRate, settings.lowPassHz, butterworthQ(lpN, i), 0., 2));
            for (int i = 0; i < kBands; ++i) {
                const auto& b = settings.bands[static_cast<std::size_t>(i)];
                ch.bands[static_cast<std::size_t>(i)].set(Coefficients::make(sampleRate, b.frequency, b.q, b.gainDb, 0));
            }
        }
    }

    double sampleRate = 48000.;
    double protectionGain = 1.0;
    Settings settings {};
    std::vector<Channel> channels;
};

} // namespace inverso
