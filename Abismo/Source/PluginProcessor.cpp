#include "PluginProcessor.h"
#include "StateRestore.h"
#include "PluginEditor.h"
#include <algorithm>
#include <cmath>

using namespace inverso_abismo;

juce::AudioProcessorValueTreeState::ParameterLayout InversoAbismoProcessor::makeParameters() {
 return abismo_ui::makeLayout(specs().data(),P::Count);
}

InversoAbismoProcessor::InversoAbismoProcessor()
 : AudioProcessor(BusesProperties().withOutput("Output",juce::AudioChannelSet::stereo(),true)),
   apvts(*this,nullptr,"PARAMETERS",makeParameters()) {
 for (std::size_t i=0;i<raw.size();++i) raw[i]=apvts.getRawParameterValue(specs()[i].id);
}

bool InversoAbismoProcessor::isBusesLayoutSupported(const BusesLayout& layout) const {
 return layout.getMainOutputChannelSet()==juce::AudioChannelSet::stereo()
     && layout.getMainInputChannelSet().isDisabled();
}

void InversoAbismoProcessor::prepareToPlay(double sampleRate,int) {
 Values v{};
 for (std::size_t i=0;i<raw.size();++i) v[i]=raw[i]->load(std::memory_order_relaxed);
 engine.setValues(v);
 engine.prepare(sampleRate);
 prepared=true;
 outputPeak.store(0.f);
}

void InversoAbismoProcessor::releaseResources() { prepared=false; }

void InversoAbismoProcessor::processBlock(juce::AudioBuffer<float>& audio,juce::MidiBuffer& midi) {
 juce::ScopedNoDenormals noDenormals;
 const int samples=audio.getNumSamples();
 if (!prepared || audio.getNumChannels()<2) { audio.clear(); return; }
 Values v{};
 for (std::size_t i=0;i<raw.size();++i) v[i]=raw[i]->load(std::memory_order_relaxed);
 engine.setValues(v);
 if (auto* head=getPlayHead()) {
  if (const auto position=head->getPosition()) {
   engine.setTransport(position->getBpm().orFallback(120.0),position->getPpqPosition().orFallback(0.0),position->getIsPlaying());
  }
 }
 auto* left=audio.getWritePointer(0);
 auto* right=audio.getWritePointer(1);
 // Split the block at each MIDI event so notes start sample-accurately.
 int position=0;
 for (const auto metadata : midi) {
  const int at=std::clamp(metadata.samplePosition,0,samples);
  if (at>position) { engine.render(left+position,right+position,at-position); position=at; }
  const auto m=metadata.getMessage();
  midiLearn.handle(m);
  if (m.isNoteOn()) engine.noteOn(m.getNoteNumber(),m.getFloatVelocity());
  else if (m.isNoteOff()) engine.noteOff(m.getNoteNumber());
  else if (m.isPitchWheel()) engine.setPitchBend(abismo_ui::pitchBendOf(m.getPitchWheelValue()));
  else if (m.isControllerOfType(1)) engine.setModWheel(static_cast<float>(m.getControllerValue())/127.f);
  else if (m.isSustainPedalOn()) engine.setSustain(true);
  else if (m.isSustainPedalOff()) engine.setSustain(false);
  else if (m.isChannelPressure()) engine.setPressure(static_cast<float>(m.getChannelPressureValue())/127.f);
  else if (m.isAftertouch()) engine.setPressure(static_cast<float>(m.getAfterTouchValue())/127.f);
  else if (m.isAllNotesOff()) engine.allNotesOff();
  else if (m.isAllSoundOff()) engine.allSoundOff();
 }
 if (position<samples) engine.render(left+position,right+position,samples-position);
 for (int c=2;c<audio.getNumChannels();++c) audio.clear(c,0,samples);
 float peak=0.f;
 for (int i=0;i<samples;++i) peak=std::max({peak,std::abs(left[i]),std::abs(right[i])});
 outputPeak.store(std::max(peak,outputPeak.load(std::memory_order_relaxed)*.9f),std::memory_order_relaxed);
 voicesSounding.store(engine.activeVoices(),std::memory_order_relaxed);
}

int InversoAbismoProcessor::getNumPrograms() { return static_cast<int>(presets().size()); }
const juce::String InversoAbismoProcessor::getProgramName(int index) {
 if (index<0 || index>=getNumPrograms()) return {};
 return juce::String::fromUTF8(presets()[static_cast<std::size_t>(index)].name);
}
// Hosts re-send the current program (Cubase does on load and while playing);
// re-applying it would wipe the user's edits, so only a different preset is applied.
void InversoAbismoProcessor::setCurrentProgram(int index) {
 if (index<0 || index>=getNumPrograms() || index==currentProgram) return;
 currentProgram=index;
 const auto values=presetValues(static_cast<std::size_t>(index));
 for (std::size_t i=0;i<values.size();++i) {
  if (i==static_cast<std::size_t>(P::PressureCC)) continue; // the learned knob belongs to the keyboard, not the preset
  if (auto* param=apvts.getParameter(specs()[i].id)) {
   param->beginChangeGesture();
   param->setValueNotifyingHost(param->convertTo0to1(values[i]));
   param->endChangeGesture();
  }
 }
 // Tell the host which program is now active; otherwise its program parameter
 // keeps the old preset and the host switches back to it.
 updateHostDisplay(ChangeDetails().withProgramChanged(true));
}

void InversoAbismoProcessor::getStateInformation(juce::MemoryBlock& dest) {
 auto state=apvts.copyState();
 midiLearn.save(state);
 state.setProperty("program",currentProgram,nullptr);
 if (auto xml=state.createXml()) copyXmlToBinary(*xml,dest);
}
void InversoAbismoProcessor::setStateInformation(const void* data,int size) {
 auto xml=getXmlFromBinary(data,size);
 if (xml && xml->hasTagName(apvts.state.getType())) {
  auto state=juce::ValueTree::fromXml(*xml);
  currentProgram=juce::jlimit(0,getNumPrograms()-1,static_cast<int>(state.getProperty("program",0)));
  apvts.replaceState(state);
  restoreEveryParameter(apvts);
  midiLearn.load(state);
 }
}

juce::AudioProcessorEditor* InversoAbismoProcessor::createEditor(){return new InversoAbismoEditor(*this);}
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter(){return new InversoAbismoProcessor();}
