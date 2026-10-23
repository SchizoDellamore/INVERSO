#include "../Source/PluginProcessor.h"
#include <JuceHeader.h>
#include <cmath>
#include <cstdio>
#include <cstdlib>

static void verify(bool okay,const char* message){
 if(!okay){std::fprintf(stderr,"HOST SMOKE FAILED: %s\n",message);std::exit(1);}
}
static void param(InversoAbismoProcessor& p,const char* name,float value){
 auto* raw=dynamic_cast<juce::RangedAudioParameter*>(p.apvts.getParameter(name));
 verify(raw!=nullptr,"missing parameter");
 raw->setValueNotifyingHost(raw->convertTo0to1(value));
}
// Plays a note at a block-relative offset across host block sizes; returns RMS.
static float play(InversoAbismoProcessor& p,int sr,int blockSize,int note){
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
 InversoAbismoProcessor p;
 juce::AudioProcessor::BusesLayout layout;
 layout.outputBuses.add(juce::AudioChannelSet::stereo());
 verify(p.setBusesLayout(layout),"stereo instrument layout rejected");
 verify(p.acceptsMidi(),"instrument must accept MIDI");
 verify(p.getNumPrograms()>=10,"factory presets missing");
 for(int sr:{44100,48000,96000}){
  p.prepareToPlay(sr,64); // Hosts may send other block sizes than announced.
  for(int program=0;program<p.getNumPrograms();program+=3){
   p.setCurrentProgram(program);
   for(int size:{1,17,64,512,2048}){
    const float level=play(p,sr,size,57);
    verify(level>1e-4f,"note produced no audio");
   }
  }
  // Silence without MIDI after everything has rung out.
  param(p,"REV_MIX",0.f);
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
 for(int i=0;i<200;++i){param(p,"CUTOFF",200.f+static_cast<float>(i)*90.f);block.clear();p.processBlock(block,midi);}
 verify(p.voicesSounding.load()==1,"sustain pedal must hold the voice");
 midi.addEvent(juce::MidiMessage::controllerEvent(1,64,0),0);
 for(int i=0;i<400;++i){block.clear();p.processBlock(block,midi);midi.clear();}
 verify(p.voicesSounding.load()==0,"pedal up must release the voice");

 // Program change and state round trip.
 p.setCurrentProgram(5);
 param(p,"CUTOFF",777.f);
 juce::MemoryBlock saved;
 p.getStateInformation(saved);
 verify(saved.getSize()>0,"empty plugin state");
 InversoAbismoProcessor another;
 another.setStateInformation(saved.getData(),static_cast<int>(saved.getSize()));
 verify(std::abs(another.apvts.getRawParameterValue("CUTOFF")->load()-777.f)<1.f,"state restore failed");
 verify(another.getCurrentProgram()==5,"program index not restored");
 verify(std::abs(another.apvts.getRawParameterValue("SUB_LEVEL")->load()-70.f)<.1f,"preset values not restored");

 // Editor must construct and lay out without a host.
 std::unique_ptr<juce::AudioProcessorEditor> editor(p.createEditor());
 verify(editor!=nullptr && editor->getWidth()>0,"editor failed to open");
 // Optional: ABISMO_SNAPSHOT=<file.png> saves a picture of the editor for review.
 if(const char* path=std::getenv("ABISMO_SNAPSHOT")){
  const auto image=editor->createComponentSnapshot(editor->getLocalBounds());
  juce::File file(juce::File::getCurrentWorkingDirectory().getChildFile(path));
  file.deleteFile();
  juce::FileOutputStream out(file);
  juce::PNGImageFormat().writeImageToStream(image,out);
 }
 editor.reset();
 std::puts("Abismo JUCE host smoke passed");
}
