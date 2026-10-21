#include "../Source/DSP/AdaptiveSuppressor.h"
#include <array>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <stdexcept>
int main(){
 constexpr int N=512;
 std::array<inverso_proc::AdaptiveSuppressor,64> units=[](){
  std::array<inverso_proc::AdaptiveSuppressor,64> a={
#define M inverso_proc::AdaptiveSuppressor(inverso_proc::Kind::Mudder),
#define A inverso_proc::AdaptiveSuppressor(inverso_proc::Kind::Annoyance),
   M M M M M M M M M M M M M M M M M M M M M M M M M M M M M M M M
   A A A A A A A A A A A A A A A A A A A A A A A A A A A A A A A A
#undef M
#undef A
  }; return a; }();
 for(auto& p:units){p.prepare(48000);inverso_proc::Parameters param;param.amount=.8f;param.sensitivity=.7f;p.setParameters(param);}
 float l[N],r[N],ol[N],orr[N];
 for(int i=0;i<N;++i){l[i]=0.3f*std::sin(2*inverso_proc::pi*440*i/48000);r[i]=0.3f*std::sin(2*inverso_proc::pi*2400*i/48000);}
 auto t0=std::chrono::steady_clock::now();
 for(int repeat=0;repeat<96;++repeat)for(auto& p:units){const auto m=p.process(l,r,ol,orr,N);if(!std::isfinite(m.reductionDb))throw std::runtime_error("non-finite attenuation");}
 auto t1=std::chrono::steady_clock::now();
 printf("64 instances 96 blocks of 512 stereo, secs=%.3f\n",std::chrono::duration<double>(t1-t0).count());
}
