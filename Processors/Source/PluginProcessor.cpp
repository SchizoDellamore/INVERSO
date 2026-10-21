#include "PluginProcessor.h"
#include "PluginEditor.h"

juce::AudioProcessorValueTreeState::ParameterLayout InversoProcessor::makeParameters() {
 using namespace juce;
 std::vector<std::unique_ptr<RangedAudioParameter>> params;
 auto add=[&](const char* id,const char* label,float initial){
  params.push_back(std::make_unique<AudioParameterFloat>(ParameterID{id,1},label,NormalisableRange<float>(0.f,100.f,0.01f),initial));
 };
 add("AMOUNT","Amount",45.f);
 add("SENSITIVITY","Sensitivity",50.f);
 add("FOCUS","Focus",50.f);
 params.push_back(std::make_unique<AudioParameterBool>(ParameterID{"BYPASS",1},"Bypass",false));
 params.push_back(std::make_unique<AudioParameterBool>(ParameterID{"DELTA",1},"Audition removed",false));
 return {params.begin(),params.end()};
}
InversoProcessor::InversoProcessor():AudioProcessor(BusesProperties()
 .withInput("Input",juce::AudioChannelSet::stereo(),true)
 .withOutput("Output",juce::AudioChannelSet::stereo(),true)),apvts(*this,nullptr,"PARAMETERS",makeParameters()) {
 for(auto& x:bandReductionDb)x.store(0.f);
}
void InversoProcessor::prepareToPlay(double sampleRate,int) {
 dsp.prepare(sampleRate);
 outputPeak.store(0);reductionDb.store(0);
 for(auto& x:bandReductionDb)x.store(0.f);
 setLatencySamples(0);
}
bool InversoProcessor::isBusesLayoutSupported(const BusesLayout& layout)const {
 return layout.getMainInputChannelSet()==juce::AudioChannelSet::stereo() &&
        layout.getMainOutputChannelSet()==juce::AudioChannelSet::stereo();
}
void InversoProcessor::processBlock(juce::AudioBuffer<float>& audio,juce::MidiBuffer&) {
 juce::ScopedNoDenormals noDenormals;
 if(audio.getNumChannels()!=2 || audio.getNumSamples()<=0)return;
 inverso_proc::Parameters p;
 auto read=[this](const char* s) noexcept {return apvts.getRawParameterValue(s)->load(std::memory_order_relaxed);};
 p.amount=read("AMOUNT")*0.01f;
 p.sensitivity=read("SENSITIVITY")*0.01f;
 p.focus=read("FOCUS")*0.01f;
 p.bypass=read("BYPASS")>=0.5f;
 p.delta=read("DELTA")>=0.5f;
 dsp.setParameters(p);
 // DSP reads each input before writing its output; safe for in-place processing.
 float* left=audio.getWritePointer(0);
 float* right=audio.getWritePointer(1);
 const auto meter=dsp.process(left,right,left,right,audio.getNumSamples());
 outputPeak.store(meter.outputPeak,std::memory_order_relaxed);
 reductionDb.store(meter.reductionDb,std::memory_order_relaxed);
 for(int i=0;i<bandCount();++i)
  bandReductionDb[static_cast<std::size_t>(i)].store(meter.bandReductionDb[static_cast<std::size_t>(i)],std::memory_order_relaxed);
}
void InversoProcessor::getStateInformation(juce::MemoryBlock& out){
 auto xml=apvts.copyState().createXml();if(xml)copyXmlToBinary(*xml,out);
}
void InversoProcessor::setStateInformation(const void* data,int size){
 auto xml=getXmlFromBinary(data,size);
 if(xml && xml->hasTagName(apvts.state.getType()))apvts.replaceState(juce::ValueTree::fromXml(*xml));
}
juce::AudioProcessorEditor* InversoProcessor::createEditor(){return new InversoProcessorEditor(*this);}
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter(){return new InversoProcessor();}
