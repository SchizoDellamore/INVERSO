#pragma once
// INVERSO PROCESSORS 0.1 - Portable, allocation-free stereo DSP.
// Dynamic band suppression, NOT a resonance-tracking spectral processor.
// Per-band RMS vs broadband RMS; envelopes and gain are smoothly updated.
// Neither a de-esser nor a brick-wall limiter.
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>

namespace inverso_proc {
constexpr double pi=3.14159265358979323846;
constexpr int maxBands=7;
enum class Kind { Mudder, Annoyance };
inline float clip(float v,float lo,float hi) noexcept {return std::max(lo,std::min(hi,v));}

struct Parameters {
 float amount=0.45f;       // 0..1
 float sensitivity=0.50f;  // 0..1
 float focus=0.5f;        // 0..1 shifts band placement by ~0.75-1.35
 bool bypass=false;
 bool delta=false;        // audition removed signal (not level matched)
};

struct Meter {
 float reductionDb=0.f;
 float inputPeak=0.f;
 float outputPeak=0.f;
 std::array<float,maxBands> bandReductionDb {};
};

struct BandPass {
 double b0=0,b2=0,a1=0,a2=0;
 double z1=0,z2=0;
 void reset() noexcept {z1=z2=0;}
 void tune(double rate,double frequency,double q) noexcept {
  const double f=std::clamp(frequency,25.,rate*0.43);
  const double w=2*pi*f/rate, a=std::sin(w)/(2*q);
  const double inv=1.0/(1+a);
  b0=a*inv; b2=-a*inv; a1=-2*std::cos(w)*inv; a2=(1-a)*inv;
 }
 float tick(float x) noexcept {
  const double y=b0*x+z1;
  z1=-a1*y+z2;
  z2=b2*x-a2*y;
  if(std::abs(z1)<1e-27)z1=0;
  if(std::abs(z2)<1e-27)z2=0;
  return static_cast<float>(y);
 }
};

class AdaptiveSuppressor {
public:
 explicit AdaptiveSuppressor(Kind kind) noexcept:kind_(kind){}
 void prepare(double sampleRate) noexcept {
  sr=std::clamp(sampleRate,8000.0,384000.0);
  for(auto& channel:filters)for(auto& f:channel)f.reset();
  smoothAmount=0;smoothSensitivity=0.5f;smoothFocus=0.5f;
  globalPower=0;power.fill(0);depth.fill(0);
  processed=0;meter={};byMix=0;deltaMix=0;
  const double attackMs=kind_==Kind::Mudder?18.:8.;
  attack=std::exp(-1.0/(0.001*attackMs*sr));
  release=std::exp(-1.0/(0.001*(kind_==Kind::Mudder?170.:110.)*sr));
  smooth=1.-std::exp(-1.0/(0.012*sr));
  configure(0.5f);
 }
 void setParameters(Parameters p) noexcept {
  pending.amount=clip(p.amount,0,1);
  pending.sensitivity=clip(p.sensitivity,0,1);
  pending.focus=clip(p.focus,0,1);
  pending.bypass=p.bypass;pending.delta=p.delta;
 }
 // Audio-thread only: 2 channels linked, no allocations or locks.
 Meter process(const float* left,const float* right,float* outL,float* outR,int samples) noexcept {
  Meter block{};
  if(samples<=0)return block;
  for(int i=0;i<samples;++i){
   // Smoothed parameters prevent abrupt modulation while automating.
   smoothAmount+=static_cast<float>(smooth)*(pending.amount-smoothAmount);
   smoothSensitivity+=static_cast<float>(smooth)*(pending.sensitivity-smoothSensitivity);
   smoothFocus+=static_cast<float>(smooth)*(pending.focus-smoothFocus);
   byMix+=static_cast<float>(smooth)*((pending.bypass?1.f:0.f)-byMix);
   deltaMix+=static_cast<float>(smooth)*((pending.delta?1.f:0.f)-deltaMix);
   // 32-sample coefficient updates. Focus parameter sweeps slowly.
   if((processed++ & 31U)==0U)configure(smoothFocus);
   float in[2]={left[i],right[i]};
   const float monoPower=0.5f*(in[0]*in[0]+in[1]*in[1]);
   globalPower=envelope(globalPower,monoPower);
   float filtered[2][maxBands]{};
   for(int b=0;b<bandCount();++b){
    for(int ch=0;ch<2;++ch)filtered[ch][b]=filters[ch][b].tick(in[ch]);
    const float bp=0.5f*(filtered[0][b]*filtered[0][b]+filtered[1][b]*filtered[1][b]);
    power[b]=envelope(power[b],bp);
    const float ratio=std::sqrt(power[b]/(globalPower+1e-9f));
    // A gain threshold that scales with track level, without fixed dBFS gating.
    const float base=kind_==Kind::Mudder?0.20f:0.065f;
    const float threshold=base*(1.5f-1.0f*smoothSensitivity);
    const float excess=clip((ratio-threshold)/(threshold*1.65f),0.f,1.f);
    const float target=excess * smoothAmount * (kind_==Kind::Mudder?0.42f:0.54f);
    const float envCoef=target>depth[b]?0.010f:0.0025f;
    depth[b]+=envCoef*(target-depth[b]);
    const float rd=-20.f*std::log10(std::max(0.001f,1.f-depth[b]));
    block.bandReductionDb[b]=std::max(block.bandReductionDb[b],rd);
   }
   for(int ch=0;ch<2;++ch){
    float removed=0;
    for(int b=0;b<bandCount();++b)removed+=depth[b]*filtered[ch][b];
    const float wet=in[ch]-removed;
    const float mixed=(1.f-deltaMix)*wet+deltaMix*removed;
    const float output=byMix*in[ch]+(1.f-byMix)*mixed;
    if(ch==0)outL[i]=output;else outR[i]=output;
    block.inputPeak=std::max(block.inputPeak,std::abs(in[ch]));
    block.outputPeak=std::max(block.outputPeak,std::abs(output));

   }
  }
  for (int b=0;b<bandCount();++b) block.reductionDb=std::max(block.reductionDb,block.bandReductionDb[b]);
  meter=block;
  return block;
 }
 int bandCount()const noexcept{return kind_==Kind::Mudder?3:7;}
 Kind kind()const noexcept{return kind_;}
 std::array<float,maxBands> centresHz()const noexcept {return currentCentre;}
private:
 float envelope(float current,float signal) const noexcept {
  const double c=signal>current?attack:release;
  return static_cast<float>(c*current+(1.-c)*signal);
 }
 void configure(float focus) noexcept {
  static constexpr float mud[3]={160,275,440};
  static constexpr float irrit[7]={1150,1600,2250,3150,4400,6100,8200};
  const float factor=std::pow(2.f,(focus-.5f)*.7f);
  for(int b=0;b<bandCount();++b){
   const auto hz=(kind_==Kind::Mudder?mud[b]:irrit[b])*factor;
   currentCentre[b]=hz;
   const auto q=kind_==Kind::Mudder?(0.90+0.18*b):5.0;
   for(int c=0;c<2;++c)filters[c][b].tune(sr,hz,q);
  }
 }
 Kind kind_;
 Parameters pending{};
 std::array<std::array<BandPass,maxBands>,2> filters{};
 std::array<float,maxBands> currentCentre{},power{},depth{};
 double sr=48000,attack=0.99,release=0.999,smooth=.00025;
 float globalPower=0,smoothAmount=0,smoothSensitivity=.5f,smoothFocus=.5f,byMix=0,deltaMix=0;
 std::uint64_t processed=0;
 Meter meter{};
 int meterDelay=0;
};
}
