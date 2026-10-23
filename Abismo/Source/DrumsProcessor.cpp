#include "DrumsProcessor.h"
#include "DrumsEditor.h"
#include "AbismoUI.h"
#include <algorithm>
#include <cmath>

using namespace inverso_abismo::drums;

juce::AudioProcessorValueTreeState::ParameterLayout InversoAbismoDrumsProcessor::makeParameters() {
 return abismo_ui::makeLayout(specs().data(),P::Count);
}

InversoAbismoDrumsProcessor::InversoAbismoDrumsProcessor()
 : AudioProcessor(BusesProperties().withOutput("Output",juce::AudioChannelSet::stereo(),true)),
   apvts(*this,nullptr,"PARAMETERS",makeParameters()) {
 for (std::size_t i=0;i<raw.size();++i) raw[i]=apvts.getRawParameterValue(specs()[i].id);
}

bool InversoAbismoDrumsProcessor::isBusesLayoutSupported(const BusesLayout& layout) const {
 return layout.getMainOutputChannelSet()==juce::AudioChannelSet::stereo()
     && layout.getMainInputChannelSet().isDisabled();
}

void InversoAbismoDrumsProcessor::prepareToPlay(double sampleRate,int) {
 Values v{};
 for (std::size_t i=0;i<raw.size();++i) v[i]=raw[i]->load(std::memory_order_relaxed);
 engine.setValues(v);
 engine.prepare(sampleRate);
 prepared=true;
 outputPeak.store(0.f);
}

void InversoAbismoDrumsProcessor::releaseResources() { prepared=false; }

void InversoAbismoDrumsProcessor::processBlock(juce::AudioBuffer<float>& audio,juce::MidiBuffer& midi) {
 juce::ScopedNoDenormals noDenormals;
 const int samples=audio.getNumSamples();
 if (!prepared || audio.getNumChannels()<2) { audio.clear(); return; }
 Values v{};
 for (std::size_t i=0;i<raw.size();++i) v[i]=raw[i]->load(std::memory_order_relaxed);
 engine.setValues(v);
 if (auto* head=getPlayHead())
  if (const auto position=head->getPosition())
   engine.setTransport(position->getBpm().orFallback(120.0),position->getPpqPosition().orFallback(0.0),position->getIsPlaying());
 if (const auto pads=auditions.exchange(0))
  for (int pad=0;pad<numPads;++pad) if (pads & (1u<<pad)) engine.noteOn(firstPadNote+pad,.8f);
 auto* left=audio.getWritePointer(0);
 auto* right=audio.getWritePointer(1);
 int position=0;
 for (const auto metadata : midi) {
  const int at=std::clamp(metadata.samplePosition,0,samples);
  if (at>position) { engine.render(left+position,right+position,at-position); position=at; }
  const auto m=metadata.getMessage();
  if (m.isNoteOn()) engine.noteOn(m.getNoteNumber(),m.getFloatVelocity());
  else if (m.isNoteOff()) engine.noteOff(m.getNoteNumber());
  else if (m.isPitchWheel()) engine.setPitchBend((static_cast<float>(m.getPitchWheelValue())-8192.f)/8192.f);
  else if (m.isSustainPedalOn()) engine.setSustain(true);
  else if (m.isSustainPedalOff()) engine.setSustain(false);
  else if (m.isAllNotesOff()) engine.allNotesOff();
  else if (m.isAllSoundOff()) engine.allSoundOff();
 }
 if (position<samples) engine.render(left+position,right+position,samples-position);
 for (int c=2;c<audio.getNumChannels();++c) audio.clear(c,0,samples);
 float peak=0.f;
 for (int i=0;i<samples;++i) peak=std::max({peak,std::abs(left[i]),std::abs(right[i])});
 outputPeak.store(std::max(peak,outputPeak.load(std::memory_order_relaxed)*.9f),std::memory_order_relaxed);
 hitsSounding.store(engine.activeHits(),std::memory_order_relaxed);
 padLights.fetch_or(engine.takeTriggeredPads(),std::memory_order_relaxed);
}

int InversoAbismoDrumsProcessor::getNumPrograms() { return static_cast<int>(kits().size()); }
const juce::String InversoAbismoDrumsProcessor::getProgramName(int index) {
 if (index<0 || index>=getNumPrograms()) return {};
 return juce::String::fromUTF8(kits()[static_cast<std::size_t>(index)].name);
}
void InversoAbismoDrumsProcessor::setCurrentProgram(int index) {
 if (index<0 || index>=getNumPrograms()) return;
 currentProgram=index;
 const auto values=kitValues(static_cast<std::size_t>(index));
 for (std::size_t i=0;i<values.size();++i) {
  if (auto* param=apvts.getParameter(specs()[i].id)) {
   param->beginChangeGesture();
   param->setValueNotifyingHost(param->convertTo0to1(values[i]));
   param->endChangeGesture();
  }
 }
}

void InversoAbismoDrumsProcessor::getStateInformation(juce::MemoryBlock& dest) {
 auto state=apvts.copyState();
 state.setProperty("program",currentProgram,nullptr);
 if (auto xml=state.createXml()) copyXmlToBinary(*xml,dest);
}
void InversoAbismoDrumsProcessor::setStateInformation(const void* data,int size) {
 auto xml=getXmlFromBinary(data,size);
 if (xml && xml->hasTagName(apvts.state.getType())) {
  auto state=juce::ValueTree::fromXml(*xml);
  currentProgram=juce::jlimit(0,getNumPrograms()-1,static_cast<int>(state.getProperty("program",0)));
  apvts.replaceState(state);
 }
}

juce::AudioProcessorEditor* InversoAbismoDrumsProcessor::createEditor(){return new InversoAbismoDrumsEditor(*this);}
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter(){return new InversoAbismoDrumsProcessor();}
