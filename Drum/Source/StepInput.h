#pragma once
#include "DrumEngine.h"

namespace inverso_drum {

// Mouse painting is idempotent: adding never erases an existing note or
// overwrites its carefully edited velocity; erasing clears that event alone.
enum class StepPaintAction { add, erase };

inline bool applyStepPaint(Engine& engine,int pattern,int pad,int step,
                           StepPaintAction action,int insertedVelocity=105) noexcept {
 if(pattern<0||pattern>=patterns||pad<0||pad>=pads||step<0||step>=steps)
  return false;
 const int before=engine.get(pattern,pad,step);
 if(action==StepPaintAction::add){
  if(before>0)return false;  // preserve custom velocity and per-hit FX
  engine.set(pattern,pad,step,std::clamp(insertedVelocity,1,127));
  return true;
 }
 // A deleted event must not leave hidden effects which reactivate on reinsertion.
 bool changed=before!=0;
 engine.set(pattern,pad,step,0);
 for(int fx=0;fx<3;++fx){
  if(engine.getEventFx(pattern,pad,step,fx)!=Engine::inheritFx)
   changed=true;
  engine.setEventFx(pattern,pad,step,fx,Engine::inheritFx);
 }
 return changed;
}

} // namespace inverso_drum
