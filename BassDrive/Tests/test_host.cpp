#include "../Source/PluginProcessor.h"
#include <JuceHeader.h>
#include <cmath>
#include <cstdio>
#include <cstdlib>

static void verify(bool okay, const char* message) {
 if(!okay){std::fprintf(stderr,"HOST SMOKE FAILED: %s\n",message);std::exit(1);}
}
static void param(InversoBassDriveProcessor& p,const char* name,float value){
 auto* raw=dynamic_cast<juce::RangedAudioParameter*>(p.apvts.getParameter(name));
 verify(raw!=nullptr,"missing parameter");
 raw->setValueNotifyingHost(raw->convertTo0to1(value));
}
int main() {
 juce::ScopedJuceInitialiser_GUI init;
 InversoBassDriveProcessor p;
 for(int sr : {44100,48000,96000}){
  for(int channels : {1,2}) {
   juce::AudioProcessor::BusesLayout layout;
   layout.inputBuses.add(channels==1?juce::AudioChannelSet::mono():juce::AudioChannelSet::stereo());
   layout.outputBuses.add(layout.inputBuses[0]);
   verify(p.setBusesLayout(layout),"layout rejected");
   p.prepareToPlay(sr,32); // Intentionally underreports subsequent block sizes.
   verify(p.getLatencySamples()==0,"latency must be zero");
   for(int mode=0;mode<4;++mode){
    param(p,"DRIVE",65.f);
    param(p,"FUZZ_MODE",static_cast<float>(mode));
    param(p,"FUZZ",mode?65.f:0.f);
    param(p,"MIX",100.f);
    param(p,"BYPASS",0.f);
    float changed=0.f;
    for(int size : {1,16,32,64,256,1024,2048}) {
     juce::AudioBuffer<float> block(channels,size);
     for(int c=0;c<channels;++c)
      for(int i=0;i<size;++i)block.setSample(c,i,0.45f*std::sin(2.*juce::MathConstants<double>::pi*280.*i/sr));
     const float original=block.getSample(0,size/2);
     juce::MidiBuffer midi;
     p.processBlock(block,midi);
     for(int c=0;c<channels;++c)
      for(int i=0;i<size;++i)verify(std::isfinite(block.getSample(c,i)),"nonfinite audio output");
     changed+=std::abs(block.getSample(0,size/2)-original);
    }
    verify(changed>0.005f,"drive/fuzz did not change audio");
   }
   // In bypass, an extended block must pass the original through unaffected.
   param(p,"BYPASS",1.f);
   juce::AudioBuffer<float> block(channels,32768);
   for(int c=0;c<channels;++c) for(int i=0;i<32768;++i)block.setSample(c,i,.25f);
   juce::MidiBuffer midi;
   p.processBlock(block,midi);
   verify(std::abs(block.getSample(0,32767)-.25f)<.00001f,"bypass failed");
   // No prepare must pass through, never mute.
   p.releaseResources();
   block.clear();block.setSample(0,0,.9f);
   p.processBlock(block,midi);
   verify(block.getSample(0,0)==.9f,"unprepared callback altered input");
  }
 }
 // State save/load must preserve the same parameter format.
 p.prepareToPlay(48000,512);
 juce::MemoryBlock saved;
 p.getStateInformation(saved);
 verify(saved.getSize()>0,"empty plugin state");
 InversoBassDriveProcessor another;
 another.setStateInformation(saved.getData(),static_cast<int>(saved.getSize()));
 verify(std::abs(another.apvts.getRawParameterValue("DRIVE")->load()-65.f)<.1f,"state restore failed");
 std::puts("Bass Drive JUCE host smoke passed");
}
