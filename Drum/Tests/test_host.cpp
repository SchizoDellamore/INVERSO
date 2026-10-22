#include "../Source/PluginProcessor.h"
#include <JuceHeader.h>
#include <cmath>
#include <cstdio>
#include <cstdlib>
static void check(bool b,const char* msg){if(!b){std::fprintf(stderr,"HOST TEST FAILED: %s\n",msg);std::exit(1);}}
int main(){
 juce::ScopedJuceInitialiser_GUI init;
 InversoDrumProcessor p;
 for(int rate:{44100,48000,96000}){
  p.prepareToPlay(rate,32);
  for(int sz:{1,17,64,511,2048,8192}){
   juce::AudioBuffer<float> buffer(2,sz);juce::MidiBuffer midi;
   midi.addEvent(juce::MidiMessage::noteOn(10,36,static_cast<juce::uint8>(110)),0);
   p.processBlock(buffer,midi);
   double energy=0;
   for(int i=0;i<sz;++i){const auto l=buffer.getSample(0,i),r=buffer.getSample(1,i);
    check(std::isfinite(l)&&std::isfinite(r),"finite output");energy+=l*l+r*r;}
   check(energy>0,"MIDI note must create audio");
  }
  juce::MemoryBlock mem;p.getStateInformation(mem);check(mem.getSize()>10,"save state");
  InversoDrumProcessor restored;restored.setStateInformation(mem.getData(),static_cast<int>(mem.getSize()));
  check(restored.engine.get(0,0,0)==p.engine.get(0,0,0),"pattern state restored");
  p.releaseResources();
 }
 std::puts("Inverso Drum JUCE host smoke passed");
}
