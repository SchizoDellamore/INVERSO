#include "../Source/DSP/BassDriveCore.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <vector>
using namespace inverso_bass;
static void check(bool ok,const char* msg){if(!ok){std::fprintf(stderr,"FAIL: %s\n",msg);std::exit(1);}}
static std::vector<float> render(Parameters p,float hz,float amplitude,int sampleRate=48000,int len=48000){
 Core d; d.prepare(sampleRate,1);d.setParameters(p);
 std::vector<float> out(len);
 for(int i=0;i<len;++i){out[i]=d.processChannel(0,amplitude*std::sin(2*pi*hz*i/sampleRate));d.tickFrame();check(std::isfinite(out[i]),"finite");}
 return out;
}
static double magnitude(const std::vector<float>& y,double hz,int sr=48000){
 double cs=0,sn=0;const auto n=y.size();
 for(size_t i=n/2;i<n;++i){cs+=y[i]*std::cos(2*pi*hz*i/sr);sn+=y[i]*std::sin(2*pi*hz*i/sr);}
 return std::hypot(cs,sn)/(n/4.0);
}
int main(){
 Parameters p;p.drive=0;p.fuzz=0;p.mode=FuzzMode::Off;p.mix=1;p.outputDb=0;
 auto clean=render(p,120,.12f);
 check(std::abs(magnitude(clean,120)-.12)<.003,"default zero drive/fuzz is transparent");
 // A disabled fuzz mode must remain transparent, even with Fuzz Amount set.
 p.fuzz=1.f;p.mode=FuzzMode::Off;
 auto fuzzOff=render(p,120,.12f);
 check(std::abs(magnitude(fuzzOff,120)-.12)<.003,"fuzz Off must not change audio with drive zero");
 p.fuzz=0.f;
 p.drive=.45f;
 auto driven=render(p,120,.12f);
 auto drivenHi=render(p,550,.12f);
 double h2=magnitude(drivenHi,1100),h3=magnitude(drivenHi,1650);
 std::printf("DRIVE @550Hz harmonic 2: %.5f, 3: %.5f\n",h2,h3);
 check(h2+h3>.009,"drive must generate audible harmonics at modest input levels");
 p.body=0;auto bodyOff=render(p,120,.12f);
 p.body=1;auto bodyOn=render(p,120,.12f);
 const double bodyDifference=magnitude(bodyOn,120)-magnitude(bodyOff,120);
 std::printf("BODY 120 Hz fundamental delta %.5f\n",bodyDifference);
 check(bodyDifference>.005,"body control should add tangible low-mid weight");
 p.drive=0;p.body=.65f;p.fuzz=.75f;
 double harmonics[3]{};
 for(int mode=1;mode<=3;++mode){
  p.mode=static_cast<FuzzMode>(mode);
  const auto out=render(p,550,.12f);
  harmonics[mode-1]=magnitude(out,1650)+magnitude(out,2750);
  std::printf("FUZZ %d 3rd+5th %.5f\n",mode,harmonics[mode-1]);
  check(harmonics[mode-1]>.02,"fuzz needs clear high harmonics without drive");
 }
 check(std::abs(harmonics[0]-harmonics[1])>.003,"vintage/silicon fuzz should be sonically distinct");
 std::puts("Bass Drive character tests passed");
}
