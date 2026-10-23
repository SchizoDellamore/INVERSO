#include "../Source/RelicarioProcessor.h"
#include <JuceHeader.h>
#include <cmath>
#include <cstdio>
#include <cstdlib>

static void verify(bool okay,const char* message){
 if(!okay){std::fprintf(stderr,"HOST SMOKE FAILED: %s\n",message);std::exit(1);}
}
static void param(InversoRelicarioProcessor& p,const char* name,float value){
 auto* raw=dynamic_cast<juce::RangedAudioParameter*>(p.apvts.getParameter(name));
 verify(raw!=nullptr,"missing parameter");
 raw->setValueNotifyingHost(raw->convertTo0to1(value));
}
// Plays a note at a block-relative offset across host block sizes; returns RMS.
static float play(InversoRelicarioProcessor& p,int sr,int blockSize,int note){
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
 InversoRelicarioProcessor p;
 juce::AudioProcessor::BusesLayout layout;
 layout.outputBuses.add(juce::AudioChannelSet::stereo());
 verify(p.setBusesLayout(layout),"stereo instrument layout rejected");
 verify(p.acceptsMidi(),"instrument must accept MIDI");
 verify(p.getNumPrograms()>=20,"factory presets missing");
 for(int sr:{44100,48000,96000}){
  p.prepareToPlay(sr,64); // Hosts may send other block sizes than announced.
  for(int program=0;program<p.getNumPrograms();program+=3){
   p.setCurrentProgram(program);
   for(int size:{1,17,64,512,2048}){
    const float level=play(p,sr,size,57);
    verify(level>1e-4f,"note produced no audio");
   }
  }
  // Each machine on its own must sound.
  for(const char* machine:{"FITA_LEVEL","HARM_LEVEL","FORK_LEVEL","ORGAN_LEVEL"}){
   p.setCurrentProgram(1); p.setCurrentProgram(0);
   for(const char* other:{"FITA_LEVEL","HARM_LEVEL","FORK_LEVEL","ORGAN_LEVEL"}) param(p,other,0.f);
   param(p,machine,100.f);
   verify(play(p,sr,256,57)>1e-4f,"a machine on its own produced no audio");
  }
  // Silence without MIDI after everything has rung out.
  param(p,"REV_MIX",0.f);
  param(p,"ECHO_MIX",0.f);
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
 p.setCurrentProgram(0);
 juce::MidiBuffer midi;
 midi.addEvent(juce::MidiMessage::controllerEvent(1,64,127),0);
 midi.addEvent(juce::MidiMessage::noteOn(1,60,1.f),10);
 midi.addEvent(juce::MidiMessage::noteOff(1,60),100);
 juce::AudioBuffer<float> block(2,512);
 block.clear();
 p.processBlock(block,midi);
 midi.clear();
 for(int i=0;i<200;++i){param(p,"FITA_TONE",static_cast<float>(i%100));block.clear();p.processBlock(block,midi);}
 verify(p.voicesSounding.load()==1,"sustain pedal must hold the voice");
 midi.addEvent(juce::MidiMessage::controllerEvent(1,64,0),0);
 for(int i=0;i<400;++i){block.clear();p.processBlock(block,midi);midi.clear();}
 verify(p.voicesSounding.load()==0,"pedal up must release the voice");

 // A knob learned as aftertouch: the first CC seen while armed is reported (not mod wheel or sustain).
 {
  p.learnPressure.store(true);
  juce::MidiBuffer cc;
  cc.addEvent(juce::MidiMessage::controllerEvent(1,1,90),0);
  cc.addEvent(juce::MidiMessage::controllerEvent(1,64,127),1);
  cc.addEvent(juce::MidiMessage::controllerEvent(1,21,100),2);
  cc.addEvent(juce::MidiMessage::controllerEvent(1,64,0),3);
  block.clear();
  p.processBlock(block,cc);
  verify(p.learnedCC.load()==21,"knob learn must pick CC 21");
  p.learnPressure.store(false);
  param(p,"PRESSURE_CC",21.f);
  verify(std::abs(p.apvts.getRawParameterValue("PRESSURE_CC")->load()-21.f)<.5f,"pressure CC not stored");
  // Full pressure from the knob fills the bellows above its resting level.
  p.setCurrentProgram(7);
  juce::MidiBuffer notes;
  notes.addEvent(juce::MidiMessage::noteOn(1,60,.9f),0);
  notes.addEvent(juce::MidiMessage::controllerEvent(1,21,127),1);
  for(int i=0;i<200;++i){block.clear();p.processBlock(block,notes);notes.clear();}
  verify(p.bellows.load()>1.05f,"knob did not act as aftertouch");
  juce::MidiBuffer off; off.addEvent(juce::MidiMessage::allNotesOff(1),0); off.addEvent(juce::MidiMessage::controllerEvent(1,21,0),0);
  block.clear(); p.processBlock(block,off);
  param(p,"PRESSURE_CC",0.f);
 }
 // Program change and state round trip.
 p.setCurrentProgram(5);
 param(p,"LOOP_LENGTH",7.5f);
 juce::MemoryBlock saved;
 p.getStateInformation(saved);
 verify(saved.getSize()>0,"empty plugin state");
 InversoRelicarioProcessor another;
 another.setStateInformation(saved.getData(),static_cast<int>(saved.getSize()));
 verify(std::abs(another.apvts.getRawParameterValue("LOOP_LENGTH")->load()-7.5f)<.01f,"state restore failed");
 verify(another.getCurrentProgram()==5,"program index not restored");
 {
  const auto expected=inverso_abismo::relicario::presetValues(5);
  verify(std::abs(another.apvts.getRawParameterValue("WEAR")->load()-expected[inverso_abismo::relicario::P::Wear])<.1f,"preset values not restored");
 }

 // Editor must construct and lay out without a host.
 std::unique_ptr<juce::AudioProcessorEditor> editor(p.createEditor());
 verify(editor!=nullptr && editor->getWidth()>0,"editor failed to open");
 // Optional: ABISMO_SNAPSHOT=<file.png> saves a picture of the editor for review.
 if(const char* path=std::getenv("ABISMO_SNAPSHOT")){
  if(const char* program=std::getenv("ABISMO_SNAPSHOT_PROGRAM")){
   p.setCurrentProgram(std::atoi(program));
   editor.reset(p.createEditor());
  }
  const auto image=editor->createComponentSnapshot(editor->getLocalBounds());
  juce::File file(juce::File::getCurrentWorkingDirectory().getChildFile(path));
  file.deleteFile();
  juce::FileOutputStream out(file);
  juce::PNGImageFormat().writeImageToStream(image,out);
 }
 editor.reset();
 std::puts("Relicario JUCE host smoke passed");
}
