#include "../Source/LaneActions.h"
#include <cmath>
#include <cstdio>
#include <cstdlib>
using namespace inverso_drum;
static void check(bool ok,const char* msg){
 if(!ok){std::fprintf(stderr,"TIMELINE REGRESSION FAILED: %s\n",msg);std::exit(1);}
}
int main(){
 Engine e;
 const int pattern=0, snare=1, kick=0;
 // Exact reported scenario: 4-bar overview, each 1/16 equals one of 64 cells.
 const double dur=divisionPPQ(2,0),width=960.;
 constexpr int bars=4;
 const int first=absoluteStepForPosition(0,bars,(4.5/64.)*width,width,dur);
 const int second=absoluteStepForPosition(0,bars,(20.5/64.)*width,width,dur);
 check(first==4&&second==20&&first!=second,"notes in different bars alias one step");
 check(e.laneLength[snare].load()==16,"new snare lane must start at 16 steps");
 e.set(pattern,snare,first,83);
 e.setEventFx(pattern,snare,first,1,75);
 check(e.get(pattern,snare,second)==0,"first-bar note automatically copied to second bar");
 check(applyStepPaint(e,pattern,snare,second,StepPaintAction::add),
       "cannot insert independent event into second bar");
 check(e.laneLength[snare].load()==24,"adding step 21 must grow only snare lane to 24 steps");
 check(e.laneLength[kick].load()==16,"adding snare changed the kick lane length");
 check(e.get(pattern,snare,first)==83,"later note overwrote original velocity");
 check(e.getEventFx(pattern,snare,first,1)==75,"later note overwrote original reverb");
 check(e.get(pattern,snare,second)==105,"second bar step not stored independently");
 check(e.get(pattern,snare,36)==0,"second-bar note mirrored into third bar");
 check(applyStepPaint(e,pattern,snare,second,StepPaintAction::erase),
       "cannot delete second-bar note");
 check(e.get(pattern,snare,first)==83&&e.get(pattern,snare,second)==0,
       "erasing later hit erased unrelated first-bar hit");
 check(setStepVelocityAt(e,pattern,snare,49,63),"precision velocity slider cannot insert late event");
 check(e.laneLength[snare].load()==52,"precision velocity must expand lane to step 50");
 check(e.get(pattern,snare,49)==63&&e.get(pattern,snare,first)==83,
       "late velocity incorrectly changed earlier step");
 check(absoluteStepForPpq(127.5*dur,dur)==127,"128th step cannot be selected");
 check(absoluteStepForPpq(128*dur,dur)==-1,"out-of-bounds step was wrapped");
 check(absoluteStepForPosition(0,bars,-.01,width,dur)==-1,"negative pointer accepted");
 check(absoluteStepForPosition(0,bars,width,width,dur)==-1,"pointer outside right edge accepted");
 for(int division=0;division<4;++division)
  for(int feel=0;feel<3;++feel){
   const double duration=divisionPPQ(division,feel);
   check(absoluteStepForPpq(12.1*duration,duration)==12,
         "dotted/triplet/straight step mapping incorrect");
   check(absoluteStepForPpq(44.1*duration,duration)==44,
         "two different rhythm bars map to a duplicated step");
  }
 std::puts("Timeline: distinct absolute steps, 4-bar display and automatic lane growth passed.");
}
