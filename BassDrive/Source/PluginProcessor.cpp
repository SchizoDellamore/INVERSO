#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <algorithm>
#include <cmath>

juce::AudioProcessorValueTreeState::ParameterLayout InversoBassDriveProcessor::makeParameters() {
 using namespace juce;
 std::vector<std::unique_ptr<RangedAudioParameter>> p;
 auto knob=[&](const char* id,const char* name,float low,float high,float interval,float initial){
  p.push_back(std::make_unique<AudioParameterFloat>(ParameterID{id,1},name,NormalisableRange<float>(low,high,interval),initial));
 };
 // IDs and parameter ranges are unchanged for session compatibility with 0.1.
 knob("DRIVE","Drive",0.f,100.f,.01f,25.f);
 knob("FUZZ","Fuzz",0.f,100.f,.01f,0.f);
 p.push_back(std::make_unique<AudioParameterFloat>(ParameterID{"CROSSOVER",1},"Crossover",NormalisableRange<float>(60.f,500.f,1.f,.5f),155.f));
 knob("TONE","Fuzz tone",0.f,100.f,.01f,65.f);
 knob("MIX","Mix",0.f,100.f,.01f,100.f);
 knob("OUTPUT","Output",-24.f,12.f,.01f,0.f);
 p.push_back(std::make_unique<AudioParameterChoice>(ParameterID{"FUZZ_MODE",1},"Fuzz type",StringArray{"Off","Vintage","Silicon","Gated"},0));
 p.push_back(std::make_unique<AudioParameterBool>(ParameterID{"LOW_SAFE",1},"Preserve sub-bass",true));
 p.push_back(std::make_unique<AudioParameterBool>(ParameterID{"BYPASS",1},"Bypass",false));
 return {p.begin(),p.end()};
}
InversoBassDriveProcessor::InversoBassDriveProcessor():
 AudioProcessor(BusesProperties()
  .withInput("Input",juce::AudioChannelSet::stereo(),true)
  .withOutput("Output",juce::AudioChannelSet::stereo(),true)),
 apvts(*this,nullptr,"PARAMETERS",makeParameters()){}

bool InversoBassDriveProcessor::isBusesLayoutSupported(const BusesLayout& l) const {
 const auto in=l.getMainInputChannelSet();
 return (in==juce::AudioChannelSet::mono() || in==juce::AudioChannelSet::stereo()) &&
        in==l.getMainOutputChannelSet();
}
void InversoBassDriveProcessor::prepareToPlay(double sampleRate,int /*blockSize*/) {
 const double validRate = (std::isfinite(sampleRate) && sampleRate>=8000.0) ? sampleRate : 48000.0;
 // Stabilisation release: host-rate DSP without JUCE oversampling or host latency.
 // This removes host-size-dependent oversampling buffers from the realtime path.
 // Reintroduce oversampling only after separate host validation.
 core.prepare(validRate,2);
 prepared=true;
 bypassWet=1.f;
 bypassStep=1.f-std::exp(-1.f/static_cast<float>(validRate*.012));
 setLatencySamples(0);
 inputPeak.store(0.f);outputPeak.store(0.f);oversampledPeak.store(0.f);
}
void InversoBassDriveProcessor::releaseResources(){
 prepared=false;
}
void InversoBassDriveProcessor::processBlock(juce::AudioBuffer<float>& audio,juce::MidiBuffer& midi) {
 juce::ignoreUnused(midi);
 juce::ScopedNoDenormals noDenormals;
 const int channels=audio.getNumChannels(),samples=audio.getNumSamples();
 // Fail-open: preserve input audio when not prepared or when host layout changes.
 if (!prepared || samples<=0 || channels<1 || channels>2) return;
 auto param=[this](const char* id) noexcept {return apvts.getRawParameterValue(id)->load(std::memory_order_relaxed);};
 inverso_bass::Parameters p;
 p.drive=param("DRIVE")*.01f;
 p.fuzz=param("FUZZ")*.01f;
 p.crossover=param("CROSSOVER");
 p.tone=param("TONE")*.01f;
 p.mix=param("MIX")*.01f;
 p.outputDb=param("OUTPUT");
 p.preserveLow=param("LOW_SAFE")>=.5f;
 p.mode=static_cast<inverso_bass::FuzzMode>(std::clamp(static_cast<int>(std::round(param("FUZZ_MODE"))),0,3));
 core.setParameters(p);
 const bool bypass=param("BYPASS")>=.5f;
 float inPeak=0.f,outPeak=0.f;
 // No allocation, no locks, no dynamically sized internal buffers in callback.
 auto* left=audio.getWritePointer(0);
 auto* right=channels==2?audio.getWritePointer(1):nullptr;
 for (int i=0;i<samples;++i) {
  const float dryL=left[i],dryR=right?right[i]:0.f;
  const float wetL=core.processChannel(0,std::isfinite(dryL)?dryL:0.f);
  const float wetR=right?core.processChannel(1,std::isfinite(dryR)?dryR:0.f):0.f;
  core.tickFrame();
  bypassWet+=bypassStep*((bypass?0.f:1.f)-bypassWet);
  const float outL=dryL+bypassWet*(wetL-dryL);
  const float outR=dryR+bypassWet*(wetR-dryR);
  // Never propagate invalid DSP results into the host or silently mute the buffer.
  left[i]=std::isfinite(outL)?outL:(std::isfinite(dryL)?dryL:0.f);
  if(right)right[i]=std::isfinite(outR)?outR:(std::isfinite(dryR)?dryR:0.f);
  inPeak=std::max(inPeak,std::abs(std::isfinite(dryL)?dryL:0.f));
  outPeak=std::max(outPeak,std::abs(left[i]));
  if(right){
   inPeak=std::max(inPeak,std::abs(std::isfinite(dryR)?dryR:0.f));
   outPeak=std::max(outPeak,std::abs(right[i]));
  }
 }
 inputPeak.store(inPeak,std::memory_order_relaxed);
 outputPeak.store(outPeak,std::memory_order_relaxed);
 // Kept for compatibility with the UI meter. No oversampled value in 0.2.
 oversampledPeak.store(outPeak,std::memory_order_relaxed);
}
void InversoBassDriveProcessor::getStateInformation(juce::MemoryBlock& dest) {
 auto xml=apvts.copyState().createXml();
 if(xml)copyXmlToBinary(*xml,dest);
}
void InversoBassDriveProcessor::setStateInformation(const void* data,int size) {
 auto xml=getXmlFromBinary(data,size);
 if(xml && xml->hasTagName(apvts.state.getType()))
  apvts.replaceState(juce::ValueTree::fromXml(*xml));
}
juce::AudioProcessorEditor* InversoBassDriveProcessor::createEditor(){return new InversoBassDriveEditor(*this);}
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter(){return new InversoBassDriveProcessor();}
