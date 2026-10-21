#include "../Source/DSP/EQCore.h"
#include <stdexcept>
#define REQUIRE(condition) do { if (!(condition)) throw std::runtime_error("Test failed: " #condition); } while (false)
#include <cmath>
#include <iostream>
#include <vector>

constexpr int sr = 48000;
constexpr int samples = sr;

float measureSine(double freq, inverso::Settings settings) {
    inverso::EQCore eq;
    eq.prepare(sr, 2);
    eq.setSettings(settings);
    std::vector<float> l(samples), r(samples);
    float* data[2] = {l.data(), r.data()};
    for (int n=0; n<samples; ++n) {
        const float v = 0.1f * static_cast<float>(std::sin(2.0 * inverso::kPi * freq * n / sr));
        l[n]=r[n]=v;
    }
    eq.process(data, 2, samples);
    double power = 0.;
    for (int n=samples/2; n<samples; ++n) power += l[n]*l[n];
    return static_cast<float>(std::sqrt(power / (samples/2)));
}

int main() {
    using namespace inverso;
    const float baseline = measureSine(1000, {});
    REQUIRE(std::abs(baseline - 0.1 / std::sqrt(2.)) < 0.0005);

    Settings s;
    s.highPassEnabled = true;
    s.highPassHz = 200.;
    s.highPassSlope = 24;
    REQUIRE(measureSine(40, s) < measureSine(1000, s) * 0.02f);

    s = {};
    s.lowPassEnabled = true;
    s.lowPassHz = 1000.;
    s.lowPassSlope = 24;
    REQUIRE(measureSine(9000, s) < measureSine(150, s) * 0.01f);

    s = {};
    s.bands[0] = {true, 1000., +6., 1.};
    const float boosted = measureSine(1000, s);
    REQUIRE(boosted / baseline > 1.95f && boosted / baseline < 2.05f);

    s = {};
    s.outputGainDb = +18.;
    EQCore eq;
    eq.prepare(sr, 2);
    eq.setSettings(s);
    std::vector<float> l(3000), r(3000);
    float* data[2] = {l.data(), r.data()};
    for (int n=0; n<3000; ++n) l[n] = r[n] = (n % 2 == 0 ? 0.85f : -0.85f);
    const auto meter = eq.process(data, 2, 3000);
    const float ceiling = static_cast<float>(dbToLinear(-1.0));
    REQUIRE(meter.preProtectionPeak > 1.0);
    REQUIRE(meter.outputPeak <= ceiling + 0.00001f);
    REQUIRE(meter.maximumReductionDb > 0.);
    for (int n=0; n<3000; ++n) {
        REQUIRE(std::abs(l[n]) <= ceiling + 0.00001f);
        REQUIRE(std::abs(r[n]) <= ceiling + 0.00001f);
        REQUIRE(l[n] == r[n]);
    }
    std::cout << "PASS: bypass / HP / LP / bell / stereo-linked sample peak protection\n";
}
