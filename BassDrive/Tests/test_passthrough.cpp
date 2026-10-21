#include "../Source/DSP/BassDriveCore.h"
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <algorithm>
using namespace inverso_bass;
static void check(bool b, const char* why){if(!b){std::fprintf(stderr,"FAIL: %s\n",why);std::exit(1);}}
int main(){
 for(int sr : {44100,48000,96000}){
  for(int channels : {1,2}){
   Core core;core.prepare(sr,channels);
   Parameters p;p.drive=0;p.fuzz=0;p.mix=1;p.outputDb=0;
   core.setParameters(p);
   for(int i=0;i<5000;++i){
    for(int c=0;c<channels;++c){
     const float x=.6f*std::sin(2*pi*199*i/sr+c);
     const float y=core.processChannel(c,x);
     check(std::isfinite(y),"clean finite");
     check(std::abs(x-y)<1e-6f,"clean pass-through");
    }
    core.tickFrame();
   }
   for(int m=0;m<4;++m){
    p.drive=.8f;p.fuzz=.7f;p.mode=static_cast<FuzzMode>(m);p.mix=1;
    core.setParameters(p);
    double energy=0;
    for(int i=0;i<10000;++i){
     for(int c=0;c<channels;++c){
      const float x=.35f*std::sin(2*pi*499*i/sr+c);
      const float y=core.processChannel(c,x);
      check(std::isfinite(y),"processed finite");
      energy+=y*y;
     }
     core.tickFrame();
    }
    check(energy>100.,"processed audio is silent");
   }
  }
 }
 std::puts("BassDrive passthrough/zero-silence tests passed");
}
