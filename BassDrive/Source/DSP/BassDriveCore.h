#pragma once
// INVERSO BASS DRIVE 0.4: B3K-inspired voicing, NOT a circuit emulation.
// Fixed-state, no dynamic allocations in audio callback, zero host latency.
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>

namespace inverso_bass {
constexpr double pi=3.1415926535897932384626433832795;
constexpr int maxChannels=2;
enum class FuzzMode:int {Off=0,Vintage=1,Silicon=2,Gated=3};
inline float clamp(float x,float lo,float hi) noexcept { return std::max(lo,std::min(x,hi)); }
inline float dbGain(float x) noexcept {return std::pow(10.f,x/20.f);}
struct Parameters {
 float drive=.25f,fuzz=0.f,crossover=155.f,tone=.65f,mix=1.f,outputDb=0.f;
 float body=.65f,driveTone=.65f;
 int grunt=1; // 0 Tight, 1 Balanced, 2 Thick; controls bass sent INTO distortion
 bool midBoost=false,preserveLow=true;
 FuzzMode mode=FuzzMode::Off;
};
class Core {
public:
 void prepare(double rate,int channels) noexcept {
  sr=std::isfinite(rate)?std::clamp(rate,8000.0,384000.0):48000.0;
  count=std::clamp(channels,1,maxChannels);
  for(auto& s:state)s={};
  smDrive=0;smFuzz=0;smMix=1;smTone=.65f;smDriveTone=.65f;
  smXover=155;smBody=.65f;smGrunt=.35f;smMid=0;
  smOutput=1;smPreserve=1;frames=0;
  step=1.f-std::exp(-1.f/static_cast<float>(sr*.012));
  updateCoefficients();
 }
 void setParameters(Parameters p) noexcept {
  p.drive=clamp(p.drive,0,1);p.fuzz=clamp(p.fuzz,0,1);
  p.crossover=clamp(p.crossover,60,500);p.tone=clamp(p.tone,0,1);
  p.mix=clamp(p.mix,0,1);p.outputDb=clamp(p.outputDb,-24,12);
  p.body=clamp(p.body,0,1);p.driveTone=clamp(p.driveTone,0,1);
  p.grunt=std::clamp(p.grunt,0,2);
  p.mode=static_cast<FuzzMode>(std::clamp(static_cast<int>(p.mode),0,3));
  target=p;
 }
 float processChannel(int ch,float x) noexcept {
  ch=std::clamp(ch,0,count-1);
  auto& s=state[static_cast<size_t>(ch)];
  if(!std::isfinite(x)) return 0.f;
  // Complementary parallel branch: the fundamental remains in the clean path.
  const float high=s.hp.tick(x,hpCoeffs)*smPreserve + x*(1.f-smPreserve);
  const float low=x-high;
  s.bodyLo+=bodyLoA*(x-s.bodyLo);
  s.bodyHi+=bodyHiA*(x-s.bodyHi);
  const float bodyBand=s.bodyHi-s.bodyLo;
  s.midLo+=midLoA*(x-s.midLo);
  s.midHi+=midHiA*(x-s.midHi);
  const float midBand=s.midHi-s.midLo;
  const float feed=high+smGrunt*low;
  // Moderate 300-1000 Hz pre-emphasis adds bite without permanently EQing clean signal.
  const float excited=feed+smMid*.90f*midBand;
  float driveDelta=0.f;
  if(smDrive>0.00001f) {
   const float driveGain=1.f+3.7f*smDrive+16.f*smDrive*smDrive;
   const float bias=.11f*smDrive;
   const float u=excited*driveGain;
   const float asym=.78f*(std::tanh(u+bias)-std::tanh(bias));
   const float edgy=.63f*std::tanh(u*1.7f);
   const float saturator=.62f*asym+.38f*edgy;
   const float difference=saturator-excited;
   s.driveColor+=driveToneA*(difference-s.driveColor);
   // Parallel saturation retains pick/finger transients and bass articulation.
   driveDelta=(.64f+.24f*smDrive)*smDrive*s.driveColor;
  } else {
   s.driveColor=0.f;
  }
  float fuzzDelta=0.f;
  if(target.mode!=FuzzMode::Off && smFuzz>0.00002f) {
   const float fuzzGain=2.4f+31.f*smFuzz*smFuzz;
   float fuzzWave=0.f;
   switch(target.mode) {
    case FuzzMode::Vintage:
     fuzzWave=.95f*(std::tanh((feed+.042f)*fuzzGain)-std::tanh(.042f*fuzzGain));
     break;
    case FuzzMode::Silicon:
     fuzzWave=1.30f*clamp(feed*fuzzGain,-.63f,.63f);
     break;
    case FuzzMode::Gated: {
     const float a=std::abs(feed);
     const float envCoef=a>s.gateEnvelope?gateAttack:gateRelease;
     s.gateEnvelope+=envCoef*(a-s.gateEnvelope);
     const float threshold=.012f+.038f*smFuzz;
     const float gate=clamp((s.gateEnvelope-threshold)/(threshold*.65f+1.e-8f),0.f,1.f);
     fuzzWave=std::tanh(feed*fuzzGain*2.f)*gate;
     break;
    }
    default: break;
   }
   // DC block is applied to the nonlinear fuzz branch only.
   const float ac=fuzzWave-s.dcInput+dcCoefficient*s.dcOutput;
   s.dcInput=fuzzWave;s.dcOutput=ac;
   s.fuzzColor+=fuzzToneA*(ac-s.fuzzColor);
   fuzzDelta=std::min(1.f,smFuzz*1.25f)*(s.fuzzColor-feed);
  } else {
   s.dcInput=0;s.dcOutput=0;s.fuzzColor*=.998f;
   s.gateEnvelope*=1.f-gateRelease;
  }
  const float activity=std::max(smDrive,smFuzz*.5f*(target.mode!=FuzzMode::Off?1.f:0.f));
  const float bodyLift=smBody*activity*.90f*bodyBand;
  const float wet=x+driveDelta+fuzzDelta+bodyLift;
  const float y=(x+smMix*(wet-x))*smOutput;
  if(!std::isfinite(y)){s={};return x;}
  return y;
 }
 void tickFrame() noexcept {
  smDrive+=step*(target.drive-smDrive);
  smFuzz+=step*(target.fuzz-smFuzz);
  smMix+=step*(target.mix-smMix);
  smTone+=step*(target.tone-smTone);
  smDriveTone+=step*(target.driveTone-smDriveTone);
  smXover+=step*(target.crossover-smXover);
  smBody+=step*(target.body-smBody);
  const float gruntTarget=target.grunt==0?.08f:(target.grunt==1?.35f:.78f);
  smGrunt+=step*(gruntTarget-smGrunt);
  smMid+=step*((target.midBoost?1.f:0.f)-smMid);
  smOutput+=step*(dbGain(target.outputDb)-smOutput);
  smPreserve+=step*((target.preserveLow?1.f:0.f)-smPreserve);
  if((++frames&31U)==0U)updateCoefficients();
 }
 int numChannels() const noexcept{return count;}
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
 struct Channel {
  Biquad hp{};
  float bodyLo=0,bodyHi=0,midLo=0,midHi=0;
  float driveColor=0,fuzzColor=0,dcInput=0,dcOutput=0,gateEnvelope=0;
 };
 void updateCoefficients() noexcept {
  const double w=2.*pi*smXover/sr;
  const double c=std::cos(w),a=std::sin(w)/(2.*.7071067811865476),n=1./(1.+a);
  hpCoeffs.b0=.5*(1.+c)*n;hpCoeffs.b1=-(1.+c)*n;hpCoeffs.b2=hpCoeffs.b0;
  hpCoeffs.a1=-2.*c*n;hpCoeffs.a2=(1.-a)*n;
  const auto lp=[this](float hz){return 1.f-std::exp(-2.f*static_cast<float>(pi)*hz/static_cast<float>(sr));};
  bodyLoA=lp(65.f);bodyHiA=lp(310.f);
  midLoA=lp(300.f);midHiA=lp(1000.f);
  driveToneA=lp(3000.f+5000.f*smDriveTone);
  fuzzToneA=lp(800.f*std::pow(18.f,smTone));
  gateAttack=1.f-std::exp(-1.f/static_cast<float>(sr*.002));
  gateRelease=1.f-std::exp(-1.f/static_cast<float>(sr*.055));
  dcCoefficient=std::exp(-2.f*static_cast<float>(pi)*16.f/static_cast<float>(sr));
 }
 Parameters target{};
 std::array<Channel,maxChannels> state{};
 Coeffs hpCoeffs{};
 double sr=48000;
 float smDrive=0,smFuzz=0,smMix=1,smTone=.65f,smDriveTone=.65f;
 float smXover=155,smBody=.65f,smGrunt=.35f,smMid=0,smOutput=1,smPreserve=1;
 float bodyLoA=.01f,bodyHiA=.05f,midLoA=.02f,midHiA=.08f;
 float driveToneA=.4f,fuzzToneA=.2f,gateAttack=.01f,gateRelease=.001f,dcCoefficient=.999f,step=.001f;
 std::uint64_t frames=0;
 int count=2;
};
}
