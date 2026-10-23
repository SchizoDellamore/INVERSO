#include "../Source/PluginProcessor.h"
#include <JuceHeader.h>
#include <cmath>
#include <cstdio>
#include <cstdlib>

static void verify(bool okay,const char* message){
 if(!okay){std::fprintf(stderr,"BERIMBASS HOST SMOKE FAILED: %s\n",message);std::exit(1);}
}
static void param(InversoBerimbassProcessor& p,const char* name,float value){
 auto* raw=dynamic_cast<juce::RangedAudioParameter*>(p.apvts.getParameter(name));
 verify(raw!=nullptr,"missing parameter");
 raw->setValueNotifyingHost(raw->convertTo0to1(value));
}
// A bass line across host block sizes, with mod wheel and aftertouch moving.
static float line(InversoBerimbassProcessor& p,int sr,int blockSize){
 double sum=0.0;long count=0;
 const int step=sr/4;
 for(int done=0;done<sr*2;done+=blockSize){
  juce::AudioBuffer<float> block(2,blockSize);
  block.clear();
  juce::MidiBuffer midi;
  for(int i=0;i<blockSize;++i){
   const int t=done+i;
   if(t%step==0){
    const int n=28+(t/step*5)%12;
    midi.addEvent(juce::MidiMessage::noteOn(1,n,.9f),i);
    midi.addEvent(juce::MidiMessage::controllerEvent(1,1,(t/step*37)%128),i);
    midi.addEvent(juce::MidiMessage::channelPressureChange(1,(t/step*23)%128),i);
   }
   if(t%step==step-200) midi.addEvent(juce::MidiMessage::allNotesOff(1),i);
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

int main(){
 juce::ScopedJuceInitialiser_GUI init;
 InversoBerimbassProcessor p;
 juce::AudioProcessor::BusesLayout layout;
 layout.outputBuses.add(juce::AudioChannelSet::stereo());
 verify(p.setBusesLayout(layout),"stereo instrument layout rejected");
 verify(p.getNumPrograms()>=12,"presets missing");
 for(int sr:{44100,48000,96000}){
  p.prepareToPlay(sr,128);
  for(int program=0;program<p.getNumPrograms();++program){
   p.setCurrentProgram(program);
   for(int size:{1,64,1024}) verify(line(p,sr,size)>1e-3f,"bass line produced no audio");
  }
  juce::MidiBuffer midi;midi.addEvent(juce::MidiMessage::allSoundOff(1),0);
  param(p,"REV_MIX",0.f);
  juce::AudioBuffer<float> block(2,sr/2);block.clear();
  p.processBlock(block,midi);midi.clear();block.clear();p.processBlock(block,midi);
  verify(block.getMagnitude(0,sr/2)<1e-6f,"all sound off must silence");
 }
 verify(p.modWheel.load()>=0.f && p.pressure.load()>=0.f,"controller mirrors missing");
 p.setCurrentProgram(8);
 param(p,"GLIDE",333.f);
 juce::MemoryBlock saved;
 p.getStateInformation(saved);
 InversoBerimbassProcessor restored;
 restored.setStateInformation(saved.getData(),static_cast<int>(saved.getSize()));
 verify(std::lround(restored.apvts.getRawParameterValue("BODY")->load())==2,"preset body not restored");
 verify(std::abs(restored.apvts.getRawParameterValue("GLIDE")->load()-333.f)<1.f,"state restore failed");
 verify(restored.getCurrentProgram()==8,"program index not restored");
 p.setCurrentProgram(0);
 std::unique_ptr<juce::AudioProcessorEditor> editor(p.createEditor());
 verify(editor!=nullptr && editor->getWidth()>0,"editor failed to open");
 if(const char* path=std::getenv("BERIMBASS_SNAPSHOT")){
  const auto image=editor->createComponentSnapshot(editor->getLocalBounds());
  juce::File file(juce::File::getCurrentWorkingDirectory().getChildFile(path));
  file.deleteFile();
  juce::FileOutputStream out(file);
  juce::PNGImageFormat().writeImageToStream(image,out);
 }
 editor.reset();
 std::puts("Berimbass JUCE host smoke passed");
}
