#include "../Source/DSP/EQCore.h"
#include <cmath>
#include <iostream>
#include <random>
#include <stdexcept>
#include <vector>
#define REQUIRE(condition) do { if (!(condition)) throw std::runtime_error("Test failed: " #condition); } while (false)

int main() {
    using namespace inverso;
    // Output gain must ramp rather than jumping at the beginning of a block.
    EQCore eq;
    Settings s;
    s.protectionEnabled = false;
    eq.prepare(48000., 2);
    eq.setSettings(s);
    float left[1024], right[1024];
    float* signal[2] = {left, right};
    for (int i=0;i<1024;++i) left[i] = right[i] = 0.1f;
    eq.process(signal,2,1024);
    s.outputGainDb = 12.;
    eq.setSettings(s);
    float oneL=0.1f, oneR=0.1f;
    float* one[2] = {&oneL,&oneR};
    eq.process(one,2,1);
    REQUIRE(oneL < 0.101f && oneL > 0.099f);
    REQUIRE(oneL == oneR);
    for(int i=0;i<1024;++i) left[i]=right[i]=0.1f;
    eq.process(signal,2,1024);
    REQUIRE(std::abs(left[1023] - 0.1f * dbToLinear(12.)) < 0.0005f);

    // Rapid automation: reject non-finite values and unexpected ceiling overshoot.
    std::mt19937 rng(814);
    std::uniform_real_distribution<double> randF(25., 18000.), randQ(0.2,18.), randDb(-18.,18.);
    std::uniform_real_distribution<float> randInput(-1.3f,1.3f);
    for (double sr : {44100., 48000., 96000.}) {
        eq.prepare(sr,2);
        s={};
        s.highPassEnabled=true; s.lowPassEnabled=true;
        s.highPassSlope=48; s.lowPassSlope=48;
        s.protectionEnabled=true; s.protectionCeilingDb=-1.;
        eq.setSettings(s);
        for(int block=0;block<160;++block) {
            s.highPassHz = 30. + static_cast<double>(block%40)*13.;
            s.lowPassHz = 7000. + static_cast<double>(block%40)*190.;
            s.highPassEnabled = (block%7 !=0);
            s.lowPassEnabled = (block%9 !=0);
            s.outputGainDb = randDb(rng);
            for (auto& band : s.bands) {
                band.enabled = (block%3 !=0);
                band.frequency=randF(rng);
                band.q=randQ(rng);
                band.gainDb=randDb(rng);
            }
            eq.setSettings(s);
            for (int n=0;n<1024;++n) {left[n]=randInput(rng);right[n]=randInput(rng);}
            const auto m=eq.process(signal,2,1024);
            REQUIRE(std::isfinite(m.outputPeak));
            REQUIRE(m.outputPeak <= dbToLinear(s.protectionCeilingDb) + 0.00002);
            for(int n=0;n<1024;++n) REQUIRE(std::isfinite(left[n]) && std::isfinite(right[n]));
        }
    }
    std::cout << "PASS: smoothed output gain and rapid filter automation, finite audio and linked sample-peak ceiling\n";
}
