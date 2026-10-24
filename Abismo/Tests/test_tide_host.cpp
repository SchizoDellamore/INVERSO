#include "../Source/TideProcessor.h"
#include "../Source/TideEditor.h"
#include <JuceHeader.h>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <random>

static void verify(bool okay,const char* message){
 if(!okay){std::fprintf(stderr,"HOST SMOKE FAILED: %s\n",message);std::exit(1);}
}
int main(){
 juce::ScopedJuceInitialiser_GUI init;
 InversoTideProcessor p;
 for(auto set:{juce::AudioChannelSet::mono(),juce::AudioChannelSet::stereo()}){
  juce::AudioProcessor::BusesLayout l; l.inputBuses.add(set); l.outputBuses.add(set);
  verify(p.checkBusesLayoutSupported(l),"layout rejected");
 }
 std::mt19937 rng(1);
 std::normal_distribution<float> g(0.f,.05f);
 for(double sr:{44100.0,48000.0,96000.0}){
  p.prepareToPlay(sr,512);
  verify(p.getLatencySamples()==0,"TIDE should have no latency");
  for(int program=0;program<p.getNumPrograms();++program){
   p.setCurrentProgram(program);
   for(int size:{1,100,512,2048}){
    if(size==1 && sr>50000) continue;
    juce::MidiBuffer midi;
    for(int k=0;k<static_cast<int>(sr*.3)/size+1;++k){
     juce::AudioBuffer<float> b(2,size);
     for(int c=0;c<2;++c) for(int i=0;i<size;++i) b.setSample(c,i,g(rng));
     p.processBlock(b,midi);
     for(int c=0;c<2;++c) for(int i=0;i<size;++i) verify(std::isfinite(b.getSample(c,i)) && std::abs(b.getSample(c,i))<4.f,"bad output");
    }
   }
  }
 }
 p.setCurrentProgram(3);
 juce::MemoryBlock saved;
 p.getStateInformation(saved);
 InversoTideProcessor another;
 another.setStateInformation(saved.getData(),static_cast<int>(saved.getSize()));
 verify(another.getCurrentProgram()==3,"program not restored");
 std::unique_ptr<juce::AudioProcessorEditor> editor(p.createEditor());
 verify(editor!=nullptr && editor->getWidth()>0,"editor failed to open");
 if(const char* path=std::getenv("ABISMO_SNAPSHOT")){
  // play something with a resonance first so the screen has content
  p.prepareToPlay(48000,512);
  p.setCurrentProgram(std::getenv("ABISMO_SNAPSHOT_PROGRAM")?std::atoi(std::getenv("ABISMO_SNAPSHOT_PROGRAM")):2);
  juce::MidiBuffer midi;
  double ph=0;
  for(int k=0;k<200;++k){
   juce::AudioBuffer<float> b(2,512);
   for(int i=0;i<512;++i){ ph+=2*3.14159265*2600/48000; const float x=(g(rng)*3.f+.25f*std::sin(static_cast<float>(ph)))*(k%40>=20?1.f:.15f); b.setSample(0,i,x); b.setSample(1,i,x); }
   p.processBlock(b,midi);
   if(k%10==0) dynamic_cast<InversoTideEditor*>(editor.get())->refresh();
  }
  for(int k=0;k<3;++k) dynamic_cast<InversoTideEditor*>(editor.get())->refresh();
  const auto image=editor->createComponentSnapshot(editor->getLocalBounds());
  juce::File file(juce::File::getCurrentWorkingDirectory().getChildFile(path));
  file.deleteFile();
  juce::FileOutputStream out(file);
  juce::PNGImageFormat().writeImageToStream(image,out);
 }
 editor.reset();
 std::puts("Tide JUCE host smoke passed");
}
