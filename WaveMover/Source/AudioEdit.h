#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <memory>
#include <random>
#include <stdexcept>
#include <string>
#include <vector>
#include <signalsmith-stretch.h>

namespace wavemover {
constexpr double pi=3.14159265358979323846;
constexpr std::size_t maxBytes=256u*1024u*1024u;
struct Clip {
    double rate=48000;
    std::vector<std::vector<float>> data;
    Clip()=default;
    Clip(int channels,std::size_t frames,double sr):rate(sr) {
        if(channels<1||channels>2||!std::isfinite(sr)||sr<8000||sr>384000||frames>maxBytes/(sizeof(float)*static_cast<std::size_t>(channels)))
            throw std::runtime_error("Audio excede o limite de 256 MB, canais ou taxa suportada.");
        data.assign(static_cast<std::size_t>(channels),std::vector<float>(frames));
    }
    int channels()const{return static_cast<int>(data.size());}
    std::size_t frames()const{return data.empty()?0:data[0].size();}
    std::size_t bytes()const{return frames()*data.size()*sizeof(float);}
};
using Audio=std::shared_ptr<const Clip>;
struct Range {std::size_t begin=0,end=0;};
inline Range bounded(Range r,std::size_t n){r.begin=std::min(r.begin,n);r.end=std::clamp(r.end,r.begin,n);return r;}
inline Clip section(const Clip& a,Range r){
    r=bounded(r,a.frames()); Clip b(a.channels(),r.end-r.begin,a.rate);
    for(int c=0;c<a.channels();++c)std::copy(a.data[c].begin()+static_cast<std::ptrdiff_t>(r.begin),a.data[c].begin()+static_cast<std::ptrdiff_t>(r.end),b.data[c].begin());
    return b;
}
inline Clip splice(const Clip&a,Range r,const Clip&b){
    r=bounded(r,a.frames());
    if(a.channels()!=b.channels()||a.rate!=b.rate)throw std::runtime_error("Formato diferente na colagem.");
    Clip out(a.channels(),a.frames()-(r.end-r.begin)+b.frames(),a.rate);
    for(int c=0;c<a.channels();++c){auto p=std::copy(a.data[c].begin(),a.data[c].begin()+static_cast<std::ptrdiff_t>(r.begin),out.data[c].begin());p=std::copy(b.data[c].begin(),b.data[c].end(),p);std::copy(a.data[c].begin()+static_cast<std::ptrdiff_t>(r.end),a.data[c].end(),p);}
    return out;
}
inline void cleanMarkers(std::vector<std::size_t>&m,std::size_t n){m.erase(std::remove_if(m.begin(),m.end(),[n](auto p){return p==0||p>=n;}),m.end());std::sort(m.begin(),m.end());m.erase(std::unique(m.begin(),m.end()),m.end());}
inline std::vector<std::size_t> grid(const Clip&a,double bpm,int divisions){
    if(!std::isfinite(bpm)||bpm<20||bpm>400||divisions<1||divisions>64)throw std::runtime_error("Grade invalida.");
    std::vector<std::size_t> m; const double hop=a.rate*60.0/bpm/divisions;
    for(double p=hop;p<static_cast<double>(a.frames())&&m.size()<20000;p+=hop)m.push_back(static_cast<std::size_t>(std::llround(p)));
    cleanMarkers(m,a.frames());return m;
}
inline std::vector<std::size_t> transients(const Clip&a,double sensitivity){
    std::vector<std::size_t> m;const auto hop=std::max<std::size_t>(1,static_cast<std::size_t>(a.rate*.003));
    const auto minGap=static_cast<std::size_t>(a.rate*.045);double slow=0,previous=0;std::size_t last=0;
    const double threshold=1.3+4.0*(1-std::clamp(sensitivity,0.0,1.0));
    for(std::size_t i=0;i<a.frames();i+=hop){double energy=0;const auto end=std::min(i+hop,a.frames());
        for(int c=0;c<a.channels();++c)for(auto j=i;j<end;++j)energy+=a.data[c][j]*a.data[c][j];
        energy=std::sqrt(energy/static_cast<double>((end-i)*a.channels()));
        if(i>=minGap&&i-last>=minGap&&energy>.008&&energy>slow*threshold&&energy>previous*1.15){m.push_back(i);last=i;}
        slow=.92*slow+.08*energy;previous=energy;
    }cleanMarkers(m,a.frames());return m;
}
enum class Op {Reverse,Delete,Trim,Silence,Duplicate,FadeIn,FadeOut,FadeBoth,Gain,Normalize,Stretch,Pitch,TempoRamp,Speed,TapeStop,TapeStart,Reverb,Tail,ReverseTail,Delay,Lowpass,Highpass,Saturate,Repeat,Ratchet,PingPong,Freeze,Shuffle};
struct Parameters {double a=1,b=1;std::uint32_t seed=1984;};
struct Result {Clip audio;std::vector<std::size_t> markers;};
inline float lerpRead(const std::vector<float>&a,double p){
    if(a.empty()||p<0||p>static_cast<double>(a.size()-1))return 0;
    const auto i=static_cast<std::size_t>(p);const auto j=std::min(i+1,a.size()-1);const auto f=static_cast<float>(p-static_cast<double>(i));return a[i]+f*(a[j]-a[i]);
}
inline Clip stretch(const Clip&a,double ratio,double semitones){
    if(!std::isfinite(ratio)||ratio<.25||ratio>4||!std::isfinite(semitones)||std::abs(semitones)>24)throw std::runtime_error("Duracao: 25 a 400%; afinacao: -24 a +24 semitons.");
    if(a.frames()==0)return a;if(std::abs(ratio-1)<1e-9&&std::abs(semitones)<1e-9)return a;
    const auto n=std::max<std::size_t>(1,static_cast<std::size_t>(std::llround(static_cast<double>(a.frames())*ratio)));
    Clip out(a.channels(),n,a.rate);
    signalsmith::stretch::SignalsmithStretch<float> s; s.presetDefault(a.channels(),static_cast<float>(a.rate));s.setTransposeSemitones(static_cast<float>(semitones));
    const auto pad=static_cast<std::size_t>(std::ceil(a.rate*.25));const auto outPad=static_cast<std::size_t>(std::llround(static_cast<double>(pad)*ratio));
    Clip input(a.channels(),a.frames()+pad*2,a.rate),output(a.channels(),n+outPad*2,a.rate);
    for(int c=0;c<a.channels();++c)std::copy(a.data[c].begin(),a.data[c].end(),input.data[c].begin()+static_cast<std::ptrdiff_t>(pad));
    if(!s.exact(input.data,static_cast<int>(input.frames()),output.data,static_cast<int>(output.frames())))throw std::runtime_error("Trecho insuficiente para o algoritmo de stretch.");
    for(int c=0;c<a.channels();++c)std::copy_n(output.data[c].begin()+static_cast<std::ptrdiff_t>(outPad),n,out.data[c].begin());
    return out;
}
inline Clip tempoRamp(const Clip&a,double from,double to){
    if(from<20||from>400||to<20||to>400||!std::isfinite(from)||!std::isfinite(to))throw std::runtime_error("Tempo deve ficar entre 20 e 400 BPM.");
    if(a.frames()==0||std::abs(from-to)<1e-9)return a;
    // Source is at 'from' BPM; target BPM ramps linearly in SOURCE time.
    // At each source position the duration factor is from/targetBpm.
    signalsmith::stretch::SignalsmithStretch<float>s;s.presetDefault(a.channels(),static_cast<float>(a.rate));
    const int il=s.inputLatency(),ol=s.outputLatency();const std::size_t hop=512;
    Clip seek(a.channels(),static_cast<std::size_t>(il),a.rate);
    for(int c=0;c<a.channels();++c)std::copy_n(a.data[c].begin(),std::min(a.frames(),seek.frames()),seek.data[c].begin());
    s.seek(seek.data,il,1.f);
    std::vector<std::size_t> counts;std::size_t total=0;double exact=0;
    for(std::size_t pos=0;pos<a.frames();pos+=hop){const auto k=std::min(hop,a.frames()-pos);const double t=(static_cast<double>(pos)+static_cast<double>(k)*.5)/static_cast<double>(a.frames());exact+=static_cast<double>(k)*from/(from+(to-from)*t);const auto target=static_cast<std::size_t>(std::llround(exact));counts.push_back(target-total);total=target;}
    Clip render(a.channels(),total+static_cast<std::size_t>(ol),a.rate);std::size_t index=0,write=0;
    std::vector<float*> output(static_cast<std::size_t>(a.channels()));
    for(std::size_t pos=0;pos<a.frames();pos+=hop){const auto k=std::min(hop,a.frames()-pos);Clip block(a.channels(),k,a.rate);
        for(int c=0;c<a.channels();++c){for(std::size_t j=0;j<k;++j){const auto src=static_cast<std::size_t>(il)+pos+j;if(src<a.frames())block.data[c][j]=a.data[c][src];}output[c]=render.data[c].data()+write;}
        s.process(block.data,static_cast<int>(k),output.data(),static_cast<int>(counts[index]));write+=counts[index++];}
    for(int c=0;c<a.channels();++c)output[c]=render.data[c].data()+write;
    s.flush(output.data(),ol,static_cast<float>(to/from));
    Clip out(a.channels(),total,a.rate);for(int c=0;c<a.channels();++c)std::copy_n(render.data[c].begin()+ol,total,out.data[c].begin());return out;
}
inline Clip varispeed(const Clip&a,double start,double end){
    if(!std::isfinite(start)||!std::isfinite(end)||start<0||end<0||start>4||end>4||start+end<.1)throw std::runtime_error("Velocidade invalida (0 a 4x; media minima 0.05x).");
    if(a.frames()==0)return a;const double average=(start+end)*.5;
    Clip out(a.channels(),std::max<std::size_t>(1,static_cast<std::size_t>(std::llround(static_cast<double>(a.frames())/average))),a.rate);
    for(std::size_t i=0;i<out.frames();++i){const double t=out.frames()>1?static_cast<double>(i)/static_cast<double>(out.frames()-1):0;const double p=static_cast<double>(a.frames()-1)*(start*t+.5*(end-start)*t*t)/average;
        const double speed=start+(end-start)*t,band=std::min(1.,1./std::max(1.,speed));const auto center=static_cast<std::int64_t>(p);
        for(int c=0;c<a.channels();++c){double v=0,w=0;for(int tap=-12;tap<=12;++tap){const auto j=center+tap;const double x=(p-static_cast<double>(j))*band;const double sinc=std::abs(x)<1e-9?1:std::sin(pi*x)/(pi*x);const double win=.5+.5*std::cos(pi*(p-static_cast<double>(j))/13.);const double weight=sinc*win*band;if(j>=0&&j<static_cast<std::int64_t>(a.frames())){v+=a.data[c][static_cast<std::size_t>(j)]*weight;w+=weight;}}out.data[c][i]=static_cast<float>(std::abs(w)>1e-9?v/w:0);}}
    return out;
}
inline Clip reverb(const Clip&a,double seconds,double mix,bool tailOnly){
    seconds=std::clamp(seconds,.1,15.);mix=std::clamp(mix,0.,1.);
    Clip out(a.channels(),a.frames()+static_cast<std::size_t>(std::ceil(a.rate*seconds*1.25)),a.rate);
    const double lengths[]{.0297,.0371,.0411,.0437};
    for(int c=0;c<a.channels();++c){std::vector<std::vector<double>> rings;double feedback[4]{},low[4]{};
        for(int k=0;k<4;++k){const auto n=static_cast<std::size_t>(a.rate*(lengths[k]+c*.0017));rings.emplace_back(std::max<std::size_t>(1,n));feedback[k]=std::pow(.001,static_cast<double>(n)/(a.rate*seconds));}
        for(std::size_t i=0;i<out.frames();++i){const double dry=i<a.frames()?a.data[c][i]:0;double wet=0;
            for(int k=0;k<4;++k){auto&r=rings[k];const auto j=i%r.size();const double delayed=r[j];low[k]=.65*delayed+.35*low[k];r[j]=dry+low[k]*feedback[k];wet+=delayed*.25;}
            out.data[c][i]=static_cast<float>((tailOnly?0:dry)+mix*wet);}}
    return out;
}
inline Clip crossfadeAt(const Clip&a,std::size_t seam,double ms){
    seam=std::min(seam,a.frames());const auto n=std::min({seam,a.frames()-seam,static_cast<std::size_t>(std::max(0.,ms)*a.rate*.001)});
    if(n==0)throw std::runtime_error("Posicione o cursor entre dois trechos; crossfade precisa de audio dos dois lados.");
    Clip out(a.channels(),a.frames()-n,a.rate);
    for(int c=0;c<a.channels();++c){std::copy_n(a.data[c].begin(),seam-n,out.data[c].begin());for(std::size_t j=0;j<n;++j){const double t=n==1?.5:static_cast<double>(j)/static_cast<double>(n-1);const auto w=static_cast<float>(.5-.5*std::cos(pi*t));out.data[c][seam-n+j]=a.data[c][seam-n+j]*(1-w)+a.data[c][seam+j]*w;}std::copy(a.data[c].begin()+static_cast<std::ptrdiff_t>(seam+n),a.data[c].end(),out.data[c].begin()+static_cast<std::ptrdiff_t>(seam));}
    return out;
}
inline Result render(const Clip&a,Op op,Parameters p,std::vector<std::size_t>markers={}){
    Result result{a,{}};auto&out=result.audio;const auto n=a.frames();if(n==0)return result;
    switch(op){
    case Op::Reverse:for(auto&c:out.data)std::reverse(c.begin(),c.end());break;
    case Op::Delete:out=Clip(a.channels(),0,a.rate);break;
    case Op::Trim:break;
    case Op::Silence:for(auto&c:out.data)std::fill(c.begin(),c.end(),0.f);break;
    case Op::Duplicate:case Op::Repeat:case Op::PingPong:{const int count=op==Op::Duplicate?2:std::clamp(static_cast<int>(p.a),2,32);out=Clip(a.channels(),n*static_cast<std::size_t>(count),a.rate);
        for(int k=0;k<count;++k){if(k)result.markers.push_back(n*static_cast<std::size_t>(k));for(int c=0;c<a.channels();++c)for(std::size_t j=0;j<n;++j)out.data[c][n*static_cast<std::size_t>(k)+j]=a.data[c][op==Op::PingPong&&k%2?n-1-j:j];}break;}
    case Op::FadeIn:case Op::FadeOut:case Op::FadeBoth:{const auto fade=std::min(n,static_cast<std::size_t>(std::max(0.,p.a)*a.rate*.001));const double curve=std::clamp(p.b,.1,8.);for(auto&c:out.data)for(std::size_t j=0;j<fade;++j){const double t=fade>1?static_cast<double>(j)/static_cast<double>(fade-1):0;const float v=static_cast<float>(std::pow(t,curve));if(op!=Op::FadeOut)c[j]*=v;if(op!=Op::FadeIn)c[n-1-j]*=v;}break;}
    case Op::Gain:{const float gain=static_cast<float>(std::pow(10.,p.a/20.));for(auto&c:out.data)for(auto&v:c)v*=gain;break;}
    case Op::Normalize:{float peak=0;for(const auto&c:a.data)for(float v:c)peak=std::max(peak,std::abs(v));if(peak>0){const float gain=static_cast<float>(std::pow(10.,-.05))/peak;for(auto&c:out.data)for(auto&v:c)v*=gain;}break;}
    case Op::Stretch:out=stretch(a,p.a/100.,0);break;
    case Op::Pitch:out=stretch(a,1,p.a);break;
    case Op::TempoRamp:out=tempoRamp(a,p.a,p.b);break;
    case Op::Speed:out=varispeed(a,p.a,p.a);break;
    case Op::TapeStop:case Op::TapeStart:out=varispeed(a,op==Op::TapeStop?1:0,op==Op::TapeStop?0:1);{auto edge=render(out,op==Op::TapeStop?Op::FadeOut:Op::FadeIn,{5,1,p.seed});out=std::move(edge.audio);}break;
    case Op::Reverb:case Op::Tail:case Op::ReverseTail:out=reverb(a,p.a,p.b/100.,op!=Op::Reverb);if(op==Op::ReverseTail)for(auto&c:out.data)std::reverse(c.begin(),c.end());break;
    case Op::Delay:{const auto delay=std::max<std::size_t>(1,static_cast<std::size_t>(std::clamp(p.a,1.,4000.)*a.rate*.001));const double fb=std::clamp(p.b/100.,0.,.9);const int repeats=fb>0?std::min(80,static_cast<int>(std::ceil(std::log(.0001)/std::log(fb)))+1):1;out=Clip(a.channels(),n+delay*static_cast<std::size_t>(repeats),a.rate);
        for(int c=0;c<a.channels();++c)for(std::size_t i=0;i<out.frames();++i){const double dry=i<n?a.data[c][i]:0;out.data[c][i]=static_cast<float>(dry+(i>=delay?out.data[c][i-delay]*fb:0));}break;}
    case Op::Lowpass:case Op::Highpass:{const double alpha=1-std::exp(-2*pi*std::clamp(p.a,20.,a.rate*.45)/a.rate);for(int c=0;c<a.channels();++c){double last=0;for(auto&v:out.data[c]){last+=alpha*(v-last);v=op==Op::Lowpass?static_cast<float>(last):v-static_cast<float>(last);}}break;}
    case Op::Saturate:{const double drive=std::pow(10.,std::clamp(p.a,0.,36.)/20.);for(auto&c:out.data)for(auto&v:c)v=static_cast<float>(std::tanh(v*drive));break;}
    case Op::Ratchet:{const int count=std::clamp(static_cast<int>(p.a),2,32);const auto k=std::max<std::size_t>(1,n/static_cast<std::size_t>(count));for(int c=0;c<out.channels();++c)for(std::size_t j=0;j<n;++j)out.data[c][j]=a.data[c][j%k];for(std::size_t j=k;j<n;j+=k)result.markers.push_back(j);break;}
    case Op::Freeze:{const auto count=static_cast<std::size_t>(std::clamp(p.a,.05,30.)*a.rate);out=Clip(a.channels(),count,a.rate);const auto grain=std::min(n,std::max<std::size_t>(8,static_cast<std::size_t>(std::clamp(p.b,10.,500.)*a.rate*.001)));const auto hop=std::max<std::size_t>(1,grain/4);std::vector<float>weights(count);std::mt19937 rng(p.seed);std::uniform_int_distribution<std::size_t>dist(0,n-grain);
        for(std::int64_t start=-static_cast<std::int64_t>(grain)+static_cast<std::int64_t>(hop);start<static_cast<std::int64_t>(count);start+=static_cast<std::int64_t>(hop)){const auto src=dist(rng);for(std::size_t j=0;j<grain;++j){const auto pos=start+static_cast<std::int64_t>(j);if(pos<0||pos>=static_cast<std::int64_t>(count))continue;const float w=grain==1?1.f:static_cast<float>(.5-.5*std::cos(2*pi*static_cast<double>(j)/static_cast<double>(grain-1)));weights[static_cast<std::size_t>(pos)]+=w;for(int c=0;c<a.channels();++c)out.data[c][static_cast<std::size_t>(pos)]+=a.data[c][src+j]*w;}}
        for(int c=0;c<a.channels();++c)for(std::size_t j=0;j<count;++j)if(weights[j]>.00001f)out.data[c][j]/=weights[j];break;}
    case Op::Shuffle:{cleanMarkers(markers,n);markers.insert(markers.begin(),0);markers.push_back(n);std::vector<std::size_t>order(markers.size()-1);for(std::size_t i=0;i<order.size();++i)order[i]=i;std::mt19937 rng(p.seed);std::shuffle(order.begin(),order.end(),rng);std::size_t dst=0;for(auto k:order){const auto len=markers[k+1]-markers[k];if(dst)result.markers.push_back(dst);for(int c=0;c<a.channels();++c)std::copy_n(a.data[c].begin()+static_cast<std::ptrdiff_t>(markers[k]),len,out.data[c].begin()+static_cast<std::ptrdiff_t>(dst));dst+=len;}break;}
    }
    for(const auto&c:out.data)for(float v:c)if(!std::isfinite(v))throw std::runtime_error("Audio invalido na operacao "+std::to_string(static_cast<int>(op)));return result;
}
}
