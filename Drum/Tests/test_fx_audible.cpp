#include "../Source/DrumFX.h"
#include <cmath>
#include <cstdio>
#include <cstdlib>
using namespace inverso_drum;
static void check(bool yes,const char* msg){if(!yes){std::fprintf(stderr,"FX TEST FAILED: %s\n",msg);std::exit(1);}}
int main(){
 for(const int sr : {44100,48000,96000}){
  // Isolate FX output: no dry voice remains. Both effects must have a measurable tail.
  Effects r; r.prepare(sr);r.reverbWet.store(.8f);r.delayWet.store(0.f);
  double energyRev=0; int firstRev=-1;
  for(int i=0;i<sr;++i){
   float l=0,rch=0;
   r.process(i==0?1.f:0.f,i==0?1.f:0.f,0,0,l,rch);
   check(std::isfinite(l)&&std::isfinite(rch),"finite reverb");
   if(std::abs(l)+std::abs(rch)>1e-6f && firstRev<0)firstRev=i;
   if(i>sr/30) energyRev+=l*l+rch*rch;
  }
  std::printf("%dHz reverb: first=%d, tailEnergy=%.8f\n",sr,firstRev,energyRev);
  check(firstRev>0 && firstRev<sr/7 && energyRev>.01,"reverb must generate audible tail");
  Effects d; d.prepare(sr);d.reverbWet.store(0.f);d.delayWet.store(.75f);
  d.delayDivision.store(1);d.delayFeel.store(0);d.setTempo(120);
  const int expected=sr/4; // 1/8 note at 120 BPM (0.25sec)
  float detected=0;
  int firstDelay=-1;
  for(int i=0;i<sr;++i){
   float l=0,rch=0;
   d.process(0,0,i==0?1.f:0.f,i==0?1.f:0.f,l,rch);
   check(std::isfinite(l)&&std::isfinite(rch),"finite delay");
   if(std::abs(l)>1e-5f && firstDelay<0)firstDelay=i;
   if(i==expected)detected=l;
  }
  std::printf("%dHz delay: first=%d expected=%d echo=%.5f\n",sr,firstDelay,expected,detected);
  check(std::abs(firstDelay-expected)<=1 && detected>.5f,"delay must echo at musical interval");
  // Per-pad sends must actually feed effect routing on engine output.
  Engine e;e.prepare(sr);
  for(auto& v:e.sendVerb)v.store(0.f);
  for(auto& v:e.sendDelay)v.store(0.f);
  e.sendVerb[1].store(1.f);e.sendDelay[1].store(1.f);
  e.trigger(1,1.f);double sends=0;
  for(int i=0;i<sr/4;++i){float l=0,rch=0,vl=0,vr=0,dl=0,dr=0;
   e.renderFrame(l,rch,vl,vr,dl,dr);sends+=vl*vl+vr*vr+dl*dl+dr*dr;
  }
  check(sends>1.0,"pad sends must be nonzero and audible");
 }
 std::puts("INVERSO DRUM independent delay/reverb/send tests passed.");
}
