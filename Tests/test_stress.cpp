#include "../Source/DSP/EQCore.h"
#include <stdexcept>
#define REQUIRE(condition) do { if (!(condition)) throw std::runtime_error("Test failed: " #condition); } while (false)
#include <cmath>
#include <iostream>
#include <random>
#include <vector>

int main() {
    std::mt19937 rng(13107);
    std::uniform_real_distribution<float> input(-1.5f,1.5f);
    std::uniform_real_distribution<double> freq(20.,17000.), gain(-18.,18.), q(0.25,18.);
    for(double sr:{44100.,48000.,96000.}) {
        inverso::EQCore engine;
        engine.prepare(sr,2);
        inverso::Settings s;
        s.highPassEnabled=true;s.lowPassEnabled=true;
        s.highPassHz=25.;s.lowPassHz=17000.;
        s.highPassSlope=48;s.lowPassSlope=48;
        s.outputGainDb=4.;s.protectionEnabled=true;
        for(auto& b:s.bands) b={true,freq(rng),gain(rng),q(rng)};
        engine.setSettings(s);
        std::vector<float> a(512), b(512);float* data[2]={a.data(),b.data()};
        for(int block=0;block<30;++block) {
            for(int i=0;i<512;++i) {a[i]=input(rng);b[i]=input(rng);}
            const auto m=engine.process(data,2,512);
            REQUIRE(std::isfinite(m.inputPeak)&&std::isfinite(m.outputPeak));
            REQUIRE(m.outputPeak <= inverso::dbToLinear(-1.) + 2e-5);
            for(int i=0;i<512;++i){REQUIRE(std::isfinite(a[i])&&std::isfinite(b[i]));}
        }
    }
    std::cout << "PASS: randomized stress at 44.1 / 48 / 96 kHz, 48 dB/oct filters, 8 EQ bands, finite stereo sample ceiling\n";
}
