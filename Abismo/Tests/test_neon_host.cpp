#include "../Source/NeonProcessor.h"
#include <JuceHeader.h>
#include "StateRoundTrip.h"
#include <cmath>
#include <cstdio>
#include <cstdlib>

static void verify(bool okay,const char* message){
 if(!okay){std::fprintf(stderr,"HOST SMOKE FAILED: %s\n",message);std::exit(1);}
}
static void param(InversoNeonProcessor& p,const char* name,float value){
 auto* raw=dynamic_cast<juce::RangedAudioParameter*>(p.apvts.getParameter(name));
 verify(raw!=nullptr,"missing parameter");
 raw->setValueNotifyingHost(raw->convertTo0to1(value));
}
// Plays a note at a block-relative offset across host block sizes; returns RMS.
static float play(InversoNeonProcessor& p,int sr,int blockSize,int note){
 double sum=0.0;long count=0;
 const int total=sr*2;
 for(int done=0;done<total;done+=blockSize){
  juce::AudioBuffer<float> block(2,blockSize);
  block.clear();
  juce::MidiBuffer midi;
  if(done==0) midi.addEvent(juce::MidiMessage::noteOn(1,note,.9f),std::min(blockSize-1,7));
  if(done<=sr && done+blockSize>sr) midi.addEvent(juce::MidiMessage::noteOff(1,note),sr-done);
  p.processBlock(block,midi);
  for(int c=0;c<2;++c)for(int i=0;i<blockSize;++i){
   const float x=block.getSample(c,i);
   verify(std::isfinite(x),"non-finite output");
   verify(std::abs(x)<=1.f,"output above 0 dBFS");
   sum+=x*x;++count;
  }
 }
 return static_cast<float>(std::sqrt(sum/static_cast<double>(count)));
}

int main(){
 juce::ScopedJuceInitialiser_GUI init;
 { InversoNeonProcessor rt; if (!stateRoundTrip(rt, "neon")) return 1; }
 InversoNeonProcessor p;
 juce::AudioProcessor::BusesLayout layout;
 layout.outputBuses.add(juce::AudioChannelSet::stereo());
 verify(p.setBusesLayout(layout),"stereo instrument layout rejected");
 verify(p.acceptsMidi(),"instrument must accept MIDI");
 verify(p.getNumPrograms()>=20,"factory presets missing");
 for(int sr:{44100,48000,96000}){
  p.prepareToPlay(sr,64); // Hosts may send other block sizes than announced.
  for(int program=0;program<p.getNumPrograms();program+=3){
   p.selectProgram(program);
   for(int size:{1,17,64,512,2048}){
    const float level=play(p,sr,size,57);
    verify(level>1e-4f,"note produced no audio");
   }
  }
  // Each sound source on its own must sound.
  for(const char* source:{"OSC1_LEVEL","OSC2_LEVEL","SUB_LEVEL","NOISE_LEVEL","FM_LEVEL"}){
   p.selectProgram(7); p.selectProgram(6);
   for(const char* other:{"OSC1_LEVEL","OSC2_LEVEL","SUB_LEVEL","NOISE_LEVEL","FM_LEVEL"}) param(p,other,0.f);
   param(p,source,100.f);
   verify(play(p,sr,256,57)>1e-4f,"a sound source on its own produced no audio");
  }
  // Silence without MIDI after everything has rung out.
  param(p,"REV_MIX",0.f);
  param(p,"DELAY_MIX",0.f);
  juce::MidiBuffer midi;
  midi.addEvent(juce::MidiMessage::allSoundOff(1),0);
  juce::AudioBuffer<float> block(2,sr);
  p.processBlock(block,midi);
  midi.clear();
  block.clear();
  p.processBlock(block,midi);
  verify(block.getMagnitude(0,sr)<1e-6f,"all sound off must silence the instrument");
 }
 // Sustain pedal and automation inside a running block.
 p.prepareToPlay(48000,512);
 p.selectProgram(0);
 juce::MidiBuffer midi;
 midi.addEvent(juce::MidiMessage::controllerEvent(1,64,127),0);
 midi.addEvent(juce::MidiMessage::noteOn(1,60,1.f),10);
 midi.addEvent(juce::MidiMessage::noteOff(1,60),100);
 juce::AudioBuffer<float> block(2,512);
 block.clear();
 p.processBlock(block,midi);
 midi.clear();
 for(int i=0;i<200;++i){param(p,"CUTOFF",200.f+static_cast<float>(i%100)*20.f);block.clear();p.processBlock(block,midi);}
 verify(p.voicesSounding.load()==1,"sustain pedal must hold the voice");
 midi.addEvent(juce::MidiMessage::controllerEvent(1,64,0),0);
 for(int i=0;i<400;++i){block.clear();p.processBlock(block,midi);midi.clear();}
 verify(p.voicesSounding.load()==0,"pedal up must release the voice");


 // MIDI learn: arm a knob, turn CC 21, the knob follows; the assignment is saved with the session.
 {
  const int index=p.midiLearn.indexOf("RESONANCE");
  verify(index>=0,"parameter not found for MIDI learn");
  p.midiLearn.arm(index);
  juce::MidiBuffer cc;
  cc.addEvent(juce::MidiMessage::controllerEvent(1,21,127),2);
  juce::AudioBuffer<float> ccBlock(2,512);
  ccBlock.clear();
  p.processBlock(ccBlock,cc);
  p.midiLearn.flush();
  verify(p.midiLearn.ccFor(index)==21,"MIDI learn must take CC 21");
  auto* param=p.apvts.getParameter("RESONANCE");
  verify(param->getValue()>.99f,"CC 21 at 127 must turn the knob fully up");
  cc.clear(); cc.addEvent(juce::MidiMessage::controllerEvent(1,21,0),0);
  block.clear(); p.processBlock(block,cc); p.midiLearn.flush();
  verify(param->getValue()<.01f,"CC 21 at 0 must turn the knob fully down");
  juce::MemoryBlock saved;
  p.getStateInformation(saved);
  InversoNeonProcessor other;
  other.setStateInformation(saved.getData(),static_cast<int>(saved.getSize()));
  verify(other.midiLearn.ccFor(index)==21,"MIDI learn not restored with the session");
  p.midiLearn.forget(index);
  verify(p.midiLearn.ccFor(index)<0,"forget did not forget");
 }
 // A host re-sending a (stale) program must not wipe edits; with no edits it applies.
 {
  p.selectProgram(3);
  param(p,"RESONANCE",12.f); param(p,"LATCH",1.f);
  p.setCurrentProgram(0);
  verify(std::abs(p.apvts.getRawParameterValue("RESONANCE")->load()-12.f)<.01f && p.apvts.getRawParameterValue("LATCH")->load()>.5f,
         "a program change from the host wiped the user's settings");
  p.selectProgram(3);
  p.setCurrentProgram(0);
  verify(p.getCurrentProgram()==0,"an unedited preset did not follow the host's program change");
 }
 // Program change and state round trip.
 p.selectProgram(5);
 param(p,"GLIDE",7.5f);
 juce::MemoryBlock saved;
 p.getStateInformation(saved);
 verify(saved.getSize()>0,"empty plugin state");
 InversoNeonProcessor another;
 another.setStateInformation(saved.getData(),static_cast<int>(saved.getSize()));
 verify(std::abs(another.apvts.getRawParameterValue("GLIDE")->load()-7.5f)<.01f,"state restore failed");
 verify(another.getCurrentProgram()==5,"program index not restored");
 {
  const auto expected=inverso_abismo::neon::presetValues(5);
  verify(std::abs(another.apvts.getRawParameterValue("RESONANCE")->load()-expected[inverso_abismo::neon::P::Resonance])<.1f,"preset values not restored");
 }

 // Editor must construct and lay out without a host.
 std::unique_ptr<juce::AudioProcessorEditor> editor(p.createEditor());
 verify(editor!=nullptr && editor->getWidth()>0,"editor failed to open");
 // Optional: ABISMO_SNAPSHOT=<file.png> saves a picture of the editor for review.
 if(const char* path=std::getenv("ABISMO_SNAPSHOT")){
  if(const char* program=std::getenv("ABISMO_SNAPSHOT_PROGRAM")){
   p.selectProgram(std::atoi(program));
   editor.reset(p.createEditor());
  }
  const auto image=editor->createComponentSnapshot(editor->getLocalBounds());
  juce::File file(juce::File::getCurrentWorkingDirectory().getChildFile(path));
  file.deleteFile();
  juce::FileOutputStream out(file);
  juce::PNGImageFormat().writeImageToStream(image,out);
 }
 editor.reset();
 std::puts("Neon JUCE host smoke passed");
}
