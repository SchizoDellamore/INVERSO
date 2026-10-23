#include "SamplerProcessor.h"
#include "SamplerEditor.h"
#include "AbismoUI.h"
#include "DSP/SamplerDefault.h"
#include <algorithm>
#include <cmath>

using namespace inverso_abismo::sampler;

namespace { constexpr double maxSampleSeconds=600.0; }

juce::AudioProcessorValueTreeState::ParameterLayout InversoAbismoSamplerProcessor::makeParameters() {
 return abismo_ui::makeLayout(specs().data(),P::Count);
}

InversoAbismoSamplerProcessor::InversoAbismoSamplerProcessor()
 : AudioProcessor(BusesProperties().withOutput("Output",juce::AudioChannelSet::stereo(),true)),
   apvts(*this,nullptr,"PARAMETERS",makeParameters()) {
 for (std::size_t i=0;i<raw.size();++i) raw[i]=apvts.getRawParameterValue(specs()[i].id);
 formats.registerBasicFormats();
 current=std::make_shared<SampleData>(makeDefaultSample(48000.0));
 display=current;
 engine.setSample(current.get());
 startTimer(500);
}

InversoAbismoSamplerProcessor::~InversoAbismoSamplerProcessor() { stopTimer(); }

bool InversoAbismoSamplerProcessor::isBusesLayoutSupported(const BusesLayout& layout) const {
 return layout.getMainOutputChannelSet()==juce::AudioChannelSet::stereo()
     && layout.getMainInputChannelSet().isDisabled();
}

void InversoAbismoSamplerProcessor::prepareToPlay(double sampleRate,int) {
 Values v{};
 for (std::size_t i=0;i<raw.size();++i) v[i]=raw[i]->load(std::memory_order_relaxed);
 engine.setValues(v);
 engine.prepare(sampleRate);
 prepared=true;
 outputPeak.store(0.f);
}

void InversoAbismoSamplerProcessor::releaseResources() { prepared=false; }

void InversoAbismoSamplerProcessor::processBlock(juce::AudioBuffer<float>& audio,juce::MidiBuffer& midi) {
 juce::ScopedNoDenormals noDenormals;
 const int samples=audio.getNumSamples();
 // Pick up a newly loaded sample without ever freeing memory on this thread:
 // the replaced one is parked in `retired` and released by the message thread.
 if (swapLock.tryEnter()) {
  if (pending && !retired) {
   retired=std::move(current);
   current=std::move(pending);
   engine.setSample(current.get());
  }
  swapLock.exit();
 }
 if (!prepared || audio.getNumChannels()<2) { audio.clear(); return; }
 Values v{};
 for (std::size_t i=0;i<raw.size();++i) v[i]=raw[i]->load(std::memory_order_relaxed);
 engine.setValues(v);
 if (auto* head=getPlayHead())
  if (const auto position=head->getPosition())
   engine.setTransport(position->getBpm().orFallback(120.0),position->getPpqPosition().orFallback(0.0),position->getIsPlaying());
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
 playhead.store(engine.playheadPosition(),std::memory_order_relaxed);
}

void InversoAbismoSamplerProcessor::queueSample(std::shared_ptr<SampleData> data,const juce::String& path) {
 display=data;
 {
  const juce::SpinLock::ScopedLockType lock(swapLock);
  pending=std::move(data); // an older, never-played pending sample is freed here, off the audio thread
 }
 if (!prepared) {
  // No audio callback running (e.g. session restore before playback): swap now.
  const juce::SpinLock::ScopedLockType lock(swapLock);
  if (pending) { retired=std::move(current); current=std::move(pending); engine.setSample(current.get()); }
 }
 {
  const juce::ScopedLock lock(pathLock);
  samplePath=path;
  missingPath.clear();
 }
 ++sampleVersion;
}

bool InversoAbismoSamplerProcessor::loadSampleFile(const juce::File& file) {
 std::unique_ptr<juce::AudioFormatReader> reader(formats.createReaderFor(file));
 if (!reader || reader->lengthInSamples<4 || reader->sampleRate<1000.0) return false;
 const auto frames=static_cast<int>(std::min<juce::int64>(reader->lengthInSamples,
                                    static_cast<juce::int64>(reader->sampleRate*maxSampleSeconds)));
 juce::AudioBuffer<float> buffer(static_cast<int>(std::min(2u,reader->numChannels)),frames);
 if (!reader->read(&buffer,0,frames,0,true,buffer.getNumChannels()>1)) return false;
 auto data=std::make_shared<SampleData>();
 data->sampleRate=reader->sampleRate;
 data->name=file.getFileName().toStdString();
 data->left.assign(buffer.getReadPointer(0),buffer.getReadPointer(0)+frames);
 if (buffer.getNumChannels()>1) data->right.assign(buffer.getReadPointer(1),buffer.getReadPointer(1)+frames);
 queueSample(std::move(data),file.getFullPathName());
 return true;
}

void InversoAbismoSamplerProcessor::useBuiltInSample() {
 queueSample(std::make_shared<SampleData>(makeDefaultSample(48000.0)),{});
}

void InversoAbismoSamplerProcessor::collectGarbage() {
 std::shared_ptr<SampleData> old;
 {
  const juce::SpinLock::ScopedLockType lock(swapLock);
  old=std::move(retired);
 }
 // `old` is destroyed here, on the calling (message) thread.
}

std::shared_ptr<const InversoAbismoSamplerProcessor::SampleData> InversoAbismoSamplerProcessor::getDisplaySample() const { return display; }
juce::String InversoAbismoSamplerProcessor::getSamplePath() const { const juce::ScopedLock lock(pathLock); return samplePath; }
juce::String InversoAbismoSamplerProcessor::getMissingPath() const { const juce::ScopedLock lock(pathLock); return missingPath; }
bool InversoAbismoSamplerProcessor::isAudioFile(const juce::String& path) const {
 return formats.findFormatForFileExtension(juce::File(path).getFileExtension())!=nullptr;
}

int InversoAbismoSamplerProcessor::getNumPrograms() { return static_cast<int>(presets().size()); }
const juce::String InversoAbismoSamplerProcessor::getProgramName(int index) {
 if (index<0 || index>=getNumPrograms()) return {};
 return juce::String::fromUTF8(presets()[static_cast<std::size_t>(index)].name);
}
void InversoAbismoSamplerProcessor::setCurrentProgram(int index) {
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
}

// The sample is stored by path, not embedded: sessions stay small, but moving
// or deleting the file means the built-in voice plays until it is reloaded.
void InversoAbismoSamplerProcessor::getStateInformation(juce::MemoryBlock& dest) {
 auto state=apvts.copyState();
 state.setProperty("program",currentProgram,nullptr);
 const auto missing=getMissingPath();
 state.setProperty("samplePath",missing.isNotEmpty()?missing:getSamplePath(),nullptr);
 if (auto xml=state.createXml()) copyXmlToBinary(*xml,dest);
}
void InversoAbismoSamplerProcessor::setStateInformation(const void* data,int size) {
 auto xml=getXmlFromBinary(data,size);
 if (!xml || !xml->hasTagName(apvts.state.getType())) return;
 auto state=juce::ValueTree::fromXml(*xml);
 currentProgram=juce::jlimit(0,getNumPrograms()-1,static_cast<int>(state.getProperty("program",0)));
 const juce::String path=state.getProperty("samplePath",juce::String()).toString();
 state.removeProperty("samplePath",nullptr);
 apvts.replaceState(state);
 if (path.isEmpty()) { if (getSamplePath().isNotEmpty()) useBuiltInSample(); return; }
 if (path==getSamplePath()) return;
 const juce::File file(path);
 if (!file.existsAsFile() || !loadSampleFile(file)) {
  const juce::ScopedLock lock(pathLock);
  missingPath=path;
  ++sampleVersion;
 }
}

juce::AudioProcessorEditor* InversoAbismoSamplerProcessor::createEditor(){return new InversoAbismoSamplerEditor(*this);}
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter(){return new InversoAbismoSamplerProcessor();}
