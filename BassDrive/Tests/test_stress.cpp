#include "../Source/DSP/BassDriveCore.h"
#include <cstdlib>
#include <cmath>
#include <vector>
#include <cstdio>
using namespace inverso_bass;
int main(){
 for(int sr:{44100,48000,96000})for(int n:{16,32,64}){
  std::vector<Core> cores(static_cast<size_t>(n));
  for(auto& c:cores)c.prepare(sr,2);
  for(int i=0;i<16000;++i){
   Parameters p;
   p.drive=((i/41)%101)/100.f;p.fuzz=((i/37)%101)/100.f;
   p.body=((i/59)%101)/100.f;p.crossover=60+((i/97)%441);
   p.mode=static_cast<FuzzMode>((i/1200)%4);p.tone=((i/19)%101)/100.f;
   p.mix=((i/23)%101)/100.f;p.outputDb=-12+((i/87)%24);
   p.preserveLow=(i/1100)%2==0;
   const float x=.4f*std::sin(2*pi*107*i/sr);
   for(auto& c:cores){if(i%64==0)c.setParameters(p);
    const auto a=c.processChannel(0,x),b=c.processChannel(1,x*.8f);
    c.tickFrame();
    if(!std::isfinite(a)||!std::isfinite(b))std::exit(2);
    if(std::abs(a)>=10||std::abs(b)>=10)std::exit(3);
   }
  }
  std::printf("%d Hz, %d instances: pass\n",sr,n);
 }
}
