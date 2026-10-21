#include "../Source/DSP/BassDriveCore.h"
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <cassert>
#include <cmath>
#include <vector>
#include <cstdio>
int main(){
 using namespace inverso_bass;
 for(int n: {16,32,64}){
  std::vector<Core> processors(static_cast<size_t>(n));
  for(auto& c:processors){ c.prepare(192000,2); }
  for(int frame=0;frame<30000;++frame){
   Parameters p;
   p.drive=static_cast<float>((frame/97)%100)/99.f;
   p.fuzz=static_cast<float>((frame/61)%100)/99.f;
   p.mode=static_cast<FuzzMode>((frame/1500)%4);
   p.crossover=60+static_cast<float>((frame/41)%440);
   p.tone=static_cast<float>((frame/83)%100)/99.f;
   p.mix=static_cast<float>((frame/137)%100)/99.f;
   p.outputDb=static_cast<float>((frame/101)%36)-24.f;
   p.preserveLow=(frame/700)%2==0;
   const float x=.8f*std::sin(.013f*frame);
   for(auto& c:processors){
    if(frame%32==0)c.setParameters(p);
    const float a=c.processChannel(0,x);
    const float b=c.processChannel(1,x*.8f);
    c.tickFrame();
    assert(std::isfinite(a)&&std::isfinite(b));
    assert(std::abs(a)<100.f&&std::abs(b)<100.f);
   }
  }
  std::printf("%d simulated instances passed\n",n);
 }
}
