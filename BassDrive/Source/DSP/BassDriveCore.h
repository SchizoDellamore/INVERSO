#pragma once
// INVERSO BASS DRIVE 0.3: real-time, fixed-memory, host-rate DSP.
// Broad musical drive and three optional fuzz characters. No block-size buffers.
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>

namespace inverso_bass {
constexpr double pi = 3.1415926535897932384626433832795;
constexpr int maxChannels = 2;
enum class FuzzMode : int { Off=0, Vintage=1, Silicon=2, Gated=3 };
inline float clamp(float x, float a, float b) noexcept { return std::max(a, std::min(b,x)); }
inline float dbGain(float dB) noexcept { return std::pow(10.f, dB / 20.f); }
struct Parameters {
 float drive=.25f, fuzz=0.f, crossover=155.f, tone=.65f, mix=1.f, outputDb=0.f;
 float body=.65f; // full-bodied low-mid enhancement, independent of fuzz selection
 bool preserveLow=true;
 FuzzMode mode=FuzzMode::Off;
};

class Core {
public:
 void prepare(double sampleRate, int channels) noexcept {
  sr=std::isfinite(sampleRate)?std::clamp(sampleRate,8000.0,384000.0):48000.0;
  count=std::clamp(channels,1,maxChannels);
  for(auto& c:state)c={};
  smDrive=0;smFuzz=0;smMix=1;smTone=.65f;smXover=155.f;smBody=.65f;
  smOutput=1.f;smPreserve=1.f;frames=0;
  step=1.f-std::exp(-1.f/static_cast<float>(sr*.012));
  updateCoefficients();
 }
 void setParameters(Parameters p) noexcept {
  p.drive=clamp(p.drive,0,1);p.fuzz=clamp(p.fuzz,0,1);
  p.crossover=clamp(p.crossover,60,500);p.tone=clamp(p.tone,0,1);
  p.mix=clamp(p.mix,0,1);p.outputDb=clamp(p.outputDb,-24,12);
  p.body=clamp(p.body,0,1);
  p.mode=static_cast<FuzzMode>(std::clamp(static_cast<int>(p.mode),0,3));
  target=p;
 }
 // processChannel for each channel, THEN tickFrame once. Supports in-place input.
 float processChannel(int ch, float x) noexcept {
  ch=std::clamp(ch,0,count-1);
  auto& s=state[static_cast<std::size_t>(ch)];
  if(!std::isfinite(x))return 0.f;
  // The 2nd-order crossover opens up bass harmonics better than v0.2's 4th-order HP.
  const float hp=s.hp.tick(x,hpCoeffs);
  const float high=hp*smPreserve+x*(1.f-smPreserve);
  const float low=x-high; // mathematically complementary: no hollowed-out bass
  // Gentle band-limited 65-310 Hz body enhancement, rather than boosting deep subbass.
  s.bodyLo+=bodyLoA*(x-s.bodyLo);
  s.bodyHi+=bodyHiA*(x-s.bodyHi);
  const float bodyBand=s.bodyHi-s.bodyLo;
  const float activation=std::max(smDrive,target.mode==FuzzMode::Off?0.f:smFuzz*.55f);
  const float bodyLift=smBody*activation*1.35f*bodyBand;

  // Parallel asymmetric preamp. Higher drive = richer even/odd harmonics,
  // but some direct signal is kept to prevent the compressed, lifeless v0.2 tone.
  float driven=high;
  if(smDrive>0.00002f) {
    const float gain=1.f+28.f*smDrive*smDrive;
    const float bias=.075f*smDrive;
    const float shaped=.68f*(std::tanh((high+bias)*gain)-std::tanh(bias*gain));
    const float saturationMix=std::min(.88f,smDrive*2.4f);
    const float preamped=high+saturationMix*(shaped-high);
    s.driveTone+=driveToneA*(preamped-s.driveTone);
    driven=preamped*.80f+s.driveTone*.20f;
  } else s.driveTone=high;

  float colored=driven;
  if(target.mode!=FuzzMode::Off && smFuzz>0.00002f) {
    const float gain=2.5f+26.f*smFuzz*smFuzz;
    float fuzzed=0.f;
    switch(target.mode) {
      case FuzzMode::Vintage:
        // Asymmetrical soft clip: a rounded, dense and harmonically rich fuzz.
        fuzzed=.86f*(std::tanh(high*gain*.87f+.33f)-std::tanh(.33f));
        break;
      case FuzzMode::Silicon:
        // Hard clipping: unmistakable, aggressive harmonic edge.
        fuzzed=1.72f*clamp(high*gain,-.43f,.43f);
        break;
      case FuzzMode::Gated: {
        const float a=std::abs(high);
        const float e=(a>s.gateEnvelope?gateAttack:gateRelease);
        s.gateEnvelope+=e*(a-s.gateEnvelope);
        const float threshold=.017f+smFuzz*.045f;
        const float gate=clamp((s.gateEnvelope-threshold)/(.7f*threshold+1.e-7f),0.f,1.f);
        fuzzed=.84f*std::tanh(high*gain*1.7f)*gate;
        break;
      }
      default: break;
    }
    // Remove DC introduced by asymmetric circuits and tame fizzy highs.
    const float ac=fuzzed-s.dcInput+dcCoefficient*s.dcOutput;
    s.dcInput=fuzzed;s.dcOutput=ac;
    s.fuzzTone+=fuzzToneA*(ac-s.fuzzTone);
    const float fuzzBlend=std::min(1.f,smFuzz*1.25f);
    // Independent fuzz layered on top of the primary drive character.
    colored=driven*(1.f-fuzzBlend)+s.fuzzTone*fuzzBlend;
  } else {
    s.dcInput=0.f;s.dcOutput=0.f;
    s.fuzzTone*=.999f;
    s.gateEnvelope*=1.f-gateRelease;
  }

  const float wet=low+colored+bodyLift;
  const float result=(x+smMix*(wet-x))*smOutput;
  if(!std::isfinite(result)){s={};return x;} // fail open, never mute a valid input
  return result;
 }
 void tickFrame() noexcept {
  smDrive+=step*(target.drive-smDrive);
  smFuzz+=step*(target.fuzz-smFuzz);
  smMix+=step*(target.mix-smMix);
  smTone+=step*(target.tone-smTone);
  smXover+=step*(target.crossover-smXover);
  smBody+=step*(target.body-smBody);
  smOutput+=step*(dbGain(target.outputDb)-smOutput);
  smPreserve+=step*((target.preserveLow?1.f:0.f)-smPreserve);
  if((++frames&31U)==0U)updateCoefficients();
 }
 int numChannels()const noexcept{return count;}
private:
 struct Coeffs{double b0=1,b1=0,b2=0,a1=0,a2=0;};
 struct Biquad {
  double z1=0,z2=0;
  float tick(float x,const Coeffs& c) noexcept {
   const double y=c.b0*x+z1;
   z1=c.b1*x-c.a1*y+z2;
   z2=c.b2*x-c.a2*y;
   if(std::abs(z1)<1e-25)z1=0;
   if(std::abs(z2)<1e-25)z2=0;
   return static_cast<float>(y);
  }
 };
 struct Channel{Biquad hp{};float bodyLo=0,bodyHi=0,driveTone=0,fuzzTone=0,dcInput=0,dcOutput=0,gateEnvelope=0;};
 void updateCoefficients() noexcept {
  const double w=2.*pi*smXover/sr;
  const double c=std::cos(w),a=std::sin(w)/(2.*.7071067811865476),n=1./(1.+a);
  hpCoeffs.b0=.5*(1.+c)*n;hpCoeffs.b1=-(1.+c)*n;hpCoeffs.b2=hpCoeffs.b0;
  hpCoeffs.a1=-2.*c*n;hpCoeffs.a2=(1.-a)*n;
  const auto lp=[this](float hz){return 1.f-std::exp(-2.f*static_cast<float>(pi)*hz/static_cast<float>(sr));};
  bodyLoA=lp(65.f);bodyHiA=lp(310.f);
  driveToneA=lp(7800.f);
  fuzzToneA=lp(800.f*std::pow(18.f,smTone));
  gateAttack=1.f-std::exp(-1.f/static_cast<float>(sr*.002));
  gateRelease=1.f-std::exp(-1.f/static_cast<float>(sr*.055));
  dcCoefficient=std::exp(-2.f*static_cast<float>(pi)*16.f/static_cast<float>(sr));
 }
 Parameters target{};
 std::array<Channel,maxChannels> state{};
 double sr=48000.0;
 Coeffs hpCoeffs{};
 float smDrive=0,smFuzz=0,smMix=1,smTone=.65f,smXover=155,smBody=.65f,smOutput=1,smPreserve=1;
 float bodyLoA=.01f,bodyHiA=.05f,driveToneA=.4f,fuzzToneA=.2f,gateAttack=.01f,gateRelease=.001f,dcCoefficient=.999f,step=.001f;
 std::uint64_t frames=0;
 int count=2;
};
}
