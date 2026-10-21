#include "../Source/DSP/EQCore.h"
#include <chrono>
#include <cmath>
#include <iostream>
#include <vector>

int main(){
    using namespace inverso;
    constexpr int block=256, blocks=188; // ~1 second per instance at 48 kHz
    constexpr double sr=48000.;
    const Settings settings{};
    for(int n : {16,32,64}){
        std::vector<EQCore> instances(static_cast<std::size_t>(n));
        for(auto& eq: instances){eq.prepare(sr,2);eq.setSettings(settings);}
        std::vector<float> l(block),r(block);
        const auto begin=std::chrono::steady_clock::now();
        float checkpoint=0;
        for(int j=0;j<blocks;++j){
            for(int k=0;k<block;++k){l[k]=static_cast<float>(0.6*std::sin(k*0.02+j));r[k]=l[k]*0.7f;}
            for(auto& instance:instances){
                // Each instance processes the same audio to benchmark overhead, not a serial chain.
                float left[block],right[block];
                std::copy(l.begin(),l.end(),left);std::copy(r.begin(),r.end(),right);
                float* data[2]={left,right};
                auto m=instance.process(data,2,block);
                checkpoint+=m.outputTruePeak;
            }
        }
        const double seconds=std::chrono::duration<double>(std::chrono::steady_clock::now()-begin).count();
        if(!std::isfinite(checkpoint))return 1;
        std::cout<<n<<" instances: processed "<<blocks*block/sr<<" audio seconds/instance in "<<seconds<<" wall seconds (single benchmark thread; not DAW CPU meter)\n";
    }
}
