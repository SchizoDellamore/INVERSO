#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstddef>
#include <utility>
#include <vector>

namespace odd {
constexpr double pi = 3.14159265358979323846;
constexpr int dimensions = 6;
constexpr int voiceCount = 12;
inline float unit(float x) { return std::isfinite(x) ? std::clamp(x, 0.0f, 1.0f) : 0.0f; }
inline uint32_t hash(uint32_t x) { x ^= x >> 16; x *= 0x7feb352du; x ^= x >> 15; x *= 0x846ca68bu; return x ^ (x >> 16); }
inline float randomAt(uint32_t seed, uint64_t index, int lane) {
    return static_cast<float>(hash(seed ^ hash(static_cast<uint32_t>(index)) ^ hash(static_cast<uint32_t>(index >> 32) + 23u) ^ hash(static_cast<uint32_t>(lane) + 51u)) >> 8) / 16777215.0f;
}
struct Route { float base = .3f, low = 0, high = 1; bool locked = false; };
struct Settings {
    std::array<Route, dimensions> route{};
    std::array<float, 4> level{{.65f, .0f, .15f, .0f}};
    int mode = 0; uint32_t seed = 1975;
    float amount = .3f, rate = .15f, tension = 0;
    bool frozen = false;
    float attack = .08f, release = .8f, gain = .18f;
    float sampleMix = 0, sampleStart = 0, sampleEnd = 1, sampleSpeed = 1;
    float grainSize = .14f; bool granular = false, sampleLoop = true, reverse = false;
};

// The random trajectory is a function of time and seed, never of buffer size.
// Locks override the range. A zero amount or Manual is exactly the base value.
class Motion {
public:
    void prepare(double sampleRate) { sr = std::max(8000.0, sampleRate); reset(); }
    void reset() { clock = 0; last.fill(0); haveLast = false; }
    void reseed() { reset(); }
    std::array<float, dimensions> tick(const Settings& s) {
        std::array<float, dimensions> out{};
        const double period = 8.0 * std::pow(.025, static_cast<double>(unit(s.rate)));
        const double phase = clock / period;
        const auto cell = static_cast<uint64_t>(std::floor(phase));
        const float u = static_cast<float>(phase - std::floor(phase));
        const float ease = u*u*(3.0f - 2.0f*u);
        for (int i=0; i<dimensions; ++i) {
            const auto& r = s.route[static_cast<size_t>(i)];
            float v = unit(r.base);
            if (s.mode != 0 && !r.locked && unit(s.amount) > 0) {
                const float lo = std::min(unit(r.low), unit(r.high));
                const float hi = std::max(unit(r.low), unit(r.high));
                const float anchor = std::clamp(v, lo, hi);
                if (s.frozen && haveLast) v = std::clamp(last[static_cast<size_t>(i)], lo, hi);
                else {
                    const float a = randomAt(s.seed, cell, i);
                    const float b = randomAt(s.seed, cell+1, i);
                    float destination = lo + (hi-lo)*(a+(b-a)*ease);
                    if (s.mode == 2) {
                        // Free has more independent motion, but still obeys every bound.
                        const float fast = .5f+.5f*std::sin(static_cast<float>(phase * (1.7+.23*i) * 2*pi) + a*6.28f);
                        destination = lo+(hi-lo)*(.65f*(a+(b-a)*ease)+.35f*fast);
                    }
                    v = std::clamp(anchor + unit(s.amount)*(destination-anchor), lo, hi);
                }
            }
            out[static_cast<size_t>(i)] = unit(v);
        }
        last = out; haveLast = true;
        if (!s.frozen) clock += 1.0/sr;
        return out;
    }
    const std::array<float, dimensions>& current() const { return last; }
private:
    double sr = 48000, clock = 0;
    std::array<float, dimensions> last{}; bool haveLast = false;
};

struct Sample { double sampleRate = 48000; std::vector<float> left, right; };
struct Stereo { float l = 0, r = 0; };

class Engine {
    struct Voice {
        bool active = false, released = false, heldByPedal = false;
        int note = 60, channel = 1;
        uint64_t age = 0;
        double phase = 0, pulse = 0, play = 0, grainClock = 0;
        std::array<double, 12> partial{};
        float env = 0, velocity = 1, noise = 0;
        uint32_t rng = 1, grainSeed = 1;
        float nextNoise() { rng = hash(rng+0x9e3779b9u); return static_cast<float>(rng >> 8)/8388607.5f-1; }
    };
public:
    void prepare(double sampleRate) { sr=std::max(8000.0,sampleRate); motion.prepare(sr); reset(); }
    void reset() { for (auto& v:voices) v=Voice{}; ordinal=0; motion.reset(); bend.fill(0); sustain.fill(false); }
    void stop() { for(auto& v:voices)v=Voice{}; }
    void restartMotion() { ordinal=0; motion.reset(); }
    void setSample(const Sample* data) { stop(); sample=data; }
    bool hasSample() const { return sample && sample->left.size()>1; }
    void noteOn(int note, float velocity, int channel, const Settings& s) {
        Voice* chosen=nullptr;
        for(auto& v:voices) if(!v.active){chosen=&v;break;}
        if(!chosen) chosen=&*std::min_element(voices.begin(),voices.end(),[](const Voice& a,const Voice& b){return a.env < b.env;});
        *chosen=Voice{}; chosen->active=true; chosen->note=std::clamp(note,0,127);
        chosen->channel=std::clamp(channel,1,16);chosen->velocity=unit(velocity);
        chosen->rng=hash(s.seed ^ hash(++ordinal) ^ hash(static_cast<uint32_t>(note)));
        chosen->grainSeed=chosen->rng;
        for(size_t i=0;i<chosen->partial.size();++i) chosen->partial[i]=0;
        chosen->play=unit(s.sampleStart)*sampleLength();
    }
    void noteOff(int note, int channel) {
        for(auto& v:voices)if(v.active&&v.note==note&&v.channel==channel){
            if(sustain[static_cast<size_t>(channel-1)])v.heldByPedal=true;else v.released=true;
        }
    }
    void pedal(int channel, bool down) {
        const auto c=static_cast<size_t>(std::clamp(channel,1,16)-1); sustain[c]=down;
        if(!down)for(auto& v:voices)if(v.channel==channel&&v.heldByPedal){v.released=true;v.heldByPedal=false;}
    }
    void pitchBend(int channel, float semitones) { bend[static_cast<size_t>(std::clamp(channel,1,16)-1)]=std::clamp(semitones,-12.0f,12.0f); }
    int activeVoices() const { int n=0;for(const auto& v:voices)if(v.active)++n;return n; }
    const std::array<float,dimensions>& values() const { return effective; }
    Stereo tick(const Settings& s) {
        auto m=motion.tick(s);
        // The performance macro respects locks and permitted ranges as well.
        for(int k: {0,1,2})if(!s.route[static_cast<size_t>(k)].locked&&s.tension>0){
            const auto& r=s.route[static_cast<size_t>(k)];
            const float hi=std::max(unit(r.low),unit(r.high));
            m[static_cast<size_t>(k)]+=unit(s.tension)*(hi-m[static_cast<size_t>(k)])*.65f;
        }
        effective=m;
        const float rough=m[0], unstable=m[1], dense=m[2], bright=m[3], vowel=m[4], pulseRate=m[5];
        Stereo result;
        const double attackStep=1.0/(sr*std::clamp(static_cast<double>(s.attack),.002,10.0));
        const double releaseStep=1.0/(sr*std::clamp(static_cast<double>(s.release),.01,15.0));
        for(auto& v:voices){
            if(!v.active)continue;
            v.env = v.released ? std::max(0.0f,v.env-static_cast<float>(releaseStep)) : std::min(1.0f,v.env+static_cast<float>(attackStep));
            if(v.released&&v.env<=0){v.active=false;continue;}
            const double age=static_cast<double>(v.age)/sr;
            const double drift=unstable*(.23*std::sin(age*2.17)+.09*std::sin(age*7.63));
            const double f=440*std::pow(2.0,(v.note-69+bend[static_cast<size_t>(v.channel-1)]+drift)/12.0);
            const float white=v.nextNoise();
            v.noise += (.015f+.6f*bright)*(white-v.noise);
            v.phase = std::fmod(v.phase+f/sr,1.0);
            const float fundamental=static_cast<float>(std::sin(v.phase*2*pi));
            float material=0, swarmL=0, swarmR=0;
            for(size_t j=0;j<v.partial.size();++j){
                double ratio;
                if(j<6)ratio=1.0+static_cast<double>(j)*(1.0+rough*.67)+rough*.13*j*j;
                else ratio=std::pow(2.0,(static_cast<double>(j)-8.5)*(dense*.13+unstable*.12)/12.0);
                const double pf=f*ratio;
                v.partial[j]=std::fmod(v.partial[j]+pf/sr,1.0);
                const float osc=pf<sr*.45 ? static_cast<float>(std::sin(v.partial[j]*2*pi)) : 0.0f;
                if(j<6)material+=osc*static_cast<float>(std::exp(-static_cast<double>(j)*(.85-.6*bright)))/(1.0f+.4f*static_cast<float>(j));
                else {
                    const float weight=unit(dense*6.0f-static_cast<float>(j-6));
                    if(j%2==0)swarmL+=osc*weight;else swarmR+=osc*weight;
                }
            }
            const float excitation=.4f+.6f*rough*(.5f+.5f*v.noise);
            material=material*.27f*excitation + v.noise*rough*.17f;
            // Formant-like, band-limited harmonics: explicitly synthetic voice source.
            float throat=0;
            const double formant=350+vowel*1700;
            for(int h=1;h<=14;++h){
                const double hf=f*h;
                if(hf>=sr*.45)break;
                const double formantWeight=std::exp(-std::pow((hf-formant)/(130+vowel*380),2.0)*.5);
                throat+=static_cast<float>(std::sin(v.phase*h*2*pi + rough*.55*fundamental)*(formantWeight+.15/h));
            }
            throat=throat*.3f + v.noise*rough*.1f;
            v.pulse=std::fmod(v.pulse+(0.2+12*pulseRate)/sr,1.0);
            const float beat=static_cast<float>(std::exp(-v.pulse*(14+rough*34)));
            const float pulse=(fundamental*.7f+v.noise*rough*.5f)*beat;
            const float base=material*unit(s.level[0])+throat*unit(s.level[1])+pulse*unit(s.level[3]);
            Stereo voice{base+swarmL*.18f*unit(s.level[2]),base+swarmR*.18f*unit(s.level[2])};
            if(hasSample()&&s.sampleMix>0){
                const auto a=sampleVoice(v,s,f);
                voice.l+=a.l*unit(s.sampleMix);voice.r+=a.r*unit(s.sampleMix);
            }
            const float amp=v.env*v.velocity;
            result.l+=voice.l*amp;result.r+=voice.r*amp;
            ++v.age;
        }
        result.l*=std::clamp(s.gain,0.0f,1.0f);result.r*=std::clamp(s.gain,0.0f,1.0f);
        if(!std::isfinite(result.l))result.l=0;
        if(!std::isfinite(result.r))result.r=0;
        return result;
    }
private:
    double sampleLength()const{return sample?static_cast<double>(sample->left.size()):0;}
    float read(const std::vector<float>& data,double at)const{
        if(data.empty())return 0;
        at=std::clamp(at,0.0,static_cast<double>(data.size()-1));
        const auto i=static_cast<size_t>(at),j=std::min(i+1,data.size()-1);
        return data[i]+(data[j]-data[i])*static_cast<float>(at-static_cast<double>(i));
    }
    Stereo sampleVoice(Voice& v,const Settings& s,double f){
        const double length=sampleLength();
        const double lo=std::min(unit(s.sampleStart),unit(s.sampleEnd))*std::max(0.0,length-2);
        const double hi=std::max(lo+2,std::max(unit(s.sampleStart),unit(s.sampleEnd))*length);
        const double span=hi-lo;
        const double step=sample->sampleRate/sr*std::clamp(static_cast<double>(s.sampleSpeed),.125,4.0)*(f/261.625565);
        if(v.play<lo)v.play=lo;
        double pos=v.play;
        float window=1;
        if(s.granular){
            const double grainSamples=std::clamp(static_cast<double>(s.grainSize),.015,.5)*sr;
            const double cycle=v.grainClock/grainSamples;
            const auto index=static_cast<uint64_t>(cycle);
            const double phase=cycle-std::floor(cycle);
            const double region=std::max(0.0,span-grainSamples*step);
            pos=lo+randomAt(v.grainSeed,index,14)*region+phase*grainSamples*step;
            window=static_cast<float>(.5-.5*std::cos(phase*2*pi));
            ++v.grainClock;
        }else{
            if(!s.sampleLoop&&v.play>=hi)return {};
            pos=lo+std::fmod(std::max(0.0,v.play-lo),span);
            const double edge=std::min(128.0,span*.1);
            window=static_cast<float>(std::min({1.0,(pos-lo)/edge,(hi-pos)/edge}));
            v.play+=step;
        }
        if(s.reverse)pos=hi-1-(pos-lo);
        return {read(sample->left,pos)*window,read(sample->right.empty()?sample->left:sample->right,pos)*window};
    }
    double sr=48000;std::array<Voice,voiceCount> voices{};uint32_t ordinal=0;
    std::array<float,16>bend{};std::array<bool,16>sustain{};
    Motion motion;std::array<float,dimensions> effective{};const Sample* sample=nullptr;
};

namespace edit {
inline std::pair<size_t,size_t> range(const Sample& s,float a,float b){
    const auto n=s.left.size();const auto lo=static_cast<size_t>(std::min(unit(a),unit(b))*static_cast<float>(n));
    const auto hi=static_cast<size_t>(std::max(unit(a),unit(b))*static_cast<float>(n));
    return {std::min(lo,n),std::min(hi,n)};
}
inline void reverse(Sample& s,float a,float b){auto [lo,hi]=range(s,a,b);for(auto* d:{&s.left,&s.right})if(d->size()>=hi)std::reverse(d->begin()+static_cast<ptrdiff_t>(lo),d->begin()+static_cast<ptrdiff_t>(hi));}
inline void crop(Sample& s,float a,float b){auto [lo,hi]=range(s,a,b);if(hi<=lo)return;for(auto* d:{&s.left,&s.right})if(d->size()>=hi)*d=std::vector<float>(d->begin()+static_cast<ptrdiff_t>(lo),d->begin()+static_cast<ptrdiff_t>(hi));}
inline void erase(Sample& s,float a,float b){auto [lo,hi]=range(s,a,b);if(hi<=lo)return;for(auto* d:{&s.left,&s.right})if(d->size()>=hi)d->erase(d->begin()+static_cast<ptrdiff_t>(lo),d->begin()+static_cast<ptrdiff_t>(hi));}
inline void fade(Sample& s,float a,float b,bool fadeIn){auto [lo,hi]=range(s,a,b);if(hi<=lo)return;for(auto* d:{&s.left,&s.right})if(d->size()>=hi)for(size_t i=lo;i<hi;++i){float w=static_cast<float>(i-lo)/static_cast<float>(std::max<size_t>(1,hi-lo-1));(*d)[i]*=fadeIn?w:1-w;}}
inline void speed(Sample& s,float a,float b,double factor){
    auto [lo,hi]=range(s,a,b);if(hi<=lo)return;factor=std::clamp(factor,.25,4.0);
    const auto count=static_cast<size_t>(static_cast<double>(hi-lo)/factor);
    for(auto* d:{&s.left,&s.right})if(d->size()>=hi){
        std::vector<float> out;out.reserve(d->size()-(hi-lo)+count);out.insert(out.end(),d->begin(),d->begin()+static_cast<ptrdiff_t>(lo));
        for(size_t i=0;i<count;++i){double p=lo+i*factor;auto k=std::min(static_cast<size_t>(p),hi-1);auto j=std::min(k+1,hi-1);out.push_back((*d)[k]+((*d)[j]-(*d)[k])*static_cast<float>(p-k));}
        out.insert(out.end(),d->begin()+static_cast<ptrdiff_t>(hi),d->end());*d=std::move(out);
    }
}
}
}
