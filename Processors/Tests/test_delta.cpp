// Regression: LISTEN REMOVED must be the mathematical difference (dry - wet)
// even if the internal BYPASS parameter is enabled.
#include "../Source/DSP/AdaptiveSuppressor.h"
#include <array>
#include <cmath>
#include <cstdio>
#include <stdexcept>
using namespace inverso_proc;
constexpr int N=512;
static void need(bool cond,const char* msg){if(!cond)throw std::runtime_error(msg);}

static void verifyDifference(Kind kind,bool bypass,bool amountZero){
 AdaptiveSuppressor normal(kind), listen(kind);
 normal.prepare(48000);listen.prepare(48000);
 Parameters p;
 p.amount=amountZero?0.f:0.9f;
 p.sensitivity=0.85f;
 p.focus=0.5f;
 p.bypass=false;p.delta=false;
 normal.setParameters(p);
 p.bypass=bypass;p.delta=true;
 listen.setParameters(p);
 std::array<float,N> l{},r{},wetL{},wetR{},delL{},delR{};
 double maxErr=0,deltaEnergy=0,signalEnergy=0;
 for(int k=0;k<380;k++){
  for(int i=0;i<N;i++){
   const double t=static_cast<double>(k*N+i)/48000.;
   l[i]=0.15f*std::sin(2*pi*220*t)+0.22f*std::sin(2*pi*3150*t)+0.12f*std::sin(2*pi*6100*t);
   r[i]=0.18f*std::sin(2*pi*170*t)+0.24f*std::sin(2*pi*4400*t);
  }
  normal.process(l.data(),r.data(),wetL.data(),wetR.data(),N);
  listen.process(l.data(),r.data(),delL.data(),delR.data(),N);
  if(k>=300)for(int i=0;i<N;i++){
   maxErr=std::max(maxErr,std::abs(static_cast<double>(delL[i])-(l[i]-wetL[i])));
   maxErr=std::max(maxErr,std::abs(static_cast<double>(delR[i])-(r[i]-wetR[i])));
   deltaEnergy+=delL[i]*delL[i]+delR[i]*delR[i];
   signalEnergy+=l[i]*l[i]+r[i]*r[i];
  }
 }
 const double ratio=std::sqrt(deltaEnergy/signalEnergy);
 std::printf("%s bypass=%d amountZero=%d delta/input=%.4f nullError=%.6f\n",
    kind==Kind::Mudder?"Mudder":"Annoyance",bypass,amountZero,ratio,maxErr);
 need(maxErr<0.0002,"LISTEN REMOVED differs from dry - wet");
 if(amountZero)need(ratio<0.0001,"LISTEN REMOVED must be silent when Amount is zero");
 else need(ratio>0.02 && ratio<0.85,"LISTEN REMOVED is too small or leaking full dry audio");
}
int main(){
 for(const auto k:{Kind::Mudder,Kind::Annoyance}){
  verifyDifference(k,false,false);
  verifyDifference(k,true,false);
  verifyDifference(k,false,true);
  verifyDifference(k,true,true);
 }
 return 0;
}
