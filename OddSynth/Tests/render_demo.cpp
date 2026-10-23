#include "../Source/OddCore.h"
#include <fstream>
#include <string>
#include <algorithm>

static void u16(std::ofstream& f,uint16_t n){f.put(static_cast<char>(n&255));f.put(static_cast<char>(n>>8));}
static void u32(std::ofstream& f,uint32_t n){u16(f,static_cast<uint16_t>(n&65535));u16(f,static_cast<uint16_t>(n>>16));}
int main(int argc,char** argv){
    constexpr int rate=48000,chapter=rate*8,chapters=5,frames=chapter*chapters;
    std::ofstream out(argc>1?argv[1]:"OddSynth_Demonstracao.wav",std::ios::binary);if(!out)return 1;
    out.write("RIFF",4);u32(out,36+frames*6);out.write("WAVEfmt ",8);u32(out,16);u16(out,1);u16(out,2);u32(out,rate);u32(out,rate*6);u16(out,6);u16(out,24);out.write("data",4);u32(out,frames*6);
    odd::Sample sample;sample.sampleRate=rate;sample.left.resize(rate*2);sample.right.resize(rate*2);
    uint32_t rng=2391;for(size_t i=0;i<sample.left.size();++i){const double t=static_cast<double>(i)/rate;rng=odd::hash(rng+7);const double noise=static_cast<double>(rng>>8)/8388607.5-1;const double impulse=std::exp(-std::fmod(t,.337)*24);const double metal=std::sin(t*2*odd::pi*263)+.7*std::sin(t*2*odd::pi*419)+.4*std::sin(t*2*odd::pi*713);sample.left[i]=static_cast<float>((metal*.1+noise*.17)*impulse);sample.right[i]=static_cast<float>((metal*.09-noise*.12)*impulse);}
    for(int section=0;section<chapters;++section){
        odd::Engine engine;engine.prepare(rate);engine.setSample(&sample);odd::Settings s;s.seed=171+static_cast<uint32_t>(section)*991;s.gain=.2f;s.attack=.35f;s.release=1.1f;s.amount=.65f;s.rate=.25f;s.mode=1;
        for(auto& r:s.route)r={.4f,.15f,.75f,false};
        s.level.fill(0);s.level[static_cast<size_t>(std::min(section,3))]=.9f;
        if(section==0){s.route[0].base=.75f;s.route[1].base=.15f;s.route[3].base=.6f;}
        if(section==1){s.route[4].base=.6f;s.route[0].base=.55f;s.mode=2;}
        if(section==2){s.route[2].base=.95f;s.route[2].high=1;s.route[1].base=.75f;s.level[0]=.08f;}
        if(section==3){s.route[5].base=.4f;s.route[0].base=.6f;s.route[1].base=.4f;s.level[0]=.12f;}
        if(section==3){s.attack=.003f;s.release=.3f;}
        if(section==4){s.level.fill(0);s.level[0]=.75f;s.level[2]=.15f;s.sampleMix=.9f;s.sampleRoute=1;s.granular=true;s.grainSize=.19f;s.mode=2;s.route[1].base=.85f;s.route[2].base=.7f;}
        const int root=section==1?45:section==3?36:41;
        engine.noteOn(root,.8f,1,s);
        for(int n=0;n<chapter;++n){
            if(n==rate*2&&section!=3)engine.noteOn(root+(section==2?1:6),.48f,1,s);
            if(n==rate*4&&section!=3)engine.noteOn(root+13,.35f,1,s);
            if(section==3&&n%(rate/2)==0)engine.noteOn(root+(n/(rate/2)%3)*7,.5f,1,s);
            if(section==3&&n%(rate/2)==rate/4)for(int k:{0,7,14})engine.noteOff(root+k,1);
            if(n==chapter-rate)for(int k:{0,1,6,7,13,14})engine.noteOff(root+k,1);
            s.tension=static_cast<float>(n)/chapter*.6f;
            const auto a=engine.tick(s);const float fade=static_cast<float>(std::min({1.0,static_cast<double>(n)/(rate*.08),static_cast<double>(chapter-n)/(rate*.14)}));
            for(float v:{a.l,a.r}){const int32_t value=static_cast<int32_t>(std::clamp(v*fade,-.95f,.95f)*8388607.0f);out.put(static_cast<char>(value&255));out.put(static_cast<char>((value>>8)&255));out.put(static_cast<char>((value>>16)&255));}
        }
    }
    return out?0:1;
}
