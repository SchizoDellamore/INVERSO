#pragma once
#include "StepInput.h"
#include <algorithm>
#include <array>
#include <memory>
#include <string>
#include <vector>

namespace inverso_drum {

class LaneUndoHistory {
public:
 struct Entry {
  int pattern=0,pad=0,length=16,rate=2,feel=0;
  float velocityMultiplier=1.f,gain=.8f,pan=0.f,drive=0.f,reverbSend=0.f,delaySend=0.f;
  std::array<uint8_t,steps> notes{};
  std::array<std::array<uint8_t,steps>,3> effects{};
  std::shared_ptr<const Sample> sample;
  bool userLoaded=false;
  std::string samplePath;
 };
 void capture(const Engine& e,int pattern,int pad,std::string path={}){
  if(pattern<0||pattern>=patterns||pad<0||pad>=pads)return;
  Entry state;
  state.pattern=pattern;state.pad=pad;state.samplePath=std::move(path);
  state.length=e.laneLength[pad].load();state.rate=e.division[pad].load();
  state.feel=e.feel[pad].load();
  state.velocityMultiplier=e.padVelocity[pad].load();
  state.gain=e.gain[pad].load();state.pan=e.pan[pad].load();
  state.drive=e.drive[pad].load();state.reverbSend=e.sendVerb[pad].load();
  state.delaySend=e.sendDelay[pad].load();
  state.sample=e.getSample(pad);state.userLoaded=e.isUserLoaded(pad);
  for(int i=0;i<steps;++i){
   state.notes[i]=e.get(pattern,pad,i);
   for(int fx=0;fx<3;++fx)
    state.effects[fx][i]=static_cast<uint8_t>(e.getEventFx(pattern,pad,i,fx));
  }
  auto& stack=stacks[pad];
  if(stack.size()>=64)stack.erase(stack.begin());
  stack.push_back(std::move(state));
 }
 bool canUndo(int pad) const{
  return pad>=0&&pad<pads&&!stacks[pad].empty();
 }
 bool undo(Engine& e,int pad,std::string* path=nullptr){
  if(!canUndo(pad))return false;
  auto& stack=stacks[pad];
  auto snapshot=std::move(stack.back());stack.pop_back();
  const int pattern=snapshot.pattern;
  e.setLaneLength(pad,snapshot.length);
  e.setDivision(pad,snapshot.rate,snapshot.feel);
  e.padVelocity[pad].store(snapshot.velocityMultiplier);
  e.gain[pad].store(snapshot.gain);e.pan[pad].store(snapshot.pan);
  e.drive[pad].store(snapshot.drive);
  e.sendVerb[pad].store(snapshot.reverbSend);
  e.sendDelay[pad].store(snapshot.delaySend);
  if(snapshot.sample)e.setSample(pad,std::move(snapshot.sample));
  e.setUserLoaded(pad,snapshot.userLoaded);
  for(int i=0;i<steps;++i){
   e.set(pattern,pad,i,snapshot.notes[i]);
   for(int fx=0;fx<3;++fx)
    e.setEventFx(pattern,pad,i,fx,snapshot.effects[fx][i]);
  }
  if(path)*path=std::move(snapshot.samplePath);
  return true;
 }
 void clear() {for(auto& stack:stacks)stack.clear();}
private:
 std::array<std::vector<Entry>,pads> stacks{};
};

// Clear only the specified instrument in the currently active pattern.
inline void eraseAllLaneNotes(Engine& e,int pattern,int pad){
 if(pattern<0||pattern>=patterns||pad<0||pad>=pads)return;
 for(int step=0;step<steps;++step)
  applyStepPaint(e,pattern,pad,step,StepPaintAction::erase);
}

// Selection never creates notes: only triggered/loaded events are selectable.
class StepSelection {
public:
 void clear() noexcept {
  for(auto& lane:selected)lane.fill(false);
 }
 void clearPad(int pad)noexcept{
  if(pad>=0&&pad<pads)selected[pad].fill(false);
 }
 bool has(int pad,int step)const noexcept{
  return pad>=0&&pad<pads&&step>=0&&step<steps&&selected[pad][step];
 }
 bool toggleLoaded(const Engine& engine,int pattern,int pad,int step)noexcept{
  if(pad<0||pad>=pads||step<0||step>=steps||engine.get(pattern,pad,step)==0)return false;
  selected[pad][step]=!selected[pad][step];
  return true;
 }
 bool addLoaded(const Engine& engine,int pattern,int pad,int step)noexcept{
  if(pad<0||pad>=pads||step<0||step>=steps||engine.get(pattern,pad,step)==0)return false;
  selected[pad][step]=true;return true;
 }
 int selectAllLoaded(const Engine& engine,int pattern,int pad)noexcept{
  if(pattern<0||pattern>=patterns||pad<0||pad>=pads)return 0;
  int count=0;
  for(int step=0;step<steps;++step){
   const bool loaded=engine.get(pattern,pad,step)>0;
   selected[pad][step]=loaded;
   if(loaded)++count;
  }
  return count;
 }
 int count(int pad)const noexcept{
  if(pad<0||pad>=pads)return 0;
  return static_cast<int>(std::count(selected[pad].begin(),selected[pad].end(),true));
 }
 template <typename Callback>
 void forEach(int pad,Callback&& callback)const{
  if(pad<0||pad>=pads)return;
  for(int step=0;step<steps;++step)
   if(selected[pad][step])callback(step);
 }
private:
 std::array<std::array<bool,steps>,pads> selected{};
};
}
