#pragma once
// INVERSO DRUM experimental: independent velocity and effect sends per event.
// Scheduling uses musical PPQ positions supplied by the VST host.
#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <cstdint>
#include <limits>
#include <memory>
#include <vector>

namespace inverso_drum {
constexpr int pads=16, patterns=8, steps=128, voices=64, pages=8;
constexpr double pi=3.1415926535897932384626433832795;
inline float clip(float v,float lo,float hi) noexcept {return std::max(lo,std::min(hi,v));}
inline double divisionPPQ(int division,int feel) noexcept {
 constexpr double bases[4]={1.0,.5,.25,.125}; // quarter, eighth, sixteenth, thirty-second
 constexpr double mods[3]={1.0,1.5,2.0/3.0}; // straight, dotted, triplet
 return bases[std::clamp(division,0,3)]*mods[std::clamp(feel,0,2)];
}
struct Sample {
 std::vector<float> left,right;
 double sampleRate=48000.0;
};
inline std::shared_ptr<const Sample> factorySample(int pad,double sampleRate){
 auto s=std::make_shared<Sample>();
 s->sampleRate=sampleRate;
 const int n=static_cast<int>(sampleRate*(pad==0?.65:(pad==1?.35:.23)));
 s->left.resize(static_cast<size_t>(n));s->right.resize(static_cast<size_t>(n));
 uint32_t seed=static_cast<uint32_t>(pad+1)*98137u;
 double phase=0;
 for(int i=0;i<n;++i){
  const float t=static_cast<float>(i/sampleRate);
  seed=seed*1664525u+1013904223u;
  const float noise=static_cast<float>((seed>>8)&0xffff)/32768.f-1.f;
  float y=0;
  if(pad==0 || pad==8){
   const float freq=pad==0?48.f:63.f;
   phase+=2*pi*(freq+88.f*std::exp(-t*41.f))/sampleRate;
   y=.84f*std::sin(phase)*std::exp(-t*13.f)+.18f*noise*std::exp(-t*110.f);
  }else if(pad==1 || pad==9){
   phase+=2*pi*(pad==1?185.:220.)/sampleRate;
   y=(.48f*noise+.33f*std::sin(phase))*std::exp(-t*18.f);
  }else if(pad==2 || pad==3 || pad==10 || pad==11){
   const float decay=(pad==3||pad==11)?13.f:56.f;
   y=.42f*noise*std::exp(-t*decay);
  }else{
   const float f=95.f+pad*49.f;
   phase+=2*pi*f/sampleRate;
   y=(.44f*std::sin(phase)+.28f*noise)*std::exp(-t*(12.f+pad*.7f));
  }
  s->left[static_cast<size_t>(i)]=y;s->right[static_cast<size_t>(i)]=y;
 }
 return s;
}
class Engine {
public:
 Engine(){
  for(auto& pat:grid)for(auto& lane:pat)for(auto& cell:lane)cell.store(0);
  for(auto& pattern:eventDrive)for(auto& lane:pattern)for(auto& val:lane)val.store(inheritFx);
  for(auto& pattern:eventVerb)for(auto& lane:pattern)for(auto& val:lane)val.store(inheritFx);
  for(auto& pattern:eventDelay)for(auto& lane:pattern)for(auto& val:lane)val.store(inheritFx);
  for(auto& g:gain)g.store(.8f);
  for(auto& p:pan)p.store(0.f);
  for(auto& a:laneLength)a.store(16);
  for(auto& a:division)a.store(2); // 1/16
  for(auto& a:feel)a.store(0);
  for(auto& a:sampleStart)a.store(0.f);
  for(auto& a:sampleEnd)a.store(1.f);
  for(auto& a:samplePitch)a.store(0.f);
  for(auto& a:sampleReverse)a.store(false);
  for(auto& a:sampleAttack)a.store(0.f);
  for(auto& a:sampleDecay)a.store(1.f);
  for(auto& a:drive)a.store(0.f);
  for(auto& a:sendVerb)a.store(.36f);
  for(auto& a:sendDelay)a.store(.24f);
  for(auto& a:currentLaneStep)a.store(-1);
  for(int s=0;s<16;s+=4)grid[0][0][s].store(116);
  grid[0][1][4].store(110);grid[0][1][12].store(110);
  for(int s=0;s<16;s+=2)grid[0][2][s].store(s%4==0?75:57);
  for(int p=1;p<patterns;++p)for(int lane=0;lane<pads;++lane)
   for(int st=0;st<16;++st)grid[p][lane][st].store(grid[0][lane][st].load());
 }
 void prepare(double hostRate){
  rate=(std::isfinite(hostRate) && hostRate>=8000)?hostRate:48000;
  for(auto& v:active)v=Voice{};
  stopTransport();
  for(int i=0;i<pads;++i)
   if(!userLoaded[static_cast<size_t>(i)].load())setSample(i,factorySample(i,rate));
 }
 void setSample(int pad,std::shared_ptr<const Sample> sample){
  if(pad>=0&&pad<pads&&sample&&sample->left.size()>1)
   std::atomic_store_explicit(&samples[static_cast<size_t>(pad)],std::move(sample),std::memory_order_release);
 }
 std::shared_ptr<const Sample> getSample(int pad)const{
  if(pad<0||pad>=pads)return {};
  return std::atomic_load_explicit(&samples[static_cast<size_t>(pad)],std::memory_order_acquire);
 }
 void setUserLoaded(int pad,bool b){if(pad>=0&&pad<pads)userLoaded[static_cast<size_t>(pad)].store(b);}
 bool isUserLoaded(int pad)const{return pad>=0&&pad<pads&&userLoaded[static_cast<size_t>(pad)].load();}
 uint8_t get(int pat,int pad,int step)const noexcept{
  if(pat<0||pat>=patterns||pad<0||pad>=pads||step<0||step>=steps)return 0;
  return grid[pat][pad][step].load(std::memory_order_relaxed);
 }
 void set(int pat,int pad,int step,int velocity)noexcept{
  if(pat>=0&&pat<patterns&&pad>=0&&pad<pads&&step>=0&&step<steps)
   grid[pat][pad][step].store(static_cast<uint8_t>(std::clamp(velocity,0,127)),std::memory_order_relaxed);
 }
 // 255 means "inherit pad"; explicit 0 means NO effect for this individual hit.
 static constexpr int inheritFx=255;
 void setEventFx(int pattern,int pad,int step,int fx,int value)noexcept{
  if(pattern<0||pattern>=patterns||pad<0||pad>=pads||step<0||step>=steps)return;
  const auto v=static_cast<uint8_t>(value==inheritFx?inheritFx:std::clamp(value,0,100));
  if(fx==0)eventDrive[pattern][pad][step].store(v,std::memory_order_relaxed);
  if(fx==1)eventVerb[pattern][pad][step].store(v,std::memory_order_relaxed);
  if(fx==2)eventDelay[pattern][pad][step].store(v,std::memory_order_relaxed);
 }
 int getEventFx(int pattern,int pad,int step,int fx)const noexcept{
  if(pattern<0||pattern>=patterns||pad<0||pad>=pads||step<0||step>=steps)return inheritFx;
  if(fx==0)return eventDrive[pattern][pad][step].load(std::memory_order_relaxed);
  if(fx==1)return eventVerb[pattern][pad][step].load(std::memory_order_relaxed);
  if(fx==2)return eventDelay[pattern][pad][step].load(std::memory_order_relaxed);
  return inheritFx;
 }
 void setLaneLength(int pad,int length)noexcept{
  if(pad<0||pad>=pads)return;
  laneLength[static_cast<size_t>(pad)].store(std::clamp(((length+2)/4)*4,4,steps),std::memory_order_relaxed);
  refreshSpan();
 }
 void setDivision(int pad,int rateIndex,int feelIndex)noexcept{
  if(pad<0||pad>=pads)return;
  division[static_cast<size_t>(pad)].store(std::clamp(rateIndex,0,3),std::memory_order_relaxed);
  feel[static_cast<size_t>(pad)].store(std::clamp(feelIndex,0,2),std::memory_order_relaxed);
  refreshSpan();
 }
 double patternSpanPPQ()const noexcept{return spanPPQ.load(std::memory_order_relaxed);}
 void refreshSpan()noexcept{
  double largest=4.;
  for(int i=0;i<pads;++i){
   const double duration=divisionPPQ(division[i].load(),feel[i].load());
   largest=std::max(largest,laneLength[i].load()*duration);
  }
  spanPPQ.store(std::ceil(largest/4.-1e-9)*4.,std::memory_order_relaxed);
 }
 // Pattern & step are -1 for MIDI/audition notes, which inherit the pad settings.
 void trigger(int pad,float velocity,int eventPattern=-1,int eventStep=-1)noexcept{
  if(pad<0||pad>=pads)return;
  auto sample=getSample(pad);if(!sample)return;
  Voice* dest=nullptr;uint64_t oldest=UINT64_MAX;
  for(auto& v:active){
   if(!v.sample){dest=&v;break;}
   if(v.serial<oldest){oldest=v.serial;dest=&v;}
  }
  if(!dest)return;
  const int total=static_cast<int>(sample->left.size());
  if(total<3)return;
  const float start=clip(sampleStart[pad].load(),0.f,.995f);
  const float end=clip(sampleEnd[pad].load(),start+.004f,1.f);
  const int first=std::clamp(static_cast<int>(start*(total-1)),0,total-3);
  const int last=std::clamp(static_cast<int>(end*(total-1)),first+2,total-1);
  dest->sample=std::move(sample);
  dest->first=first;dest->last=last;
  dest->reverse=sampleReverse[pad].load();
  dest->pos=dest->reverse?static_cast<double>(last-1):static_cast<double>(first);
  dest->pad=pad;dest->age=0;
  dest->velocity=clip(velocity,0.f,1.f);dest->serial=++voiceSerial;
  const auto effectValue=[this,pad,eventPattern,eventStep](int fx,float padDefault)noexcept{
   if(eventPattern<0||eventStep<0)return clip(padDefault,0.f,1.f);
   const int overrideValue=getEventFx(eventPattern,pad,eventStep,fx);
   return overrideValue==inheritFx?clip(padDefault,0.f,1.f):overrideValue*.01f;
  };
  dest->effectDrive=effectValue(0,drive[pad].load());
  dest->effectVerb=effectValue(1,sendVerb[pad].load());
  dest->effectDelay=effectValue(2,sendDelay[pad].load());
  const float pitch=clip(samplePitch[pad].load(),-24,24);
  dest->step=std::pow(2.,pitch/12.)*dest->sample->sampleRate/rate;
  dest->attackFrames=rate*.1*clip(sampleAttack[pad].load(),0,1);
  const float dec=clip(sampleDecay[pad].load(),0,1);
  dest->decaySeconds=.035+12.*dec*dec;
 }
 void stopTransport()noexcept{
  for(auto& x:lastLaneIndex)x=std::numeric_limits<long long>::min();
  currentStep.store(-1);for(auto& s:currentLaneStep)s.store(-1);
 }
 // PPQ is supplied for this single sample, not calculated by a local unsynchronised clock.
 void transportSample(double ppq,bool playing)noexcept{
  if(!playing||!std::isfinite(ppq)||std::abs(ppq)>1e8){stopTransport();return;}
  const double span=std::max(4.,patternSpanPPQ());
  const double pos=std::max(0.,ppq);
  const bool chained=chain.load(std::memory_order_relaxed);
  const auto barIndex=static_cast<long long>(std::floor(pos/span));
  const int pat=chained?static_cast<int>(barIndex%patterns):std::clamp(selectedPattern.load(),0,patterns-1);
  currentPattern.store(pat,std::memory_order_relaxed);
  const double localPPQ=chained?pos-static_cast<double>(barIndex)*span:pos;
  const double swingAmt=clip(swing.load(),0,1)*.46;
  for(int lane=0;lane<pads;++lane){
   const double duration=divisionPPQ(division[lane].load(),feel[lane].load());
   const int length=std::clamp(laneLength[lane].load(),1,steps);
   long long absolute=static_cast<long long>(std::floor(localPPQ/duration+1e-9));
   if((absolute&1LL) && localPPQ<static_cast<double>(absolute)*duration+swingAmt*duration) --absolute;
   const int cell=static_cast<int>((absolute%length+length)%length);
   const long long key=absolute+static_cast<long long>(pat)*1000000000LL;
   currentLaneStep[lane].store(cell,std::memory_order_relaxed);
   if(lastLaneIndex[lane]!=key){
    const bool first=lastLaneIndex[lane]==std::numeric_limits<long long>::min();
    lastLaneIndex[lane]=key;
    const double boundary=static_cast<double>(absolute)*duration+((absolute&1LL)?swingAmt*duration:0.);
    // Avoid triggering mid-step when host starts at an arbitrary PPQ position.
    if(!first||std::abs(localPPQ-boundary)<.002){
     const int vel=get(pat,lane,cell);if(vel)trigger(lane,vel/127.f,pat,cell);
    }
   }
  }
  currentStep.store(currentLaneStep[0].load(),std::memory_order_relaxed);
 }
 // Dry output plus independent FX sends; effects are processed outside the engine.
 void renderFrame(float& left,float& right,float& verbL,float& verbR,float& delL,float& delR)noexcept{
  double l=0,r=0,vl=0,vr=0,dl=0,dr=0;
  for(auto& v:active){
   if(!v.sample)continue;
   const auto& s=*v.sample;
   const int i=static_cast<int>(v.pos);
   if(i<v.first||i+1>v.last){v.sample.reset();continue;}
   const double f=v.pos-static_cast<double>(i);
   const float lo=static_cast<float>(s.left[static_cast<size_t>(i)]*(1-f)+s.left[static_cast<size_t>(i+1)]*f);
   const float ro=static_cast<float>(s.right.size()>static_cast<size_t>(i+1)?
      (s.right[static_cast<size_t>(i)]*(1-f)+s.right[static_cast<size_t>(i+1)]*f):lo);
   const float a=v.attackFrames>1?clip(static_cast<float>(v.age/v.attackFrames),0,1):1.f;
   const float envelope=a*static_cast<float>(std::exp(-v.age/(rate*v.decaySeconds)));
   const float g=gain[v.pad].load()*v.velocity*envelope;
   const float p=clip(pan[v.pad].load(),-1,1);
   float yl=lo*g*(p>0?1-p:1),yr=ro*g*(p<0?1+p:1);
   const float d=v.effectDrive;
   if(d>.00001f){
    const float inputGain=1.f+22.f*d*d;
    const float mix=.85f*d;
    const auto saturate=[inputGain,mix](float x){
     const float color=.68f*std::tanh(inputGain*x);
     return x+mix*(color-x);
    };
    yl=saturate(yl);yr=saturate(yr);
   }
   l+=yl;r+=yr;
   const float verb=v.effectVerb,del=v.effectDelay;
   vl+=yl*verb;vr+=yr*verb;dl+=yl*del;dr+=yr*del;
   v.pos+=v.reverse?-v.step:v.step;v.age++;
   if(v.pos<v.first||v.pos+1>v.last)v.sample.reset();
  }
  left=static_cast<float>(l);right=static_cast<float>(r);
  verbL=static_cast<float>(vl);verbR=static_cast<float>(vr);
  delL=static_cast<float>(dl);delR=static_cast<float>(dr);
 }
 void renderFrame(float& left,float& right)noexcept{
  float vl=0,vr=0,dl=0,dr=0;renderFrame(left,right,vl,vr,dl,dr);
 }
 std::array<std::array<std::array<std::atomic<uint8_t>,steps>,pads>,patterns> grid{};
 // Per-event FX: values 0..100% or 255 (inherit the corresponding pad setting).
 std::array<std::array<std::array<std::atomic<uint8_t>,steps>,pads>,patterns> eventDrive{},eventVerb{},eventDelay{};
 std::array<std::atomic<float>,pads> gain{},pan{},sampleStart{},sampleEnd{},samplePitch{},sampleAttack{},sampleDecay{},drive{},sendVerb{},sendDelay{};
 std::array<std::atomic<bool>,pads> sampleReverse{};
 std::array<std::atomic<int>,pads> laneLength{},division{},feel{},currentLaneStep{};
 std::atomic<int> selectedPattern{0},currentPattern{0},currentStep{-1};
 std::atomic<bool> chain{false};
 std::atomic<float> swing{0};
private:
 struct Voice {
  std::shared_ptr<const Sample> sample;
  double pos=0,step=1,age=0,attackFrames=0,decaySeconds=12;
  float velocity=1,effectDrive=0,effectVerb=0,effectDelay=0;uint64_t serial=0;int pad=0,first=0,last=0;bool reverse=false;
 };
 std::array<Voice,voices> active{};
 std::array<std::shared_ptr<const Sample>,pads> samples{};
 std::array<std::atomic<bool>,pads> userLoaded{};
 std::array<long long,pads> lastLaneIndex{};
 std::atomic<double> spanPPQ{4.};
 double rate=48000.;
 uint64_t voiceSerial=0;
};
}
