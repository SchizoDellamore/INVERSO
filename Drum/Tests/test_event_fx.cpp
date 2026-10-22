#include "../Source/DrumEngine.h"
#include <cmath>
#include <cstdio>
#include <cstdlib>
using namespace inverso_drum;
static void ck(bool x,const char* why){
 if(!x){std::fprintf(stderr,"EVENT FX FAILED: %s\n",why);std::exit(1);}
}
struct AudioStats{double dry=0,rev=0,del=0;};
static AudioStats play(Engine& e,int pat,int lane,int step,int rate){
 e.prepare(rate);
 e.trigger(lane,1.f,pat,step);
 AudioStats result;
 for(int n=0;n<6000;++n){
  float l=0,r=0,vl=0,vr=0,dl=0,dr=0;
  e.renderFrame(l,r,vl,vr,dl,dr);
  ck(std::isfinite(l)&&std::isfinite(r)&&std::isfinite(vl)&&std::isfinite(dl),
     "nonfinite sample or effects send");
  result.dry+=l*l+r*r;
  result.rev+=vl*vl+vr*vr;
  result.del+=dl*dl+dr*dr;
 }
 return result;
}
int main(){
 for(const int sr:{44100,48000,96000}){
  Engine e;e.prepare(sr);
  e.gain[1].store(.8f);
  e.drive[1].store(0.f);
  e.sendVerb[1].store(0.f);
  e.sendDelay[1].store(0.f);
  ck(e.getEventFx(0,1,3,1)==Engine::inheritFx,"initial FX must inherit from pad");
  e.setEventFx(0,1,3,1,100); // snare on 4: Reverb only
  e.setEventFx(0,1,3,2,0);
  e.setEventFx(0,1,11,1,0);
  e.setEventFx(0,1,11,2,88); // same snare on 12: Delay only
  e.setEventFx(0,1,11,0,65);
  ck(e.getEventFx(0,1,3,1)==100 && e.getEventFx(0,1,11,1)==0,
    "two events on same snare must have independent reverb");
  ck(e.getEventFx(0,1,3,2)==0 && e.getEventFx(0,1,11,2)==88,
    "two events on same snare must have independent delay");
  ck(e.getEventFx(0,1,4,1)==Engine::inheritFx,
    "unselected adjacent event should inherit pad defaults");
  ck(e.getEventFx(1,1,3,1)==Engine::inheritFx,
    "another pattern must stay independent");
  ck(e.getEventFx(0,2,3,1)==Engine::inheritFx,
    "another instrument must stay independent");
  const auto reverbHit=play(e,0,1,3,sr);
  const auto delayHit=play(e,0,1,11,sr);
  const auto dryHit=play(e,0,1,4,sr);
  ck(reverbHit.dry>1.&&reverbHit.rev>1.&&reverbHit.del<1e-10,
     "snare step4 must route to reverb only");
  ck(delayHit.dry>1.&&delayHit.del>1.&&delayHit.rev<1e-10,
     "snare step12 must route to delay only");
  ck(dryHit.dry>1.&&dryHit.del<1e-10&&dryHit.rev<1e-10,
     "snare step5 must remain entirely dry");
  ck(std::abs(delayHit.dry-dryHit.dry)>.01,
     "step-specific drive must change the audio, not only an FX label");
  // Verify immutable voice snapshots: changing the pattern after a hit starts
  // must not reassign effects to that already-playing snare.
  e.prepare(sr);e.trigger(1,1.f,0,3);
  e.setEventFx(0,1,3,1,0);
  float l=0,r=0,vl=0,vr=0,dl=0,dr=0;
  e.renderFrame(l,r,vl,vr,dl,dr);
  ck(std::abs(vl)+std::abs(vr)>.000001f,
    "editing later events must not alter active voice send");
 }
 std::puts("Per-event independent Drive/Reverb/Delay test passed.");
}
