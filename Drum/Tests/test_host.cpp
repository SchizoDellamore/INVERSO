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
  p.engine.setLaneLength(0,128);p.engine.setDivision(0,2,2);p.engine.set(0,0,127,105);
  p.engine.sampleStart[0].store(.2f);p.engine.samplePitch[0].store(7.f);
  p.engine.drive[0].store(.5f);p.effects.delayFeel.store(1);
  juce::MemoryBlock extended;p.getStateInformation(extended);
  InversoDrumProcessor expanded;expanded.setStateInformation(extended.getData(),static_cast<int>(extended.getSize()));
  check(expanded.engine.get(0,0,127)==105,"128th step state restored");
  check(expanded.engine.laneLength[0].load()==128,"lane length state restored");
  check(expanded.engine.feel[0].load()==2,"triplet state restored");
  check(std::abs(expanded.engine.samplePitch[0].load()-7)<.01f,"sample pitch state restored");
  check(expanded.effects.delayFeel.load()==1,"effect division state restored");
  p.releaseResources();
 }
 std::puts("Inverso Drum JUCE host smoke passed");
}
