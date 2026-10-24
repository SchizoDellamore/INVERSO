#include "HaloProcessor.h"
#include "StateRestore.h"
#include "HaloEditor.h"
#include "AbismoUI.h"
#include <cmath>

using namespace inverso_abismo::halo;

juce::AudioProcessorValueTreeState::ParameterLayout InversoHaloProcessor::makeParameters() {
 return abismo_ui::makeLayout(specs().data(),P::Count);
}

InversoHaloProcessor::InversoHaloProcessor()
 : AudioProcessor(BusesProperties().withInput("Input",juce::AudioChannelSet::stereo(),true).withOutput("Output",juce::AudioChannelSet::stereo(),true)),
   apvts(*this,nullptr,"PARAMETERS",makeParameters()) {
 for (std::size_t i=0;i<raw.size();++i) raw[i]=apvts.getRawParameterValue(specs()[i].id);
}

bool InversoHaloProcessor::isBusesLayoutSupported(const BusesLayout& layout) const {
 const auto in=layout.getMainInputChannelSet(), out=layout.getMainOutputChannelSet();
 return in==out && (out==juce::AudioChannelSet::mono() || out==juce::AudioChannelSet::stereo());
}

void InversoHaloProcessor::prepareToPlay(double sampleRate,int) {
 Values v{};
 for (std::size_t i=0;i<raw.size();++i) v[i]=raw[i]->load(std::memory_order_relaxed);
 engine.setValues(v);
 engine.prepare(sampleRate); // starts on the current settings, no glide from the defaults
 prepared=true;
}
void InversoHaloProcessor::releaseResources() { prepared=false; }

void InversoHaloProcessor::processBlock(juce::AudioBuffer<float>& audio,juce::MidiBuffer&) {
 juce::ScopedNoDenormals noDenormals;
 const int n=audio.getNumSamples();
 if (!prepared || n==0) return;
 Values v{};
 for (std::size_t i=0;i<raw.size();++i) v[i]=raw[i]->load(std::memory_order_relaxed);
 engine.setValues(v);
 float* left=audio.getWritePointer(0);
 float* right=audio.getNumChannels()>1?audio.getWritePointer(1):nullptr;
 engine.process(left,right,n);
 for (int c=2;c<audio.getNumChannels();++c) audio.clear(c,0,n);
 outputPeak.store(std::max(audio.getMagnitude(0,n),outputPeak.load(std::memory_order_relaxed)*.9f),std::memory_order_relaxed);
}

int InversoHaloProcessor::getNumPrograms(){return static_cast<int>(presets().size());}
const juce::String InversoHaloProcessor::getProgramName(int index) {
 return index>=0 && index<getNumPrograms()?juce::String(presets()[static_cast<std::size_t>(index)].name):juce::String();
}
// A program number from the host (re-sent on load or activation, possibly stale)
// is applied only while nothing was edited since the last preset; the preset bar always applies.
void InversoHaloProcessor::setCurrentProgram(int index) {
 if (index<0 || index>=getNumPrograms() || index==currentProgram) return;
 const auto expected=presetValues(static_cast<std::size_t>(currentProgram));
 for (std::size_t i=0;i<expected.size();++i) {
  const auto& spec=specs()[i];
  if (std::abs(raw[i]->load()-expected[i])>(spec.max-spec.min)*1e-4f) return;
 }
 selectProgram(index);
}
void InversoHaloProcessor::selectProgram(int index) {
 if (index<0 || index>=getNumPrograms()) return;
 currentProgram=index;
 const auto values=presetValues(static_cast<std::size_t>(index));
 for (std::size_t i=0;i<values.size();++i) {
  if (auto* param=apvts.getParameter(specs()[i].id)) {
   param->beginChangeGesture();
   param->setValueNotifyingHost(param->convertTo0to1(values[i]));
   param->endChangeGesture();
  }
 }
 updateHostDisplay(ChangeDetails().withProgramChanged(true));
}

void InversoHaloProcessor::getStateInformation(juce::MemoryBlock& dest) {
 auto state=apvts.copyState();
 state.setProperty("program",currentProgram,nullptr);
 if (auto xml=state.createXml()) copyXmlToBinary(*xml,dest);
}
void InversoHaloProcessor::setStateInformation(const void* data,int size) {
 auto xml=getXmlFromBinary(data,size);
 if (xml && xml->hasTagName(apvts.state.getType())) {
  auto state=juce::ValueTree::fromXml(*xml);
  currentProgram=juce::jlimit(0,getNumPrograms()-1,static_cast<int>(state.getProperty("program",0)));
  apvts.replaceState(state);
  restoreEveryParameter(apvts);
 }
}

juce::AudioProcessorEditor* InversoHaloProcessor::createEditor(){return new InversoHaloEditor(*this);}
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter(){return new InversoHaloProcessor();}
