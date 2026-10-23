#include "../Source/EnkiProcessor.h"
#include <JuceHeader.h>
#include <cmath>
#include <cstdio>
#include <cstdlib>

static void verify(bool okay,const char* message){
 if(!okay){std::fprintf(stderr,"HOST SMOKE FAILED: %s\n",message);std::exit(1);}
}
static void param(InversoEnkiProcessor& p,const char* name,float value){
 auto* raw=dynamic_cast<juce::RangedAudioParameter*>(p.apvts.getParameter(name));
 verify(raw!=nullptr,"missing parameter");
 raw->setValueNotifyingHost(raw->convertTo0to1(value));
}
static float raw(InversoEnkiProcessor& p,const char* name){return p.apvts.getRawParameterValue(name)->load();}
// Plays a short chord across host block sizes; returns RMS.
static float play(InversoEnkiProcessor& p,int sr,int blockSize){
 double sum=0.0;long count=0;
 const int total=sr*2;
 for(int done=0;done<total;done+=blockSize){
  juce::AudioBuffer<float> block(2,blockSize);
  block.clear();
  juce::MidiBuffer midi;
  if(done==0) for(int n:{48,55,60}) midi.addEvent(juce::MidiMessage::noteOn(1,n,.9f),std::min(blockSize-1,7));
  if(done<=sr && done+blockSize>sr) for(int n:{48,55,60}) midi.addEvent(juce::MidiMessage::noteOff(1,n),sr-done);
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
 InversoEnkiProcessor p;
 juce::AudioProcessor::BusesLayout layout;
 layout.outputBuses.add(juce::AudioChannelSet::stereo());
 verify(p.setBusesLayout(layout),"stereo instrument layout rejected");
 verify(p.acceptsMidi(),"instrument must accept MIDI");
 p.drawFolder="-"; // recordings only: the test must not depend on the user's library
 for(int sr:{44100,48000,96000}){
  p.prepareToPlay(sr,64);
  verify(play(p,sr,256)>1e-4f,"the initial sound produced no audio");
  for(int category=0;category<5;++category){
   param(p,"CATEGORY",static_cast<float>(category));
   for(int k=0;k<3;++k){
    p.generate();
    for(int size:{1,64,2048}) verify(play(p,sr,size)>1e-5f,"a generated sound produced no audio");
   }
  }
  p.mutate();
  verify(play(p,sr,512)>1e-5f,"a mutated sound produced no audio");
  juce::MidiBuffer midi;
  midi.addEvent(juce::MidiMessage::allSoundOff(1),0);
  param(p,"REV_MIX",0.f); param(p,"ECHO_MIX",0.f);
  juce::AudioBuffer<float> block(2,sr);
  block.clear();
  p.processBlock(block,midi);
  midi.clear();
  block.clear();
  p.processBlock(block,midi);
  if(block.getMagnitude(0,sr)>=1e-5f) std::fprintf(stderr,"residual %g at sr %d, first %g last %g voices %d\n",block.getMagnitude(0,sr),sr,block.getMagnitude(0,0,256),block.getMagnitude(0,sr-256,256),p.voicesSounding.load());
  verify(block.getMagnitude(0,sr)<1e-5f,"all sound off must silence the instrument");
 }
 // History: back and forward return the same sounds.
 p.prepareToPlay(48000,512);
 verify(p.historySize()>=2,"history missing");
 const float cutoffNow=raw(p,"CUTOFF");
 verify(p.back(),"back failed");
 verify(p.forward(),"forward failed");
 verify(std::abs(raw(p,"CUTOFF")-cutoffNow)<.5f,"history did not restore the sound");
 // Locks: a locked section survives GENERATE.
 param(p,"LOCK_SPACE",1.f);
 param(p,"REV_MIX",42.f);
 for(int k=0;k<5;++k) p.generate();
 verify(std::abs(raw(p,"REV_MIX")-42.f)<.5f,"locked space changed");
 param(p,"LOCK_SPACE",0.f);
 // A recording from the library in a source.
 p.useLibrarySample(0,5);
 param(p,"A_TYPE",3.f);
 verify(play(p,48000,512)>1e-5f,"a library recording produced no audio");
 // Knob learn.
 {
  p.learnPressure.store(true);
  juce::MidiBuffer cc;
  cc.addEvent(juce::MidiMessage::controllerEvent(1,1,90),0);
  cc.addEvent(juce::MidiMessage::controllerEvent(1,21,100),2);
  juce::AudioBuffer<float> block(2,512);
  block.clear();
  p.processBlock(block,cc);
  verify(p.learnedCC.load()==21,"knob learn must pick CC 21");
  p.learnPressure.store(false);
 }
 // State round trip.
 param(p,"CUTOFF",1234.f);
 juce::MemoryBlock saved;
 p.getStateInformation(saved);
 verify(saved.getSize()>0,"empty plugin state");
 InversoEnkiProcessor another;
 another.setStateInformation(saved.getData(),static_cast<int>(saved.getSize()));
 verify(std::abs(raw(another,"CUTOFF")-1234.f)<1.f,"state restore failed");

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
 std::puts("Enki JUCE host smoke passed");
}
