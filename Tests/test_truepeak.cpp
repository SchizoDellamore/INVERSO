#include "../Source/DSP/EQCore.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <iostream>
#include <stdexcept>
#include <vector>
#define REQUIRE(x) do {if (!(x)) throw std::runtime_error("Requirement failed: " #x); } while (false)

int main() {
    using namespace inverso;
    FourXPeakEstimator interp;
    interp.prepare();
    float peak = 0;
    for (int i=0; i<1000; ++i) peak=std::max(peak,interp.process(0.));
    REQUIRE(peak == 0.f);
    interp.reset();
    float maxConstant=0.;
    for (int i=0;i<256;++i) maxConstant=std::max(maxConstant,interp.process(0.5f));
    REQUIRE(std::abs(maxConstant-0.5f)<0.08f); // filter startup can overshoot
    interp.reset();
    float maximumSample = 0.f, maximumInterp = 0.f;
    // phase-offset near-Nyquist sine. Includes interpolated peaks not present in samples.
    for (int n=0;n<4000;++n) {
        const float sample=0.91f * std::sin(static_cast<float>(2.*kPi*10473.*n/48000.+0.27));
        if(n>64)maximumSample=std::max(maximumSample,std::abs(sample));
        const auto estimate=interp.process(sample);
        if(n>64)maximumInterp=std::max(maximumInterp,estimate);
    }
    REQUIRE(std::isfinite(maximumInterp) && maximumInterp>0.85f);
    REQUIRE(maximumInterp <= 1.2f);
    // An Fs/4 sinusoid sampled at 45 degrees peaks at only 0.707 of
    // its continuous reconstructed amplitude: detect an intersample peak.
    interp.reset();
    maximumSample=maximumInterp=0;
    for(int n=0;n<1024;++n){
        const float x=static_cast<float>(std::sin(2.*kPi*12000.*n/48000.+kPi/4.));
        const float truePeak=interp.process(x);
        if(n>64){maximumSample=std::max(maximumSample,std::abs(x));
                 maximumInterp=std::max(maximumInterp,truePeak);}
    }
    REQUIRE(maximumSample<0.72f);
    REQUIRE(maximumInterp>0.96f);

    Settings s{};
    s.protectionEnabled = true;
    s.protectionCeilingDb = -3.0;
    EQCore eq;
    eq.prepare(48000., 2);
    REQUIRE(eq.latencySamples() >= 120 && eq.latencySamples() <= 200);
    eq.setSettings(s);
    std::vector<float> left(96000), right(96000);
    for (size_t i=0;i<left.size();++i) {
        left[i]=1.4f * std::sin(static_cast<float>(2.*kPi*7855.*i/48000.+0.4));
        right[i]=0.7f * std::sin(static_cast<float>(2.*kPi*7855.*i/48000.+0.4));
    }
    // Process in unusual block lengths; peak protection and latency are block-independent.
    float maxDbTp=0.f, maxReduction=0.f;
    for (size_t pos=0;pos<left.size();) {
        const int count=static_cast<int>(std::min<std::size_t>(77+(pos%113),left.size()-pos));
        float* ptrs[2]={left.data()+pos,right.data()+pos};
        const auto meter=eq.process(ptrs,2,count);
        maxDbTp=std::max(maxDbTp,meter.outputTruePeak);
        maxReduction=std::max(maxReduction,meter.maximumReductionDb);
        REQUIRE(std::isfinite(meter.outputTruePeak));
        for(int n=0;n<count;++n) {
            REQUIRE(std::isfinite(ptrs[0][n]));
            REQUIRE(std::isfinite(ptrs[1][n]));
        }
        pos+=count;
    }
    REQUIRE(maxReduction>0.1f);
    // Tolerance: interpolated detector with a time-varying gain is not a
    // certified true-peak limiter; keep overshoot bounded for this signal.
    REQUIRE(maxDbTp <= dbToLinear(-3.) * 1.04f);

    // With protection off, reporting latency must remain constant and
    // sufficiently long to maintain alignment in a parallel mix.
    eq.prepare(48000., 1);
    s.protectionEnabled=false;
    eq.setSettings(s);
    std::array<float,512> impulse{};
    impulse[0]=0.5f;
    float* ptr=&impulse[0];
    eq.process(&ptr,1,512);
    REQUIRE(std::abs(impulse[eq.latencySamples()]-0.5f)<0.0001);
    REQUIRE(std::abs(impulse[0])<0.0001);
    std::cout << "PASS: x4 interpolation, 3 ms fixed latency, linked guard, streaming blocks and true-peak output estimate\n";
}
