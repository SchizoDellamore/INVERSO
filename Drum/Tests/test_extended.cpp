#include "../Source/DrumEngine.h"
#include "../Source/DrumFX.h"
#include <cmath>
#include <cstdio>
#include <cstdlib>
using namespace inverso_drum;
static void ck(bool x,const char* m){if(!x){std::fprintf(stderr,"FAIL: %s\n",m);std::exit(1);}}
int main(){
 for(int sr:{44100,48000,96000}){
  Engine e;e.prepare(sr);
  ck(steps==128&&pages==8,"128 step memory, eight pages");
  for(int pat=0;pat<8;++pat)for(int lane=0;lane<16;++lane){
   e.setLaneLength(lane,(lane+1)*8);
   e.setDivision(lane,lane%4,lane%3);
   e.set(pat,lane,127,127);
   ck(e.get(pat,lane,127)==127,"last step of every pattern/lane");
  }
  ck(divisionPPQ(2,0)==.25,"straight 1/16");
  ck(divisionPPQ(2,1)==.375,"dotted 1/16");
  ck(std::abs(divisionPPQ(2,2)-1.0/6.0)<1e-8,"triplet 1/16");
  e.setLaneLength(0,128);e.setDivision(0,2,0);
  e.setLaneLength(1,16);e.setDivision(1,2,0);
  ck(e.patternSpanPPQ()>=32.,"128-step lane length extends pattern chain span");
  e.chain.store(false);e.selectedPattern.store(0);
  e.set(0,0,127,120);
  e.stopTransport();e.transportSample(31.75,true);
  ck(e.currentLaneStep[0].load()==127,"128th step resolves at PPQ 31.75");
  e.transportSample(32.,true);
  ck(e.currentLaneStep[0].load()==0,"128 step lane wraps");
  e.setLaneLength(0,16);e.setDivision(0,2,2);
  e.stopTransport();e.transportSample(0.,true);
  e.transportSample(1.0/6.0,true);
  ck(e.currentLaneStep[0].load()==1,"triplet lane advances on triplet clock");
  e.setDivision(0,2,1);e.stopTransport();e.transportSample(0.,true);
  e.transportSample(.375,true);
  ck(e.currentLaneStep[0].load()==1,"dotted lane advances on dotted clock");
  e.sampleStart[0].store(.1f);e.sampleEnd[0].store(.8f);
  e.samplePitch[0].store(-12.f);e.sampleReverse[0].store(true);
  e.sampleAttack[0].store(.25f);e.sampleDecay[0].store(.7f);
  e.drive[0].store(.75f);e.sendDelay[0].store(.7f);e.sendVerb[0].store(.5f);
  e.trigger(0,1.f);
  Effects fx;fx.prepare(sr);fx.delayDivision.store(2);fx.delayFeel.store(1);fx.setTempo(120);
  double energy=0,sendEnergy=0;
  for(int i=0;i<sr/3;++i){
   float l=0,r=0,vl=0,vr=0,dl=0,dr=0;
   e.renderFrame(l,r,vl,vr,dl,dr);
   fx.process(vl,vr,dl,dr,l,r);
   ck(std::isfinite(l)&&std::isfinite(r),"FX produces finite audio");
   energy+=l*l+r*r;sendEnergy+=vl*vl+vr*vr+dl*dl+dr*dr;
  }
  ck(energy>.01,"reversed, edited sample produces audio");
  ck(sendEnergy>.01,"per-pad sends feed effects");
 }
 std::puts("INVERSO DRUM extended sequencer/sample/fx tests passed.");
}
