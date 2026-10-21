#pragma once
// INVERSO BASS DRIVE 0.1 | allocation-free, portable real-time DSP core.
// Run at an oversampled host rate (4x in the VST3 wrapper).
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <limits>

namespace inverso_bass {
constexpr double pi = 3.14159265358979323846;
constexpr int maxChannels=2;
enum class FuzzMode : int { Off=0, Vintage=1, Silicon=2, Gated=3 };
inline float clamp(float x,float a,float b) noexcept { return std::max(a,std::min(b,x)); }
inline float dbGain(float dB) noexcept { return std::pow(10.f,dB/20.f); }

struct Parameters {
 float drive=.25f;     // 0..1, warm saturation of the high band
 float fuzz=0.f;       // 0..1, independent fuzz intensity
 float crossover=155.f;// 60..500 Hz low frequency preservation crossover
 float tone=.65f;      // 0..1, fuzz post-filter
 float mix=1.f;        // wet/dry 0..1
 float outputDb=0.f;   // -24..+12 dB
 bool preserveLow=true;
 FuzzMode mode=FuzzMode::Off;
};
struct Meters { float inputPeak=0,outputPeak=0,oversampledPeak=0; };

class Core {
public:
 void prepare(double sampleRate,int channels) noexcept {
  sr=std::clamp(sampleRate,8000.0,1536000.0);
  count=std::clamp(channels,1,maxChannels);
  for(auto &s:channel)s={};
  smoothDrive=0;smoothFuzz=0;smoothMix=1;
  smoothXover=155;smoothTone=.65f;smoothOutput=1;smoothPreserve=1;
  step=1.f-std::exp(-1.f/static_cast<float>(sr*.02));
  updateFilter();
  frames=0;
 }
 void setParameters(Parameters p) noexcept {
  p.drive=clamp(p.drive,0,1);p.fuzz=clamp(p.fuzz,0,1);
  p.crossover=clamp(p.crossover,60,500);p.tone=clamp(p.tone,0,1);
  p.mix=clamp(p.mix,0,1);p.outputDb=clamp(p.outputDb,-24,12);
  p.mode=static_cast<FuzzMode>(std::clamp(static_cast<int>(p.mode),0,3));
  target=p;
 }
 // All channel calls belonging to a single sample frame must occur before the next tickFrame().
 // Audio thread only. Inputs and outputs may alias safely.
 float processChannel(int ch,float input) noexcept {
  ch=std::clamp(ch,0,count-1);
  auto& s=channel[static_cast<std::size_t>(ch)];
  const float dry=input;
  const float highpass=s.hp2.tick(s.hp1.tick(dry, coefficients), coefficients);
  const float high=highpass*smoothPreserve+dry*(1.f-smoothPreserve);
  const float low=dry-high;

  // Drive=0 and Fuzz=0 preserve exact dry high signal; no baseline coloration.
  const float driveGain=1.f+18.f*smoothDrive*smoothDrive;
  const float warm=high+smoothDrive*(std::tanh(high*driveGain)/std::tanh(driveGain)-high);
  float colored=warm;
  if(target.mode!=FuzzMode::Off && smoothFuzz>0.00001f) {
    const float fg=1.f+25.f*smoothFuzz*smoothFuzz;
    float fuzzWave=high;
    switch(target.mode){
      case FuzzMode::Vintage: {
        // Slightly asymmetric soft clipping, plus DC suppression after shaping.
        const float biased=std::tanh((high+.085f)*fg)-std::tanh(.085f*fg);
        fuzzWave=biased/std::max(.75f,std::tanh(fg));
        break;
      }
      case FuzzMode::Silicon: {
        // Firm, symmetrical clipping; 4x oversampling reduces aliasing but cannot eliminate it.
        fuzzWave=std::tanh(high*fg*2.5f);
        break;
      }
      case FuzzMode::Gated: {
        const float absx=std::abs(high);
        const float envC=absx>s.gateEnv?gateAttack:gateRelease;
        s.gateEnv += envC*(absx-s.gateEnv);
        const float threshold=.006f+smoothFuzz*.06f;
        const float gate=clamp((s.gateEnv-threshold)/(threshold*.6f+1.e-7f),0.f,1.f);
        fuzzWave=std::tanh((high+.02f)*fg*1.8f)*gate;
        break;
      }
      default: break;
    }
    // DC blocker applied only in fuzz path; prevents asymmetric DC offset.
    const float dcOut=fuzzWave-s.dcIn+dcCoef*s.dcOut;
    s.dcIn=fuzzWave;s.dcOut=dcOut;
    s.tone += alphaTone*(dcOut-s.tone);
    colored=warm+smoothFuzz*(s.tone-warm);
  } else {
    // Keep filter state warm through mode changes, avoiding old retained samples.
    s.tone += alphaTone*(high-s.tone);
    s.dcIn=high;s.dcOut=high;
    s.gateEnv *= (1.f-gateRelease);
  }
  const float wet=low+colored;
  const float mixed=dry+smoothMix*(wet-dry);
  const float out=mixed*smoothOutput;
  if(!std::isfinite(out)) {s={}; return 0.f;}
  return out;
 }
 void tickFrame() noexcept {
  smoothDrive+=step*(target.drive-smoothDrive);
  smoothFuzz+=step*(target.fuzz-smoothFuzz);
  smoothMix+=step*(target.mix-smoothMix);
  smoothTone+=step*(target.tone-smoothTone);
  smoothXover+=step*(target.crossover-smoothXover);
  smoothOutput+=step*(dbGain(target.outputDb)-smoothOutput);
  smoothPreserve+=step*((target.preserveLow?1.f:0.f)-smoothPreserve);
  if((++frames&31U)==0U) updateFilter();
 }
 int numChannels()const noexcept{return count;}
private:
 struct HPCoeffs { double b0=1,b1=0,b2=0,a1=0,a2=0; };
 struct HPState {
  double z1=0,z2=0;
  float tick(float x,const HPCoeffs& c) noexcept {
   const double y=c.b0*x+z1;
   z1=c.b1*x-c.a1*y+z2;
   z2=c.b2*x-c.a2*y;
   if(std::abs(z1)<1e-24)z1=0;
   if(std::abs(z2)<1e-24)z2=0;
   return static_cast<float>(y);
  }
 };
 struct Channel { HPState hp1{},hp2{};float tone=0,dcIn=0,dcOut=0,gateEnv=0;};
 void updateFilter() noexcept {
  const double w=2*pi*smoothXover/sr;
  const double cosine=std::cos(w),alpha=std::sin(w)/(2*0.7071067811865476);
  const double norm=1.0/(1.0+alpha);
  coefficients.b0=.5*(1+cosine)*norm;
  coefficients.b1=-(1+cosine)*norm;
  coefficients.b2=coefficients.b0;
  coefficients.a1=-2*cosine*norm;
  coefficients.a2=(1-alpha)*norm;
  const float cutoff=700.f*std::pow(22.f,smoothTone);
  alphaTone=1.f-std::exp(-2.f*static_cast<float>(pi)*cutoff/static_cast<float>(sr));
  gateAttack=1.f-std::exp(-1.f/static_cast<float>(sr*.005));
  gateRelease=1.f-std::exp(-1.f/static_cast<float>(sr*.065));
  dcCoef=std::exp(-2.f*static_cast<float>(pi)*12.f/static_cast<float>(sr));
 }
 Parameters target{};
 std::array<Channel,maxChannels> channel{};
 double sr=192000;
 float smoothDrive=0,smoothFuzz=0,smoothMix=1,smoothTone=.65f,smoothXover=155,smoothOutput=1,smoothPreserve=1;
 HPCoeffs coefficients{};
 float step=.001f,alphaTone=.2f,gateAttack=.01f,gateRelease=.001f,dcCoef=.999f;
 int count=2;
 std::uint64_t frames=0;
};
}
