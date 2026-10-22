#pragma once
#include "DrumEngine.h"
#include <array>
#include <atomic>
#include <vector>

namespace inverso_drum {
class Effects {
public:
 void prepare(double sampleRate) {
  sr=std::clamp(sampleRate,8000.,384000.);
  const int maxDelay=static_cast<int>(sr*3.)+4;
  for(auto& line:delay)line.assign(static_cast<size_t>(maxDelay),0.f);
  write=0;
  constexpr int combSizes[2][4]={{1557,1617,1491,1422},{1587,1649,1511,1447}};
  constexpr int allpassSizes[2][2]={{225,556},{248,579}};
  for(int channel=0;channel<2;++channel){
   for(int i=0;i<4;++i){
    auto& c=combs[channel][i];
    c.buffer.assign(static_cast<size_t>(std::max(12,static_cast<int>(combSizes[channel][i]*sr/48000.))),0.f);c.index=0;
   }
   for(int i=0;i<2;++i){
    auto& a=allpass[channel][i];
    a.buffer.assign(static_cast<size_t>(std::max(12,static_cast<int>(allpassSizes[channel][i]*sr/48000.))),0.f);a.index=0;
   }
  }
  setTempo(120.);
 }
 void setTempo(double bpm) noexcept {
  if(!std::isfinite(bpm)||bpm<20||bpm>400)bpm=120.;
  const double beats=divisionPPQ(delayDivision.load(),delayFeel.load());
  const int maximum=static_cast<int>(delay[0].size())-2;
  delayFrames=std::clamp(static_cast<int>(std::round(sr*60./bpm*beats)),1,std::max(1,maximum));
 }
 void process(float inputVerbL,float inputVerbR,float inputDelayL,float inputDelayR,float& left,float& right) noexcept {
  const float feedback=clip(delayFeedback.load(),0.f,.9f);
  const float delayMix=clip(delayWet.load(),0.f,1.f);
  const float reverbMix=clip(reverbWet.load(),0.f,1.f);
  const float feedbackVerb=.68f+.26f*clip(reverbDecay.load(),0.f,1.f);
  float rev[2]{};
  for(int ch=0;ch<2;++ch){
   const float signal=clip(ch?inputVerbR:inputVerbL,-4.f,4.f)*.82f;
   float sum=0;
   for(auto& c:combs[ch]){
    const float y=c.buffer[c.index];
    c.buffer[c.index]=signal+feedbackVerb*y;
    if(++c.index>=c.buffer.size())c.index=0;
    sum+=y;
   }
   float val=sum*.25f;
   for(auto& a:allpass[ch]){
    const float buffered=a.buffer[a.index];
    const float output=buffered-.5f*val;
    a.buffer[a.index]=val+.5f*buffered;
    if(++a.index>=a.buffer.size())a.index=0;
    val=output;
   }
   rev[ch]=val*1.2f;
  }
  const int size=static_cast<int>(delay[0].size());
  const int read=(write+size-delayFrames)%size;
  const float dL=delay[0][static_cast<size_t>(read)];
  const float dR=delay[1][static_cast<size_t>(read)];
  delay[0][static_cast<size_t>(write)]=clip(inputDelayL+feedback*dL,-8.f,8.f);
  delay[1][static_cast<size_t>(write)]=clip(inputDelayR+feedback*dR,-8.f,8.f);
  if(++write>=size)write=0;
  left+=reverbMix*rev[0]+delayMix*dL;
  right+=reverbMix*rev[1]+delayMix*dR;
  if(!std::isfinite(left))left=0;
  if(!std::isfinite(right))right=0;
 }
 std::atomic<float> reverbWet{.55f},reverbDecay{.55f},delayWet{.45f},delayFeedback{.34f};
 std::atomic<int> delayDivision{1},delayFeel{0};
private:
 struct Line{std::vector<float> buffer;size_t index=0;};
 std::array<std::vector<float>,2> delay{};
 std::array<std::array<Line,4>,2> combs{};
 std::array<std::array<Line,2>,2> allpass{};
 int write=0,delayFrames=24000;
 double sr=48000.;
};
}
