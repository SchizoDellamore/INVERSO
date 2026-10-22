#pragma once
#include "DrumEngine.h"
#include <cmath>

namespace inverso_drum {

// Mouse painting is idempotent: adding never erases an existing note or
// overwrites its carefully edited velocity; erasing clears that event alone.
enum class StepPaintAction { add, erase };

// A timeline coordinate names one absolute step, never a modulo-wrapped loop cell.
// Range is 0..127, with -1 for coordinates outside the editable sequence.
inline int absoluteStepForPpq(double ppq,double stepDuration) noexcept {
 if(!std::isfinite(ppq)||!std::isfinite(stepDuration)||ppq<0||stepDuration<=0)
  return -1;
 const auto step=std::floor(ppq/stepDuration+1e-9);
 if(step<0||step>=steps)return -1;
 return static_cast<int>(step);
}
inline int absoluteStepForPosition(double viewStartPPQ,int visibleBars,
                                    double x,double viewportWidth,
                                    double stepDuration) noexcept {
 if(!std::isfinite(x)||!std::isfinite(viewportWidth)||viewportWidth<=0||
    x<0||x>=viewportWidth||visibleBars<1)return -1;
 return absoluteStepForPpq(viewStartPPQ+
             4.0*visibleBars*x/viewportWidth,stepDuration);
}
inline void ensureStepCanPlay(Engine& engine,int pad,int step) noexcept {
 if(pad<0||pad>=pads||step<0||step>=steps)return;
 // E.g. inserting step index 16 enlarges a 16-step lane to 20,
 // without touching the other 15 lanes or copying the earlier notes.
 const int required=((step+4)/4)*4;
 if(engine.laneLength[pad].load()<required)
  engine.setLaneLength(pad,required);
}
inline bool setStepVelocityAt(Engine& engine,int pattern,int pad,int step,
                              int velocity) noexcept {
 if(pattern<0||pattern>=patterns||pad<0||pad>=pads||step<0||step>=steps)return false;
 const int next=std::clamp(velocity,0,127);
 if(next>0)ensureStepCanPlay(engine,pad,step);
 const int previous=engine.get(pattern,pad,step);
 if(previous==next)return false;
 engine.set(pattern,pad,step,next);
 return true;
}

inline bool applyStepPaint(Engine& engine,int pattern,int pad,int step,
                           StepPaintAction action,int insertedVelocity=105) noexcept {
 if(pattern<0||pattern>=patterns||pad<0||pad>=pads||step<0||step>=steps)
  return false;
 const int before=engine.get(pattern,pad,step);
 if(action==StepPaintAction::add){
  ensureStepCanPlay(engine,pad,step);
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
