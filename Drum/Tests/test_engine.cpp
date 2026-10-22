#include "../Source/DrumEngine.h"
#include <cmath>
#include <cstdio>
#include <cstdlib>
using namespace inverso_drum;
static void ck(bool yes,const char* why){if(!yes){std::fprintf(stderr,"FAILED: %s\n",why);std::exit(1);}}
int main(){
 for(int sr:{44100,48000,96000}){
  Engine e;e.prepare(sr);
  for(int pad=0;pad<16;++pad){auto s=e.getSample(pad);ck(s&&s->left.size()>4,"factory sounds");}
  e.trigger(0,1);double energy=0;
  for(int i=0;i<sr/4;++i){float l=0,r=0;e.renderFrame(l,r);ck(std::isfinite(l)&&std::isfinite(r),"finite audio");energy+=l*l+r*r;}
  ck(energy>5,"factory kick audible");
  for(int p=0;p<8;++p)for(int k=0;k<16;++k)for(int j=0;j<16;++j)e.set(p,k,j,(p+k+j)%128);
  ck(e.get(7,15,15)==(7+15+15)%128,"pattern persistence");
  e.set(0,0,0,127);e.selectedPattern.store(0);e.transportSample(0,true);
  ck(e.currentStep.load()==0,"host origin");
  e.transportSample(.25,true);ck(e.currentStep.load()==1,"step transition");
  e.swing.store(1);e.stopTransport();e.transportSample(0,true);
  e.transportSample(.26,true);ck(e.currentStep.load()==0,"swing holds offbeat");
  e.transportSample(.37,true);ck(e.currentStep.load()==1,"swing releases offbeat");
  e.chain.store(true);e.transportSample(4.0,true);ck(e.currentPattern.load()==1,"pattern chaining");
  e.stopTransport();ck(e.currentStep.load()==-1,"transport stop");
 }
 puts("INVERSO DRUM engine tests passed");
}
