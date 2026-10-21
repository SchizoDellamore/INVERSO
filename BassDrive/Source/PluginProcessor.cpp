#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <algorithm>
#include <cmath>

juce::AudioProcessorValueTreeState::ParameterLayout InversoBassDriveProcessor::makeParameters(){
 using namespace juce;
 std::vector<std::unique_ptr<RangedAudioParameter>> p;
 auto knob=[&](const char* id,const char* name,float low,float high,float interval,float initial){
  p.push_back(std::make_unique<AudioParameterFloat>(ParameterID{id,1},name,NormalisableRange<float>(low,high,interval),initial));
 };
 knob("DRIVE","Drive",0.f,100.f,.01f,25.f);
 knob("FUZZ","Fuzz",0.f,100.f,.01f,0.f);
 p.push_back(std::make_unique<AudioParameterFloat>(ParameterID{"CROSSOVER",1},"Crossover",NormalisableRange<float>(60.f,500.f,1.f,.5f),155.f));
 knob("TONE","Fuzz tone",0.f,100.f,.01f,65.f);
 knob("MIX","Mix",0.f,100.f,.01f,100.f);
 knob("OUTPUT","Output",-24.f,12.f,.01f,0.f);
 p.push_back(std::make_unique<AudioParameterChoice>(ParameterID{"FUZZ_MODE",1},"Fuzz type",StringArray{"Off","Vintage","Silicon","Gated"},0));
 p.push_back(std::make_unique<AudioParameterBool>(ParameterID{"LOW_SAFE",1},"Preserve sub-bass",true));
 return {p.begin(),p.end()};
}
InversoBassDriveProcessor::InversoBassDriveProcessor():
 AudioProcessor(BusesProperties()
  .withInput("Input",juce::AudioChannelSet::stereo(),true)
  .withOutput("Output",juce::AudioChannelSet::stereo(),true)),
 apvts(*this,nullptr,"PARAMETERS",makeParameters()){}

bool InversoBassDriveProcessor::isBusesLayoutSupported(const BusesLayout& l)const{
 const auto in=l.getMainInputChannelSet();
 return (in==juce::AudioChannelSet::mono() || in==juce::AudioChannelSet::stereo()) &&
        in==l.getMainOutputChannelSet();
}
void InversoBassDriveProcessor::prepareToPlay(double sampleRate,int blockSize){
 const int channels=std::clamp(getTotalNumInputChannels(),1,2);
 preparedChannels=channels;
 // Four-fold JUCE oversampling: 2 stages of half-band polyphase IIR filters.
 // The latency is padded to an integer host sample and reported to Cubase.
 oversampling=std::make_unique<juce::dsp::Oversampling<float>>(
   static_cast<size_t>(channels),2,
   juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR,true,true);
 oversampling->initProcessing(static_cast<size_t>(std::max(blockSize,32)));
 oversampling->reset();
 core.prepare(sampleRate*4.0,channels);
 setLatencySamples(juce::roundToInt(oversampling->getLatencyInSamples()));
 inputPeak.store(0.f);outputPeak.store(0.f);oversampledPeak.store(0.f);
}
void InversoBassDriveProcessor::releaseResources(){
 oversampling.reset();preparedChannels=0;
 setLatencySamples(0);
}
void InversoBassDriveProcessor::processBlock(juce::AudioBuffer<float>& audio,juce::MidiBuffer& midi){
 juce::ignoreUnused(midi);
 juce::ScopedNoDenormals noDenormals;
 const int channels=audio.getNumChannels();
 const int samples=audio.getNumSamples();
 if(samples<=0 || !oversampling || channels!=preparedChannels)return;
 auto param=[this](const char* id)noexcept{return apvts.getRawParameterValue(id)->load(std::memory_order_relaxed);};
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

 float hostInputPeak=0.f;
 for(int c=0;c<channels;++c){
  const float* channel=audio.getReadPointer(c);
  for(int i=0;i<samples;++i)hostInputPeak=std::max(hostInputPeak,std::abs(channel[i]));
 }
 juce::dsp::AudioBlock<float> block(audio);
 auto expanded=oversampling->processSamplesUp(block);
 const int expandedSamples=static_cast<int>(expanded.getNumSamples());
 float osPeak=0.f;
 for(int i=0;i<expandedSamples;++i){
  for(int c=0;c<channels;++c){
   auto* data=expanded.getChannelPointer(static_cast<size_t>(c));
   const float output=core.processChannel(c,data[i]);
   data[i]=output;
   osPeak=std::max(osPeak,std::abs(output));
  }
  core.tickFrame();
 }
 oversampling->processSamplesDown(block);
 float hostOutputPeak=0.f;
 for(int c=0;c<channels;++c){
  const float* channel=audio.getReadPointer(c);
  for(int i=0;i<samples;++i)hostOutputPeak=std::max(hostOutputPeak,std::abs(channel[i]));
 }
 inputPeak.store(hostInputPeak,std::memory_order_relaxed);
 outputPeak.store(hostOutputPeak,std::memory_order_relaxed);
 oversampledPeak.store(osPeak,std::memory_order_relaxed);
}
void InversoBassDriveProcessor::getStateInformation(juce::MemoryBlock& dest){
 auto xml=apvts.copyState().createXml();
 if(xml)copyXmlToBinary(*xml,dest);
}
void InversoBassDriveProcessor::setStateInformation(const void* data,int size){
 auto xml=getXmlFromBinary(data,size);
 if(xml && xml->hasTagName(apvts.state.getType()))
  apvts.replaceState(juce::ValueTree::fromXml(*xml));
}
juce::AudioProcessorEditor* InversoBassDriveProcessor::createEditor(){return new InversoBassDriveEditor(*this);}
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter(){return new InversoBassDriveProcessor();}
