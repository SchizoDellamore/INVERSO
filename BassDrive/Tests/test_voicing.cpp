#include "../Source/DSP/BassDriveCore.h"
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <vector>
using namespace inverso_bass;
static void verify(bool b,const char* why){if(!b){std::fprintf(stderr,"B3K-INSPIRED TEST FAILED: %s\n",why);std::exit(1);}}
static std::vector<float> render(Parameters p,float hz,float amp=0.15f,int rate=48000){
 Core c;c.prepare(rate,1);c.setParameters(p);
 std::vector<float> out(24000);
 for(size_t i=0;i<out.size();++i) {
  const float x=amp*std::sin(2*pi*hz*i/rate);
  out[i]=c.processChannel(0,x);c.tickFrame();
  verify(std::isfinite(out[i]) && std::abs(out[i])<6.f,"finite/bounded output");
 }
 return out;
}
static double diff(const std::vector<float>& a,const std::vector<float>& b){
 double sum=0;for(size_t i=a.size()/2;i<a.size();++i){const double d=a[i]-b[i];sum+=d*d;}
 return std::sqrt(sum/(a.size()/2.0));
}
int main(){
 Parameters p;p.drive=0;p.fuzz=0;p.mix=1;p.body=0;p.midBoost=false;
 const auto dry=render(p,220.f);
 p.grunt=0;p.midBoost=true;p.driveTone=0;p.fuzz=1;p.mode=FuzzMode::Off;
 verify(diff(dry,render(p,220.f))<1e-6,"drive and fuzz off must stay transparent");
 p.drive=.62f;p.fuzz=0;p.midBoost=false;p.driveTone=.65f;p.grunt=0;
 const auto tight=render(p,110.f);
 p.grunt=2;const auto thick=render(p,110.f);
 const double gruntDiff=diff(tight,thick);
 std::printf("Grunt Tight vs Thick RMS difference: %.6f\n",gruntDiff);
 verify(gruntDiff>.001,"Grunt must affect saturation's low-frequency content");
 p.grunt=1;p.midBoost=false;
 const auto midOff=render(p,600.f);
 p.midBoost=true;const auto midOn=render(p,600.f);
 const double midDiff=diff(midOff,midOn);
 std::printf("Mid boost Off vs On RMS difference: %.6f\n",midDiff);
 verify(midDiff>.001,"Mid boost must affect distortion");
 p.midBoost=false;p.driveTone=0;
 const auto dark=render(p,4200.f);
 p.driveTone=1;const auto bright=render(p,4200.f);
 const double toneDiff=diff(dark,bright);
 std::printf("Drive Tone low vs high RMS difference: %.6f\n",toneDiff);
 verify(toneDiff>.0005,"Drive tone must affect high-frequency saturation");
 p.drive=0;p.fuzz=.75f;p.mode=FuzzMode::Silicon;
 verify(diff(dry,render(p,220.f))>.005,"fuzz must work without drive");
 std::puts("B3K-inspired signal-path tests passed.");
}
