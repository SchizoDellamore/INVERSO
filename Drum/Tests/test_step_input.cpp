#include "../Source/StepInput.h"
#include <cstdio>
#include <cstdlib>
using namespace inverso_drum;
static void ck(bool ok,const char* msg){
 if(!ok){std::fprintf(stderr,"STEP MOUSE EDIT TEST FAILED: %s\n",msg);std::exit(1);}
}
int main(){
 Engine e;e.prepare(48000.);
 const int pattern=3, snare=1, kick=0;
 e.set(pattern,snare,12,77);
 e.set(pattern,snare,13,41);
 e.set(pattern,kick,12,119);
 e.setEventFx(pattern,snare,12,0,80);
 e.setEventFx(pattern,snare,12,1,72);
 e.setEventFx(pattern,snare,12,2,60);
 e.setEventFx(pattern,snare,13,1,30);
 // LMB: adds empty note with a sensible default velocity.
 ck(applyStepPaint(e,pattern,snare,14,StepPaintAction::add),"LMB did not add an empty step");
 ck(e.get(pattern,snare,14)==105,"new note velocity is wrong");
 // Repeated LMB and left-drag must never toggle an existing note OFF.
 ck(!applyStepPaint(e,pattern,snare,14,StepPaintAction::add),"LMB incorrectly toggled same note");
 ck(e.get(pattern,snare,14)==105,"LMB repeated click erased note");
 ck(!applyStepPaint(e,pattern,snare,12,StepPaintAction::add),"LMB changed existing note");
 ck(e.get(pattern,snare,12)==77,"LMB reset manually edited velocity");
 ck(e.getEventFx(pattern,snare,12,0)==80 &&
    e.getEventFx(pattern,snare,12,1)==72 &&
    e.getEventFx(pattern,snare,12,2)==60,
    "LMB destroyed existing per-step FX");
 // RMB: deletes only the note under mouse; clears orphaned event FX.
 ck(applyStepPaint(e,pattern,snare,12,StepPaintAction::erase),"RMB did not delete a note");
 ck(e.get(pattern,snare,12)==0,"RMB did not erase velocity");
 for(int fx=0;fx<3;++fx)
  ck(e.getEventFx(pattern,snare,12,fx)==Engine::inheritFx,
     "RMB deleted note but left hidden effect overrides");
 ck(e.get(pattern,snare,13)==41,"RMB erased adjacent snare event");
 ck(e.getEventFx(pattern,snare,13,1)==30,"RMB erased adjacent step's reverb");
 ck(e.get(pattern,kick,12)==119,"RMB erased same-step kick in another lane");
 ck(!applyStepPaint(e,pattern,snare,12,StepPaintAction::erase),"RMB repeated delete must be idempotent");
 ck(e.get(pattern,snare,12)==0,"RMB repeated click recreated note");
 ck(e.get(0,snare,12)==110,"RMB modified another pattern's snare");
 // Drag-paint paths are independent and should behave consistently over consecutive steps.
 for(int step=25;step<32;++step)
  ck(applyStepPaint(e,pattern,snare,step,StepPaintAction::add),"LMB drag failed to paint step");
 for(int step=25;step<32;++step)
  ck(e.get(pattern,snare,step)==105,"LMB drag failed to fill consecutive notes");
 for(int step=26;step<30;++step)
  ck(applyStepPaint(e,pattern,snare,step,StepPaintAction::erase),"RMB drag failed to erase");
 ck(e.get(pattern,snare,25)==105 && e.get(pattern,snare,30)==105 &&
    e.get(pattern,snare,31)==105,"RMB drag erased outside intended region");
 for(int step=26;step<30;++step)
  ck(e.get(pattern,snare,step)==0,"RMB drag left stray notes");
 // Guard against invalid coordinates overwriting memory.
 ck(!applyStepPaint(e,pattern,snare,128,StepPaintAction::erase),"accepted invalid step");
 ck(!applyStepPaint(e,pattern,16,12,StepPaintAction::add),"accepted invalid lane");
 std::puts("LMB insert / RMB erase, idempotent drag and independence tests passed.");
 return 0;
}
