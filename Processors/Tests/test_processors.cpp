#include "../Source/DSP/AdaptiveSuppressor.h"
#include <array>
#include <stdexcept>
#include <cmath>
#include <cstdio>
#include <vector>
using namespace inverso_proc;
static void require(bool ok,const char* why){if(!ok)throw std::runtime_error(why);}
constexpr int N=512;
static void test(Kind kind,double sr){
 AdaptiveSuppressor engine(kind);
 engine.prepare(sr);
 Parameters params; params.amount=0.f;
 engine.setParameters(params);
 float in[2][N],out[2][N];
 for(int k=0;k<100;++k){
  for(int i=0;i<N;++i){
   in[0][i]=0.18f*std::sin(2*pi*180*(i+k*N)/sr)+0.18f*std::sin(2*pi*2500*(i+k*N)/sr);
   in[1][i]=0.22f*std::sin(2*pi*280*(i+k*N)/sr);
  }
  auto m=engine.process(in[0],in[1],out[0],out[1],N);
  for(int i=0;i<N;++i){require(std::isfinite(out[0][i]),"non-finite left");require(std::isfinite(out[1][i]),"non-finite right");require(std::fabs(out[0][i]-in[0][i])<0.000005f,"amount zero not dry");}
  require(m.outputPeak>=0.f,"invalid meter");
 }
 params.amount=1;params.sensitivity=1;engine.setParameters(params);
 float totalRemoved=0;float greatestReduction=0;
 for(int k=0;k<200;++k){
  for(int i=0;i<N;++i){
   const float f=kind==Kind::Mudder?275.f:3150.f;
   in[0][i]=0.5f*std::sin(2*pi*f*(i+k*N)/sr);
   in[1][i]=in[0][i];
  }
  const auto meter=engine.process(in[0],in[1],out[0],out[1],N);
  greatestReduction=std::max(greatestReduction,meter.reductionDb);
  for(int i=0;i<N;++i){
   require(std::isfinite(out[0][i]) && std::isfinite(out[1][i]),"non-finite DSP output");
   require(std::abs(out[0][i])<1.5f && std::abs(out[1][i])<1.5f,"unbounded output");
   totalRemoved+=std::fabs(out[0][i]-in[0][i]);
  }
 }
 require(totalRemoved>10.f,"no band reduction");require(greatestReduction>0.1f,"no band meter");
 params.bypass=true;engine.setParameters(params);
 for(int k=0;k<200;++k)engine.process(in[0],in[1],out[0],out[1],N);
 double maxErr=0;
 for(int i=0;i<N;++i)maxErr=std::max(maxErr,std::fabs((double)out[0][i]-in[0][i]));
 require(maxErr<0.002,"bypass is not transparent");
 printf("%s at %.0f Hz: removed=%.1f reduction=%.2f dB bypassError=%.7f\n",kind==Kind::Mudder?"mud":"annoy",sr,totalRemoved,greatestReduction,maxErr);
}
int main(){for(double sr:{44100.,48000.,96000.}){test(Kind::Mudder,sr);test(Kind::Annoyance,sr);}return 0;}
