#include "../Source/LaneActions.h"
#include <cmath>
#include <cstdio>
#include <cstdlib>
using namespace inverso_drum;
static void check(bool x,const char* msg){
 if(!x){std::fprintf(stderr,"LANE REGRESSION FAILED: %s\n",msg);std::exit(1);}
}
static double renderEnergy(Engine& e,int pad){
 e.prepare(48000);
 e.trigger(pad,1.f);
 double energy=0;
 for(int i=0;i<2048;++i){
  float l=0,r=0,revL=0,revR=0,delL=0,delR=0;
  e.renderFrame(l,r,revL,revR,delL,delR);
  check(std::abs(revL)+std::abs(revR)+std::abs(delL)+std::abs(delR)<1e-12f,
        "effects were routed without any send or override");
  energy+=l*l+r*r;
 }
 return energy;
}
int main(){
 Engine e;
 // A new kit is DRY. Nothing should auto-enable reverb/delay/drive per pad.
 for(int lane=0;lane<pads;++lane){
  check(e.drive[lane].load()==0.f,"pad drive is active by default");
  check(e.sendVerb[lane].load()==0.f,"pad reverb is active by default");
  check(e.sendDelay[lane].load()==0.f,"pad delay is active by default");
  check(e.padVelocity[lane].load()==1.f,"pad velocity is not neutral by default");
  for(int fx=0;fx<3;++fx)
   check(e.getEventFx(0,lane,0,fx)==Engine::inheritFx,
         "a new event had a pre-applied FX override");
 }
 const double standard=renderEnergy(e,0);
 e.padVelocity[0].store(.4f);
 const double quiet=renderEnergy(e,0);
 check(quiet>0&&standard>quiet*5.5&&standard<quiet*7.,
       "global pad velocity is not scaling amplitude or preserving note velocity");
 check(e.padVelocity[1].load()==1.f,"pad velocity change leaked into another instrument");
 const int pat=2,pad=1;
 e.set(pat,pad,4,127);e.set(pat,pad,12,61);e.set(pat,pad,20,33);
 e.setEventFx(pat,pad,4,1,75);
 e.setEventFx(pat,pad,12,2,88);
 StepSelection selection;
 check(selection.selectAllLoaded(e,pat,pad)==3,"select-all must pick only loaded hits");
 check(selection.count(pad)==3,"shift-select count wrong");
 check(!selection.has(pad,5)&&!selection.toggleLoaded(e,pat,pad,5),
       "shift-click selected an empty step");
 check(selection.toggleLoaded(e,pat,pad,12)&&selection.count(pad)==2,
       "shift-click could not remove an event from selection");
 check(selection.toggleLoaded(e,pat,pad,12)&&selection.count(pad)==3,
       "shift-click could not re-add event to selection");
 check(selection.addLoaded(e,pat,pad,12),"shift-drag cannot select a hit");
 int total=0;
 selection.forEach(pad,[&](int step){
  ++total;
  setStepVelocityAt(e,pat,pad,step,90);
 });
 check(total==3&&e.get(pat,pad,4)==90&&e.get(pat,pad,12)==90&&
       e.get(pat,pad,20)==90,"batch velocity didn't update exactly selected events");
 check(e.get(0,pad,4)==110,"multi-select edit changed another pattern");
 LaneUndoHistory history;
 history.capture(e,pat,pad,"C:/SAMPLES/Snares/custom.wav");
 e.padVelocity[pad].store(1.8f);
 eraseAllLaneNotes(e,pat,pad);
 check(e.get(pat,pad,4)==0&&e.get(pat,pad,12)==0&&e.get(pat,pad,20)==0,
       "erase all didn't remove all notes");
 check(e.getEventFx(pat,pad,4,1)==Engine::inheritFx&&
       e.getEventFx(pat,pad,12,2)==Engine::inheritFx,
       "erase all left orphaned event effects");
 check(e.get(0,pad,4)==110,"erase all changed another pattern");
 std::string previousPath;
 check(history.undo(e,pad,&previousPath),"undo did not restore lane");
 check(previousPath=="C:/SAMPLES/Snares/custom.wav","undo forgot sample path");
 check(e.get(pat,pad,4)==90&&e.get(pat,pad,12)==90&&
       e.get(pat,pad,20)==90,"undo did not restore individual step velocities");
 check(e.getEventFx(pat,pad,4,1)==75&&e.getEventFx(pat,pad,12,2)==88,
       "undo did not restore effect values for individual events");
 check(e.padVelocity[pad].load()==1.f,"undo didn't restore global pad velocity");
 check(!history.canUndo(pad),"undo incorrectly reports phantom history");
 std::puts("Shift selection, select-all, erase-all, undo, pad velocity and dry defaults passed.");
}
