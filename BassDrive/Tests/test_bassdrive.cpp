#include "../Source/DSP/BassDriveCore.h"
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <cassert>
#include <cmath>
#include <cstdio>
using namespace inverso_bass;
static float runSine(Core& d,float hz,float amplitude,int frames,int channel=0){
 float squared=0;
 for(int i=0;i<frames;++i){
  const auto x=amplitude*std::sin(2*pi*hz*i/192000.f);
  const float y=d.processChannel(channel,x);
  d.tickFrame();
  assert(std::isfinite(y));
  if(i>frames/2)squared+=(y-x)*(y-x);
 }
 return std::sqrt(squared/(frames/2.f));
}
int main(){
 Core c;
 c.prepare(192000,1);
 Parameters p;
 p.drive=0;p.fuzz=0;p.mix=1;p.outputDb=0;
 c.setParameters(p);
 { float v=runSine(c,1200,.7f,16000); std::printf("clean delta %.7g\n",v); assert(v<1e-6f); }
 c.prepare(192000,1);
 p.drive=1;p.fuzz=0;p.mode=FuzzMode::Off;p.mix=0;
 c.setParameters(p);
 assert(runSine(c,1200,.7f,100000)<1e-6f);
 c.prepare(192000,1);
 p.mix=1;p.drive=.9f;p.mode=FuzzMode::Off;
 c.setParameters(p);
 const float driven=runSine(c,1200,.7f,50000);
 assert(driven>.025f);
 // Low-preservation processing should alter 55Hz less than 1.2kHz.
 c.prepare(192000,1);p.preserveLow=true;c.setParameters(p);
 const float bass=runSine(c,55,.7f,65000);
 c.prepare(192000,1);c.setParameters(p);
 const float mids=runSine(c,1200,.7f,65000);
 std::printf("bass %.6f mids %.6f ratio %.3f\n",bass,mids,bass/mids); assert(bass<mids*.65f);
 for(int m=1;m<=3;++m){
  c.prepare(192000,1);p.mode=static_cast<FuzzMode>(m);p.fuzz=.85f;p.drive=.2f;c.setParameters(p);
  const auto change=runSine(c,1200,.7f,65000);
  std::printf("fuzz mode %d highband difference rms %.6f\n",m,change);
  assert(change>.03f);
 }
 std::puts("Bass Drive functional test passed");
}
