#pragma once
// INVERSO DRUM 0.1: portable fixed-voice sampler and host-synced sequencer core.
#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <cstdint>
#include <memory>
#include <vector>

namespace inverso_drum {
constexpr int pads=16, patterns=8, steps=16, voices=64;
constexpr double pi=3.1415926535897932384626433832795;
inline float clip(float v,float lo,float hi) noexcept{return std::max(lo,std::min(hi,v));}
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
  for(auto& g:gain)g.store(0.8f);
  for(auto& p:pan)p.store(0.f);
  for(int s=0;s<16;s+=4)grid[0][0][s].store(116);
  grid[0][1][4].store(110);grid[0][1][12].store(110);
  for(int s=0;s<16;s+=2)grid[0][2][s].store(s%4==0?75:57);
  for(int p=1;p<8;++p)for(int lane=0;lane<16;++lane)for(int step=0;step<16;++step)
   grid[p][lane][step].store(grid[0][lane][step].load());
 }
 void prepare(double hostRate){
  rate=(std::isfinite(hostRate) && hostRate>=8000)?hostRate:48000;
  for(auto& v:active)v=Voice{};
  lastStep=-1;
  for(int i=0;i<16;++i){
   // Factory samples are generated at prepare time; imported samples survive rate changes.
   if(!userLoaded[static_cast<size_t>(i)].load())setSample(i,factorySample(i,rate));
  }
 }
 void setSample(int pad,std::shared_ptr<const Sample> sample){
  if(pad>=0&&pad<16 && sample && sample->left.size()>1)
   std::atomic_store_explicit(&samples[static_cast<size_t>(pad)],std::move(sample),std::memory_order_release);
 }
 std::shared_ptr<const Sample> getSample(int pad) const {
  if(pad<0||pad>=16)return {};
  return std::atomic_load_explicit(&samples[static_cast<size_t>(pad)],std::memory_order_acquire);
 }
 void setUserLoaded(int pad,bool b){if(pad>=0&&pad<16)userLoaded[static_cast<size_t>(pad)].store(b);}
 bool isUserLoaded(int pad) const {return pad>=0&&pad<16&&userLoaded[static_cast<size_t>(pad)].load();}
 uint8_t get(int pat,int pad,int step)const noexcept{
  if(pat<0||pat>=8||pad<0||pad>=16||step<0||step>=16)return 0;
  return grid[pat][pad][step].load(std::memory_order_relaxed);
 }
 void set(int pat,int pad,int step,int velocity)noexcept{
  if(pat>=0&&pat<8&&pad>=0&&pad<16&&step>=0&&step<16)
   grid[pat][pad][step].store(static_cast<uint8_t>(std::clamp(velocity,0,127)),std::memory_order_relaxed);
 }
 void trigger(int pad,float velocity) noexcept {
  if(pad<0||pad>=16)return;
  auto sample=getSample(pad);
  if(!sample)return;
  // Fixed polyphony; replace the oldest voice when all voices are occupied.
  Voice* target=nullptr;uint64_t oldest=UINT64_MAX;
  for(auto& v:active){if(!v.sample){target=&v;break;}if(v.serial<oldest){oldest=v.serial;target=&v;}}
  if(!target)return;
  target->sample=std::move(sample);target->pos=0.;target->pad=pad;
  target->velocity=clip(velocity,0.f,1.f);target->serial=++voiceSerial;
  target->step=target->sample->sampleRate/rate;
 }
 void stopTransport() noexcept {lastStep=-1;currentStep.store(-1,std::memory_order_relaxed);}
 // Caller supplies host PPQ for this *sample*. Stable for arbitrary host buffer sizes.
 void transportSample(double ppq,bool playing) noexcept {
  if(!playing || !std::isfinite(ppq)){stopTransport();return;}
  const double swingAmount=static_cast<double>(clip(swing.load(std::memory_order_relaxed),0,1))*.115;
  long long stepIndex=static_cast<long long>(std::floor(ppq*4.0+1e-8));
  if((stepIndex&1LL) && ppq<stepIndex*.25+swingAmount) --stepIndex;
  if(stepIndex!=lastStep){
   // If playback begins between step boundaries, do not issue an unexpected trigger.
   const bool first=lastStep==-1;
   lastStep=stepIndex;
   const int local=static_cast<int>((stepIndex%16+16)%16);
   currentStep.store(local,std::memory_order_relaxed);
   const int pat=chain.load(std::memory_order_relaxed)?static_cast<int>(((stepIndex/16)%8+8)%8):selectedPattern.load(std::memory_order_relaxed);
   currentPattern.store(pat,std::memory_order_relaxed);
   if(!first || std::abs(ppq-stepIndex*.25)<.001){
    for(int lane=0;lane<16;++lane){const int vel=get(pat,lane,local);if(vel>0)trigger(lane,vel/127.f);}
   }
  }
 }
 void renderFrame(float& left,float& right) noexcept {
  double l=0,r=0;
  for(auto& v:active){
   if(!v.sample)continue;
   const auto& s=*v.sample;
   const size_t i=static_cast<size_t>(v.pos);
   if(i+1>=s.left.size()){v.sample.reset();continue;}
   const double f=v.pos-static_cast<double>(i);
   const float lo=static_cast<float>(s.left[i]*(1-f)+s.left[i+1]*f);
   const float ro=static_cast<float>(s.right.size()>i+1?(s.right[i]*(1-f)+s.right[i+1]*f):lo);
   const float g=gain[static_cast<size_t>(v.pad)].load(std::memory_order_relaxed)*v.velocity;
   const float panValue=clip(pan[static_cast<size_t>(v.pad)].load(std::memory_order_relaxed),-1,1);
   l+=lo*g*(panValue>0?1-panValue:1);
   r+=ro*g*(panValue<0?1+panValue:1);
   v.pos+=v.step;
   if(v.pos>=s.left.size()-1)v.sample.reset();
  }
  left=static_cast<float>(l);right=static_cast<float>(r);
 }
 std::array<std::array<std::array<std::atomic<uint8_t>,16>,16>,8> grid{};
 std::array<std::atomic<float>,16> gain{},pan{};
 std::atomic<int> selectedPattern{0},currentPattern{0},currentStep{-1};
 std::atomic<bool> chain{false};
 std::atomic<float> swing{0.f};
private:
 struct Voice{
  std::shared_ptr<const Sample> sample;
  double pos=0,step=1;float velocity=1;uint64_t serial=0;int pad=0;
 };
 std::array<Voice,voices> active{};
 std::array<std::shared_ptr<const Sample>,16> samples{};
 std::array<std::atomic<bool>,16> userLoaded{};
 double rate=48000;
 uint64_t voiceSerial=0;
 long long lastStep=-1;
};
}
