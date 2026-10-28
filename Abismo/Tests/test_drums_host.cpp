#include "../Source/DrumsProcessor.h"
#include <JuceHeader.h>
#include "StateRoundTrip.h"
#include <cmath>
#include <cstdio>
#include <cstdlib>

static void verify(bool okay,const char* message){
 if(!okay){std::fprintf(stderr,"DRUMS HOST SMOKE FAILED: %s\n",message);std::exit(1);}
}
static void param(InversoAbismoDrumsProcessor& p,const char* name,float value){
 auto* raw=dynamic_cast<juce::RangedAudioParameter*>(p.apvts.getParameter(name));
 verify(raw!=nullptr,"missing parameter");
 raw->setValueNotifyingHost(raw->convertTo0to1(value));
}
// Plays a short groove across host block sizes; returns RMS.
static float groove(InversoAbismoDrumsProcessor& p,int sr,int blockSize){
 double sum=0.0;long count=0;
 const int step=sr/6;
 for(int done=0;done<sr*2;done+=blockSize){
  juce::AudioBuffer<float> block(2,blockSize);
  block.clear();
  juce::MidiBuffer midi;
  for(int i=0;i<blockSize;++i){
   const int t=done+i;
   if(t%step==0){
    const int n=36+(t/step)%16;
    midi.addEvent(juce::MidiMessage::noteOn(10,n,.9f),i);
    midi.addEvent(juce::MidiMessage::noteOff(10,n),std::min(blockSize-1,i+1));
   }
  }
  p.processBlock(block,midi);
  for(int c=0;c<2;++c)for(int i=0;i<blockSize;++i){
   const float x=block.getSample(c,i);
   verify(std::isfinite(x) && std::abs(x)<=1.f,"invalid output");
   sum+=x*x;++count;
  }
 }
 return static_cast<float>(std::sqrt(sum/static_cast<double>(count)));
}

// Records what the plugin tells its host.
struct HostSpy final : juce::AudioProcessorListener {
 int programChanges=0;
 void audioProcessorParameterChanged(juce::AudioProcessor*,int,float) override {}
 void audioProcessorChanged(juce::AudioProcessor*,const ChangeDetails& d) override { if(d.programChanged) ++programChanges; }
};

int main(){
 juce::ScopedJuceInitialiser_GUI init;
 { InversoAbismoDrumsProcessor rt; if (!stateRoundTrip(rt, "drums")) return 1; }
 // the shared pitch-wheel mapping (abismo_ui): both ends reached, centre at rest
 verify(std::abs(abismo_ui::pitchBendOf(0)+1.f)<1e-6f && std::abs(abismo_ui::pitchBendOf(8192))<1e-6f && std::abs(abismo_ui::pitchBendOf(16383)-1.f)<1e-6f,"pitch wheel mapping");
 InversoAbismoDrumsProcessor p;
 juce::AudioProcessor::BusesLayout layout;
 layout.outputBuses.add(juce::AudioChannelSet::stereo());
 verify(p.setBusesLayout(layout),"stereo instrument layout rejected");
 verify(p.getNumPrograms()>=6,"kits missing");
 verify(p.apvts.getParameter("P16_PAN")!=nullptr && p.apvts.getParameter("SAW_WOBBLE")!=nullptr,"parameter table incomplete");
 for(int sr:{44100,48000,96000}){
  p.prepareToPlay(sr,128);
  for(int kit=0;kit<p.getNumPrograms();++kit){
   p.setCurrentProgram(kit);
   for(int size:{1,64,1024}) verify(groove(p,sr,size)>1e-3f,"groove produced no audio");
  }
 }
 // Choosing a kit in the plugin window tells the host (otherwise Cubase keeps
 // the old program and switches back to it); the host re-sending the current
 // kit must not wipe the user's edits.
 {
  HostSpy spy;
  p.addListener(&spy);
  p.setCurrentProgram(7);
  verify(spy.programChanges>=1 && p.getCurrentProgram()==7,"kit change not reported to the host");
  verify(std::lround(p.apvts.getRawParameterValue("P01_OBJECT")->load())==24,"kit values not applied");
  param(p,"P01_TUNE",5.f);
  p.setCurrentProgram(7);
  verify(std::abs(p.apvts.getRawParameterValue("P01_TUNE")->load()-5.f)<.05f,"re-sent program wiped an edit");
  p.removeListener(&spy);
  // The switch fades the old kit out and starts clean: no tail, no clicks over 1.
  p.prepareToPlay(48000,256);
  juce::AudioBuffer<float> b(2,256);
  for(int i=0;i<40;++i){ b.clear(); juce::MidiBuffer m; if(i%8==0) m.addEvent(juce::MidiMessage::noteOn(10,36+i%16,1.f),0); p.processBlock(b,m); }
  p.setCurrentProgram(9);
  b.clear(); { juce::MidiBuffer none; p.processBlock(b,none); }
  verify(std::abs(b.getSample(0,255))<1e-3f,"old kit not faded out at the switch");
  b.clear(); { juce::MidiBuffer none; p.processBlock(b,none); }
  verify(b.getMagnitude(0,256)<1e-3f,"old kit still sounding after the switch");
  // A note played right at the switch is not lost: it sounds on the new kit.
  p.setCurrentProgram(3);
  b.clear(); { juce::MidiBuffer m; m.addEvent(juce::MidiMessage::noteOn(10,36,1.f),10); p.processBlock(b,m); }
  float after=0.f; // the kit may stumble the hit by up to 80 ms
  for(int i=0;i<30;++i){ b.clear(); juce::MidiBuffer none; p.processBlock(b,none); after=std::max(after,b.getMagnitude(0,256)); }
  verify(after>1e-2f,"note at the kit switch was lost");
  // Two kit choices in a row: one fade, then the last kit, once; nothing of the old kit comes
  // back after the fade, and a note played in between sounds on the last kit.
  for(int i=0;i<10;++i){ b.clear(); juce::MidiBuffer m; if(i%3==0) m.addEvent(juce::MidiMessage::noteOn(10,36+i,1.f),0); p.processBlock(b,m); }
  b.clear(); { juce::MidiBuffer none; p.processBlock(b,none); }
  const float ringing=b.getMagnitude(0,256);
  p.selectKit(5);
  p.selectKit(8);
  b.clear(); { juce::MidiBuffer none; p.processBlock(b,none); }
  const float fadeStart=std::abs(b.getSample(0,0)), fadeEnd=std::abs(b.getSample(0,255));
  verify(fadeEnd<1e-3f,"old kit not faded out at a double kit change");
  verify(p.getCurrentProgram()==8,"the last kit chosen is not the current one");
  float quiet=0.f; // no note: after the one fade, nothing (the old kit must not come back)
  for(int i=0;i<8;++i){ b.clear(); juce::MidiBuffer none; p.processBlock(b,none); quiet=std::max(quiet,b.getMagnitude(0,256)); }
  std::printf("double kit change: ringing %.4f, fade %.4f -> %.4f, then %.6f\n",ringing,fadeStart,fadeEnd,quiet);
  verify(ringing>1e-3f,"test needs the old kit ringing at the change");
  verify(quiet<1e-4f,"the old kit came back after its fade");
  verify(std::lround(p.apvts.getRawParameterValue("P01_OBJECT")->load())==std::lround(inverso_abismo::drums::kitValues(8)[inverso_abismo::drums::P::Object]),"kit values of the last choice not applied");
  // a note played during a kit change sounds on the new kit
  p.selectKit(2);
  b.clear(); { juce::MidiBuffer m; m.addEvent(juce::MidiMessage::noteOn(10,38,1.f),5); p.processBlock(b,m); }
  float note=0.f;
  for(int i=0;i<30;++i){ b.clear(); juce::MidiBuffer none; p.processBlock(b,none); note=std::max(note,b.getMagnitude(0,256)); }
  verify(note>1e-2f,"note at a kit change was lost");
  // two kit choices in a row with a note in between: it sounds on the last kit
  p.selectKit(4);
  p.selectKit(6);
  b.clear(); { juce::MidiBuffer m; m.addEvent(juce::MidiMessage::noteOn(10,36,1.f),5); p.processBlock(b,m); }
  float note2=0.f;
  for(int i=0;i<30;++i){ b.clear(); juce::MidiBuffer none; p.processBlock(b,none); note2=std::max(note2,b.getMagnitude(0,256)); }
  verify(note2>1e-2f && p.getCurrentProgram()==6,"note at a double kit change was lost");
  // A MIDI flood during a kit change (1200 controllers between a saw note-on and its note-off):
  // the held note-on plays on the new kit (control: without the note-off the saw sounds), and
  // the note-off after the flood is kept (with it, the saw does not hang).
  auto flood=[&](bool withNoteOff){
   for(int i=0;i<300;++i){ b.clear(); juce::MidiBuffer none; p.processBlock(b,none); } // all rung out
   p.selectKit(1);
   b.clear();
   juce::MidiBuffer m;
   m.addEvent(juce::MidiMessage::noteOn(1,64,1.f),0);
   for(int k=0;k<1200;++k) m.addEvent(juce::MidiMessage::controllerEvent(1,1,k%128),1+k%250);
   if(withNoteOff) m.addEvent(juce::MidiMessage::noteOff(1,64),255);
   p.processBlock(b,m);
   float late=0.f;
   for(int i=0;i<400;++i){ b.clear(); juce::MidiBuffer none; p.processBlock(b,none); if(i>300) late=std::max(late,b.getMagnitude(0,256)); }
   if(!withNoteOff){ juce::MidiBuffer off; off.addEvent(juce::MidiMessage::noteOff(1,64),0); b.clear(); p.processBlock(b,off); }
   return late;
  };
  const float held=flood(false), released=flood(true);
  std::printf("MIDI flood at a kit change: saw %.4f without its note-off, %.6f with it\n",held,released);
  // a pad hit after the flood still plays (controllers never take a note's place)
  for(int i=0;i<300;++i){ b.clear(); juce::MidiBuffer none; p.processBlock(b,none); }
  p.selectKit(2);
  { b.clear(); juce::MidiBuffer m;
    for(int k=0;k<1500;++k) m.addEvent(juce::MidiMessage::controllerEvent(1,1+k%100,k%128),k%250);
    for(int k=0;k<300;++k) m.addEvent(juce::MidiMessage::aftertouchChange(10,36,k%128),k%250);
    m.addEvent(juce::MidiMessage::noteOn(10,36,1.f),255);
    p.processBlock(b,m); }
  float hit=0.f;
  for(int i=0;i<30;++i){ b.clear(); juce::MidiBuffer none; p.processBlock(b,none); hit=std::max(hit,b.getMagnitude(0,256)); }
  verify(hit>1e-2f,"a pad hit after a controller flood at a kit change was lost");
  // hundreds of saw notes under a held sustain at a kit change, then sustain off: the saw played
  // after the change must not keep ringing (the pedal's release is kept, whatever came before)
  for(int i=0;i<300;++i){ b.clear(); juce::MidiBuffer none; p.processBlock(b,none); }
  p.selectKit(3);
  { b.clear(); juce::MidiBuffer m;
    m.addEvent(juce::MidiMessage::controllerEvent(1,64,127),0);
    for(int k=0;k<600;++k){ m.addEvent(juce::MidiMessage::noteOn(1,60+k%30,1.f),k%250); m.addEvent(juce::MidiMessage::noteOff(1,60+k%30),k%250); }
    m.addEvent(juce::MidiMessage::controllerEvent(1,64,0),255);
    p.processBlock(b,m); }
  for(int i=0;i<300;++i){ b.clear(); juce::MidiBuffer none; p.processBlock(b,none); }
  { b.clear(); juce::MidiBuffer m; m.addEvent(juce::MidiMessage::noteOn(1,64,1.f),0); p.processBlock(b,m); }
  for(int i=0;i<60;++i){ b.clear(); juce::MidiBuffer none; p.processBlock(b,none); }
  { b.clear(); juce::MidiBuffer m; m.addEvent(juce::MidiMessage::noteOff(1,64),0); p.processBlock(b,m); }
  float ring=0.f;
  for(int i=0;i<400;++i){ b.clear(); juce::MidiBuffer none; p.processBlock(b,none); if(i>300) ring=std::max(ring,b.getMagnitude(0,256)); }
  // control: the same saw note on the same kit with no kit change
  float control=0.f;
  { b.clear(); juce::MidiBuffer m; m.addEvent(juce::MidiMessage::noteOn(1,64,1.f),0); p.processBlock(b,m); }
  for(int i=0;i<60;++i){ b.clear(); juce::MidiBuffer none; p.processBlock(b,none); }
  { b.clear(); juce::MidiBuffer m; m.addEvent(juce::MidiMessage::noteOff(1,64),0); p.processBlock(b,m); }
  for(int i=0;i<400;++i){ b.clear(); juce::MidiBuffer none; p.processBlock(b,none); if(i>300) control=std::max(control,b.getMagnitude(0,256)); }
  std::printf("sustain at a kit change: saw tail %.6f (same note, no change: %.6f)\n",ring,control);
  verify(ring<1e-3f,"sustain off at a kit change was lost: the saw keeps ringing");
  // the pitch bend moved during a kit change reaches the new kit: the same saw note measured
  // with the bend (held during a kit change) and without it (control)
  auto sawPitch=[&](bool bendIt){
   for(int i=0;i<300;++i){ b.clear(); juce::MidiBuffer none; p.processBlock(b,none); }
   p.selectKit(4);
   { b.clear(); juce::MidiBuffer m; m.addEvent(juce::MidiMessage::pitchWheel(1,bendIt?16383:8192),0); m.addEvent(juce::MidiMessage::noteOn(1,69,1.f),1); p.processBlock(b,m); }
   juce::AudioBuffer<float> out(2,48000);
   for(int i=0;i<187;++i){ b.clear(); juce::MidiBuffer none; p.processBlock(b,none); out.copyFrom(0,i*256,b,0,0,256); }
   { b.clear(); juce::MidiBuffer m; m.addEvent(juce::MidiMessage::noteOff(1,69),0); m.addEvent(juce::MidiMessage::pitchWheel(1,8192),0); p.processBlock(b,m); }
   int crossings=0; for(int i=24000;i<47000;++i) if(out.getSample(0,i-1)<0.f && out.getSample(0,i)>=0.f) ++crossings;
   return crossings/(23000.0/48000.0);
  };
  const double flat=sawPitch(false), bentHz=sawPitch(true);
  std::printf("bend held at a kit change: A4 saw %.1f Hz unbent, %.1f Hz bent (ratio %.3f, two semitones = 1.122)\n",flat,bentHz,bentHz/flat);
  verify(bentHz/flat>1.08 && bentHz/flat<1.16,"the pitch bend moved during a kit change did not reach the new kit");
  // the saw ends on the last note pressed during a kit change, as it would have played
  for(int i=0;i<300;++i){ b.clear(); juce::MidiBuffer none; p.processBlock(b,none); }
  p.selectKit(5);
  { b.clear(); juce::MidiBuffer m; m.addEvent(juce::MidiMessage::noteOn(1,81,1.f),0); m.addEvent(juce::MidiMessage::noteOn(1,69,1.f),10); p.processBlock(b,m); }
  juce::AudioBuffer<float> last(2,48000);
  for(int i=0;i<187;++i){ b.clear(); juce::MidiBuffer none; p.processBlock(b,none); last.copyFrom(0,i*256,b,0,0,256); }
  { b.clear(); juce::MidiBuffer m; m.addEvent(juce::MidiMessage::allNotesOff(1),0); p.processBlock(b,m); }
  int lastCross=0; for(int i=24000;i<47000;++i) if(last.getSample(0,i-1)<0.f && last.getSample(0,i)>=0.f) ++lastCross;
  const double lastHz=lastCross/(23000.0/48000.0);
  std::printf("saw after A5 then A4 pressed at a kit change: %.1f Hz (A4 %.1f Hz)\n",lastHz,flat);
  verify(std::abs(lastHz/flat-1.0)<.03,"the saw did not end on the last note pressed during a kit change");
  // pedal down, a saw note pressed and let go during a kit change: it keeps sounding (the pedal
  // holds it) until the pedal is up, then fades
  for(int i=0;i<300;++i){ b.clear(); juce::MidiBuffer none; p.processBlock(b,none); }
  { b.clear(); juce::MidiBuffer m; m.addEvent(juce::MidiMessage::controllerEvent(1,64,127),0); p.processBlock(b,m); }
  p.selectKit(6);
  { b.clear(); juce::MidiBuffer m; m.addEvent(juce::MidiMessage::noteOn(1,69,1.f),0); m.addEvent(juce::MidiMessage::noteOff(1,69),100); p.processBlock(b,m); }
  float pedalHeld=0.f;
  for(int i=0;i<200;++i){ b.clear(); juce::MidiBuffer none; p.processBlock(b,none); if(i>150) pedalHeld=std::max(pedalHeld,b.getMagnitude(0,256)); }
  { b.clear(); juce::MidiBuffer m; m.addEvent(juce::MidiMessage::controllerEvent(1,64,0),0); p.processBlock(b,m); }
  float afterPedal=0.f;
  for(int i=0;i<400;++i){ b.clear(); juce::MidiBuffer none; p.processBlock(b,none); if(i>300) afterPedal=std::max(afterPedal,b.getMagnitude(0,256)); }
  // control: the same gesture on the same kit, no kit change
  for(int i=0;i<300;++i){ b.clear(); juce::MidiBuffer none; p.processBlock(b,none); }
  { b.clear(); juce::MidiBuffer m; m.addEvent(juce::MidiMessage::controllerEvent(1,64,127),0); m.addEvent(juce::MidiMessage::noteOn(1,69,1.f),0); m.addEvent(juce::MidiMessage::noteOff(1,69),100); p.processBlock(b,m); }
  float controlHeld=0.f;
  for(int i=0;i<200;++i){ b.clear(); juce::MidiBuffer none; p.processBlock(b,none); if(i>150) controlHeld=std::max(controlHeld,b.getMagnitude(0,256)); }
  { b.clear(); juce::MidiBuffer m; m.addEvent(juce::MidiMessage::controllerEvent(1,64,0),0); p.processBlock(b,m); }
  float controlAfter=0.f;
  for(int i=0;i<400;++i){ b.clear(); juce::MidiBuffer none; p.processBlock(b,none); if(i>300) controlAfter=std::max(controlAfter,b.getMagnitude(0,256)); }
  std::printf("saw let go under the pedal at a kit change: %.4f while held, %.6f after the pedal (no change: %.4f, %.6f)\n",pedalHeld,afterPedal,controlHeld,controlAfter);
  verify(pedalHeld>controlHeld*.5f,"a saw note held by the pedal during a kit change was lost");
  verify(afterPedal<controlAfter*2.f+1e-4f,"a saw note held by the pedal did not stop when the pedal was released");
  // the pedal pressed in the same block as the kit change, before the note is let go, holds it too
  for(int i=0;i<300;++i){ b.clear(); juce::MidiBuffer none; p.processBlock(b,none); }
  p.selectKit(6);
  { b.clear(); juce::MidiBuffer m; m.addEvent(juce::MidiMessage::controllerEvent(1,64,127),0); m.addEvent(juce::MidiMessage::noteOn(1,69,1.f),5); m.addEvent(juce::MidiMessage::noteOff(1,69),100); p.processBlock(b,m); }
  float sameBlock=0.f;
  for(int i=0;i<200;++i){ b.clear(); juce::MidiBuffer none; p.processBlock(b,none); if(i>150) sameBlock=std::max(sameBlock,b.getMagnitude(0,256)); }
  { b.clear(); juce::MidiBuffer m; m.addEvent(juce::MidiMessage::controllerEvent(1,64,0),0); p.processBlock(b,m); }
  verify(sameBlock>controlHeld*.5f,"a pedal pressed in the kit-change block did not hold the saw note");
  // a stop (the host releases the plugin) puts the pedal up, even with no pedal-up sent: a saw
  // note played after it is not held
  { b.clear(); juce::MidiBuffer m; m.addEvent(juce::MidiMessage::controllerEvent(1,64,127),0); p.processBlock(b,m); }
  p.releaseResources(); p.prepareToPlay(48000,256);
  { b.clear(); juce::MidiBuffer m; m.addEvent(juce::MidiMessage::noteOn(1,69,1.f),0); p.processBlock(b,m); }
  for(int i=0;i<60;++i){ b.clear(); juce::MidiBuffer none; p.processBlock(b,none); }
  { b.clear(); juce::MidiBuffer m; m.addEvent(juce::MidiMessage::noteOff(1,69),0); p.processBlock(b,m); }
  float afterStop=0.f; for(int i=0;i<400;++i){ b.clear(); juce::MidiBuffer none; p.processBlock(b,none); if(i>300) afterStop=std::max(afterStop,b.getMagnitude(0,256)); }
  verify(afterStop<controlAfter*2.f+1e-4f,"the pedal stayed down after a stop");
  // a re-prepare in the middle of playing keeps the pedal as it is (a held pedal sends nothing new)
  { b.clear(); juce::MidiBuffer m; m.addEvent(juce::MidiMessage::controllerEvent(1,64,127),0); p.processBlock(b,m); }
  p.prepareToPlay(48000,256);
  { b.clear(); juce::MidiBuffer m; m.addEvent(juce::MidiMessage::noteOn(1,69,1.f),0); p.processBlock(b,m); }
  for(int i=0;i<60;++i){ b.clear(); juce::MidiBuffer none; p.processBlock(b,none); }
  { b.clear(); juce::MidiBuffer m; m.addEvent(juce::MidiMessage::noteOff(1,69),0); p.processBlock(b,m); }
  float kept=0.f; for(int i=0;i<200;++i){ b.clear(); juce::MidiBuffer none; p.processBlock(b,none); if(i>150) kept=std::max(kept,b.getMagnitude(0,256)); }
  { b.clear(); juce::MidiBuffer m; m.addEvent(juce::MidiMessage::controllerEvent(1,64,0),0); p.processBlock(b,m); }
  verify(kept>controlHeld*.5f,"a re-prepare let go of a held pedal");

  // several hits on one pad during a kit change: one hit, not a stacked burst
  for(int i=0;i<300;++i){ b.clear(); juce::MidiBuffer none; p.processBlock(b,none); }
  auto padPeak=[&](int hits){
   for(int i=0;i<300;++i){ b.clear(); juce::MidiBuffer none; p.processBlock(b,none); }
   p.selectKit(2);
   param(p,"STUMBLE",0); // no random delay on the hits: the comparison is about stacking
   b.clear(); juce::MidiBuffer m; for(int k=0;k<hits;++k) m.addEvent(juce::MidiMessage::noteOn(10,38,1.f),k*20); p.processBlock(b,m);
   float pk=0.f; for(int i=0;i<40;++i){ b.clear(); juce::MidiBuffer none; p.processBlock(b,none); pk=std::max(pk,b.getMagnitude(0,256)); }
   return pk;
  };
  // the pad alternates recordings (round robin), so compare with the loudest of several single hits
  float one=0.f,eight=0.f;
  for(int r=0;r<6;++r) one=std::max(one,padPeak(1));
  for(int r=0;r<6;++r) eight=std::max(eight,padPeak(8));
  std::printf("pad hit at a kit change: loudest single hit %.3f, loudest of eight at once %.3f\n",one,eight);
  verify(eight<one*1.3f,"repeated hits on a pad at a kit change stacked into a burst");
  verify(eight>one*.6f,"hits on a pad at a kit change came out quieter than the loudest one played");
  // all-notes-off at a kit change (hosts send it at a loop's end) keeps the pad hits, as in play
  for(int i=0;i<300;++i){ b.clear(); juce::MidiBuffer none; p.processBlock(b,none); }
  p.selectKit(2);
  param(p,"STUMBLE",0);
  { b.clear(); juce::MidiBuffer m; m.addEvent(juce::MidiMessage::noteOn(10,36,1.f),0); m.addEvent(juce::MidiMessage::allNotesOff(1),10); p.processBlock(b,m); }
  float kick=0.f; for(int i=0;i<40;++i){ b.clear(); juce::MidiBuffer none; p.processBlock(b,none); kick=std::max(kick,b.getMagnitude(0,256)); }
  verify(kick>1e-2f,"all-notes-off at a kit change dropped a pad hit");
  // all-notes-off at a kit change lets go of a saw note pressed during it (no saw on the new kit)
  for(int i=0;i<300;++i){ b.clear(); juce::MidiBuffer none; p.processBlock(b,none); }
  p.selectKit(2);
  { b.clear(); juce::MidiBuffer m; m.addEvent(juce::MidiMessage::noteOn(1,69,1.f),0); m.addEvent(juce::MidiMessage::allNotesOff(1),10); p.processBlock(b,m); }
  float sawAfterOff=0.f; for(int i=0;i<300;++i){ b.clear(); juce::MidiBuffer none; p.processBlock(b,none); if(i>250) sawAfterOff=std::max(sawAfterOff,b.getMagnitude(0,256)); }
  verify(sawAfterOff<1e-3f,"all-notes-off at a kit change did not let go of the saw");
  verify(held>1e-2f,"a note-on held during a kit change did not play on the new kit");
  verify(released<1e-3f,"a note-off held during a kit change was dropped: the saw hangs");
 }
 // Audition from the editor path, and pad lights.
 p.prepareToPlay(48000,512);
 p.setCurrentProgram(0);
 param(p,"STUMBLE",0);
 p.takePadLights();
 p.auditionPad(4);
 juce::AudioBuffer<float> block(2,512);block.clear();
 juce::MidiBuffer none;
 p.processBlock(block,none); // fades out the previous kit; the audition waits for the next block
 block.clear();
 p.processBlock(block,none);
 verify(block.getMagnitude(0,512)>1e-3f,"audition produced no audio");
 verify(p.takePadLights()==(1u<<4),"pad light missing");
 // Musical saw on C3 and above, released by note-off.
 juce::MidiBuffer saw;saw.addEvent(juce::MidiMessage::noteOn(1,69,1.f),0);
 for(int i=0;i<40;++i){block.clear();p.processBlock(block,saw);saw.clear();}
 verify(block.getMagnitude(0,512)>1e-3f,"saw silent");
 // State round trip with a reassigned pad.
 param(p,"P03_OBJECT",13.f);
 param(p,"P03_TUNE",-7.f);
 juce::MemoryBlock saved;
 p.getStateInformation(saved);
 InversoAbismoDrumsProcessor restored;
 restored.setStateInformation(saved.getData(),static_cast<int>(saved.getSize()));
 verify(std::lround(restored.apvts.getRawParameterValue("P03_OBJECT")->load())==13,"pad object not restored");
 verify(std::abs(restored.apvts.getRawParameterValue("P03_TUNE")->load()+7.f)<.05f,"pad tune not restored");
 // The recordings are found (source tree in this build) and play by default.
 verify(static_cast<int>(InversoAbismoDrumsProcessor::library().objects.size())==inverso_abismo::drums::numLibraryObjects,"drum library missing");
 // A pad loaded with the user's own takes: several files become dynamics, survive a session
 // round trip, fall back to the OBJECT when the files are gone, and clear back to the OBJECT.
 {
  auto dir=juce::File::getSpecialLocation(juce::File::tempDirectory).getChildFile("abismo_drums_host_takes");
  dir.deleteRecursively();dir.createDirectory();
  juce::Array<juce::File> takes;
  for(int k=0;k<3;++k){
   auto f=dir.getChildFile("take"+juce::String(k)+".wav");
   juce::WavAudioFormat wav;
   std::unique_ptr<juce::OutputStream> stream=f.createOutputStream();
   auto writer=wav.createWriterFor(stream,juce::AudioFormatWriterOptions{}.withSampleRate(44100).withNumChannels(1).withBitsPerSample(16));
   verify(writer!=nullptr,"wav writer");
   juce::AudioBuffer<float> b(1,8000);
   for(int i=0;i<8000;++i) b.setSample(0,i,(.15f+.3f*static_cast<float>(k))*std::sin(6.2831853f*220.f*static_cast<float>(i)/44100.f)*std::exp(-static_cast<float>(i)/3000.f));
   writer->writeFromAudioSampleBuffer(b,0,8000);
   takes.add(f);
  }
  p.prepareToPlay(48000,512);
  verify(p.loadPadFiles(2,takes),"pad files not loaded");
  verify(p.getPadSampleName(2).contains("(3)"),"pad sample name");
  param(p,"STUMBLE",0);
  juce::AudioBuffer<float> b(2,512);juce::MidiBuffer m;
  for(int i=0;i<20;++i){b.clear();p.processBlock(b,m);} // swap in, let earlier hits die
  m.addEvent(juce::MidiMessage::noteOn(10,38,1.f),0);
  b.clear();p.processBlock(b,m);
  verify(b.getMagnitude(0,512)>1e-3f,"pad sample silent");
  juce::MemoryBlock withTakes;p.getStateInformation(withTakes);
  InversoAbismoDrumsProcessor again;
  again.setStateInformation(withTakes.getData(),static_cast<int>(withTakes.getSize()));
  verify(again.getPadSampleName(2)==p.getPadSampleName(2),"pad sample not restored");
  // a program change from the host keeps your recordings; choosing a kit in the editor brings back its own sounds
  again.setCurrentProgram((again.getCurrentProgram()+1)%again.getNumPrograms());
  verify(again.getPadSampleName(2)==p.getPadSampleName(2),"a host program change dropped a pad recording");
  again.selectKit((again.getCurrentProgram()+1)%again.getNumPrograms());
  verify(again.getPadSampleName(2).isEmpty() && again.getPadMissing(2).isEmpty(),"pad recording kept after choosing a kit");
  {
   InversoAbismoDrumsProcessor same;
   same.setStateInformation(withTakes.getData(),static_cast<int>(withTakes.getSize()));
   verify(same.getPadSampleName(2).isNotEmpty(),"pad recording not restored");
   same.selectKit(same.getCurrentProgram());
   verify(same.getPadSampleName(2).isEmpty(),"pad recording kept after choosing the same kit again");
  }
  dir.deleteRecursively();
  InversoAbismoDrumsProcessor lost;
  lost.setStateInformation(withTakes.getData(),static_cast<int>(withTakes.getSize()));
  verify(lost.getPadSampleName(2).isEmpty() && lost.getPadMissing(2).contains("take0"),"missing takes not reported");
  juce::MemoryBlock keep;lost.getStateInformation(keep);
  InversoAbismoDrumsProcessor kept;kept.setStateInformation(keep.getData(),static_cast<int>(keep.getSize()));
  verify(kept.getPadMissing(2).contains("take2"),"missing paths forgotten on save");
  p.clearPadSample(2);
  for(int i=0;i<3;++i){b.clear();m.clear();p.processBlock(b,m);}
  p.collectGarbage();
  verify(p.getPadSampleName(2).isEmpty(),"pad not cleared");
 }
 p.setCurrentProgram(0); // editor opens on the default kit
 std::unique_ptr<juce::AudioProcessorEditor> editor(p.createEditor());
 verify(editor!=nullptr && editor->getWidth()>0,"editor failed to open");
 if(const char* path=std::getenv("ABISMO_SNAPSHOT")){
  const auto image=editor->createComponentSnapshot(editor->getLocalBounds());
  juce::File file(juce::File::getCurrentWorkingDirectory().getChildFile(path));
  file.deleteFile();
  juce::FileOutputStream out(file);
  juce::PNGImageFormat().writeImageToStream(image,out);
 }
 editor.reset();
 std::puts("Abismo drums JUCE host smoke passed");
}
