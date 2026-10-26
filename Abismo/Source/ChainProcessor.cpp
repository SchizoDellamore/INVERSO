#include "ChainProcessor.h"
#include "StateRestore.h"
#include "ChainEditor.h"
#include "AbismoUI.h"
#include <cmath>

using namespace inverso_abismo::chain;

juce::AudioProcessorValueTreeState::ParameterLayout InversoChainProcessor::makeParameters() {
 const auto& l=layout(strip);
 return abismo_ui::makeLayout(l.specs.data(),static_cast<int>(l.specs.size()));
}

InversoChainProcessor::InversoChainProcessor()
 : AudioProcessor(BusesProperties().withInput("Input",juce::AudioChannelSet::stereo(),true).withOutput("Output",juce::AudioChannelSet::stereo(),true)),
   apvts(*this,nullptr,"PARAMETERS",makeParameters()) {
 for (const auto& sp:layout(strip).specs) raw.push_back(apvts.getRawParameterValue(sp.id));
}

bool InversoChainProcessor::isBusesLayoutSupported(const BusesLayout& layout) const {
 const auto in=layout.getMainInputChannelSet(), out=layout.getMainOutputChannelSet();
 return in==out && (out==juce::AudioChannelSet::mono() || out==juce::AudioChannelSet::stereo());
}

Values InversoChainProcessor::currentValues() const {
 const auto& l=layout(strip);
 Values v=l.defaults;
 for (std::size_t i=0;i<raw.size();++i) v[static_cast<std::size_t>(l.global[i])]=raw[i]->load(std::memory_order_relaxed);
 return v;
}

void InversoChainProcessor::prepareToPlay(double sampleRate,int) {
 engine.setValues(currentValues());
 engine.prepare(sampleRate); // starts on the current settings, no glide from the defaults
 setLatencySamples(engine.latency());
 prepared=true;
}
void InversoChainProcessor::releaseResources() { prepared=false; }

void InversoChainProcessor::processBlock(juce::AudioBuffer<float>& audio,juce::MidiBuffer&) {
 juce::ScopedNoDenormals noDenormals;
 const int n=audio.getNumSamples();
 if (!prepared || n==0) return;
 engine.setValues(currentValues());
 float* left=audio.getWritePointer(0);
 float* right=audio.getNumChannels()>1?audio.getWritePointer(1):nullptr;
 engine.process(left,right,n);
 for (int c=2;c<audio.getNumChannels();++c) audio.clear(c,0,n);
 float peak=audio.getMagnitude(0,n);
 if (audio.getNumChannels()>1) peak=std::max(peak,audio.getMagnitude(1,n));
 outputPeak.store(std::max(peak,outputPeak.load(std::memory_order_relaxed)*.9f),std::memory_order_relaxed);
}

int InversoChainProcessor::getNumPrograms(){return static_cast<int>(presets(strip).size());}
const juce::String InversoChainProcessor::getProgramName(int index) {
 return index>=0 && index<getNumPrograms()?juce::String(presets(strip)[static_cast<std::size_t>(index)].name):juce::String();
}
// A program number from the host (re-sent on load or activation, possibly stale)
// is applied only while nothing was edited since the last preset; the preset bar always applies.
void InversoChainProcessor::setCurrentProgram(int index) {
 if (index<0 || index>=getNumPrograms() || index==currentProgram) return;
 const auto expected=presetValues(strip,static_cast<std::size_t>(currentProgram));
 const auto& l=layout(strip);
 for (std::size_t i=0;i<raw.size();++i) {
  const auto& spec=l.specs[i];
  if (std::abs(raw[i]->load()-expected[static_cast<std::size_t>(l.global[i])])>(spec.max-spec.min)*1e-4f) return;
 }
 selectProgram(index);
}
void InversoChainProcessor::selectProgram(int index) {
 if (index<0 || index>=getNumPrograms()) return;
 currentProgram=index;
 const auto values=presetValues(strip,static_cast<std::size_t>(index));
 const auto& l=layout(strip);
 for (std::size_t i=0;i<l.specs.size();++i) {
  if (auto* param=apvts.getParameter(l.specs[i].id)) {
   param->beginChangeGesture();
   param->setValueNotifyingHost(param->convertTo0to1(values[static_cast<std::size_t>(l.global[i])]));
   param->endChangeGesture();
  }
 }
 updateHostDisplay(ChangeDetails().withProgramChanged(true));
}

void InversoChainProcessor::getStateInformation(juce::MemoryBlock& dest) {
 auto state=apvts.copyState();
 state.setProperty("program",currentProgram,nullptr);
 if (auto xml=state.createXml()) copyXmlToBinary(*xml,dest);
}
void InversoChainProcessor::setStateInformation(const void* data,int size) {
 auto xml=getXmlFromBinary(data,size);
 if (xml && xml->hasTagName(apvts.state.getType())) {
  auto state=juce::ValueTree::fromXml(*xml);
  currentProgram=juce::jlimit(0,getNumPrograms()-1,static_cast<int>(state.getProperty("program",0)));
  apvts.replaceState(state);
  restoreEveryParameter(apvts);
 }
}

juce::AudioProcessorEditor* InversoChainProcessor::createEditor(){return new InversoChainEditor(*this);}
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter(){return new InversoChainProcessor();}
