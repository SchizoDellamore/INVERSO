#include "../Source/ChainProcessor.h"
#include "../Source/ChainEditor.h"
#include <JuceHeader.h>
#include "StateRoundTrip.h"
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <random>

static void verify(bool okay,const char* message){
 if(!okay){std::fprintf(stderr,"HOST SMOKE FAILED: %s\n",message);std::exit(1);}
}
int main(){
 juce::ScopedJuceInitialiser_GUI init;
 const char* name=inverso_abismo::chain::stripName(InversoChainProcessor::strip);
 { InversoChainProcessor rt; if (!stateRoundTrip(rt, name)) return 1; }
 InversoChainProcessor p;
 for(auto set:{juce::AudioChannelSet::mono(),juce::AudioChannelSet::stereo()}){
  juce::AudioProcessor::BusesLayout l; l.inputBuses.add(set); l.outputBuses.add(set);
  verify(p.checkBusesLayoutSupported(l),"layout rejected");
 }
 const int expectedLatency=InversoChainProcessor::strip==inverso_abismo::chain::Strip::Master?107:0;
 std::mt19937 rng(1);
 std::normal_distribution<float> g(0.f,.1f);
 for(double sr:{44100.0,48000.0,96000.0}){
  p.prepareToPlay(sr,512);
  if(expectedLatency==0) verify(p.getLatencySamples()==0,"a strip without a limiter has no latency");
  else { verify(p.getLatencySamples()>=60 && p.getLatencySamples()<=260,"limiter latency out of range"); if(sr==48000.0) verify(p.getLatencySamples()==expectedLatency,"latency at 48 kHz"); }
  for(int program=0;program<p.getNumPrograms();++program){
   p.selectProgram(program);
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
 p.selectProgram(p.getNumPrograms()-1);
 juce::MemoryBlock saved;
 p.getStateInformation(saved);
 InversoChainProcessor another;
 another.setStateInformation(saved.getData(),static_cast<int>(saved.getSize()));
 verify(another.getCurrentProgram()==p.getNumPrograms()-1,"program not restored");
 // a mono track
 { juce::AudioBuffer<float> b(1,512); b.clear(); juce::MidiBuffer m; p.prepareToPlay(48000,512); p.processBlock(b,m); }
 std::unique_ptr<juce::AudioProcessorEditor> editor(p.createEditor());
 verify(editor!=nullptr && editor->getWidth()>0,"editor failed to open");
 if(const char* path=std::getenv("ABISMO_SNAPSHOT")){
  p.prepareToPlay(48000,512);
  p.selectProgram(std::getenv("ABISMO_SNAPSHOT_PROGRAM")?std::atoi(std::getenv("ABISMO_SNAPSHOT_PROGRAM")):0);
  juce::MidiBuffer midi;
  for(int k=0;k<300;++k){
   juce::AudioBuffer<float> b(2,512);
   for(int i=0;i<512;++i){ const float t=static_cast<float>((k%24)*512+i); const float x=std::exp(-t/2500.f)*std::sin(t*.03f*(1.f+3.f*std::exp(-t/800.f)))*.9f+g(rng)*.2f; b.setSample(0,i,x); b.setSample(1,i,x*.9f); }
   p.processBlock(b,midi);
   if(k%10==0) dynamic_cast<InversoChainEditor*>(editor.get())->refresh();
  }
  for(int k=0;k<3;++k) dynamic_cast<InversoChainEditor*>(editor.get())->refresh();
  const auto image=editor->createComponentSnapshot(editor->getLocalBounds());
  juce::File file(juce::File::getCurrentWorkingDirectory().getChildFile(path));
  file.deleteFile();
  juce::FileOutputStream out(file);
  juce::PNGImageFormat().writeImageToStream(image,out);
 }
 editor.reset();
 std::printf("%s JUCE host smoke passed\n",name);
}
