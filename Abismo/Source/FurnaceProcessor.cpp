#include "FurnaceProcessor.h"
#include "StateRestore.h"
#include "FurnaceEditor.h"
#include "AbismoUI.h"
#include "DSP/AbismoCommon.h"
#include <cmath>

using namespace inverso_abismo::furnace;

namespace {
// The cabinets come loudness-normalised and quiet (headroom in the convolver);
// this brings them back to the amp's level. "Off" passes the amp straight
// through, a little lower, for an IR loader after FURNACE.
constexpr float cabMakeup = 4.f;
constexpr float noCabLevel = .3f;
const char* const cabParams[] = {"CAB","MIC","MIC_POSITION","MIC_AXIS","MIC_DISTANCE"};
}

juce::AudioProcessorValueTreeState::ParameterLayout InversoFurnaceProcessor::makeParameters() {
 return abismo_ui::makeLayout(specs().data(),P::Count);
}

InversoFurnaceProcessor::InversoFurnaceProcessor()
 : AudioProcessor(BusesProperties().withInput("Input",juce::AudioChannelSet::stereo(),true).withOutput("Output",juce::AudioChannelSet::stereo(),true)),
   juce::Thread("FURNACE cabinets"),
   apvts(*this,nullptr,"PARAMETERS",makeParameters()) {
 for (std::size_t i=0;i<raw.size();++i) raw[i]=apvts.getRawParameterValue(specs()[i].id);
 for (const char* id:cabParams) apvts.addParameterListener(id,this);
 startThread(juce::Thread::Priority::low);
}

InversoFurnaceProcessor::~InversoFurnaceProcessor() {
 for (const char* id:cabParams) apvts.removeParameterListener(id,this);
 signalThreadShouldExit();
 wake.signal();
 stopThread(4000);
}

bool InversoFurnaceProcessor::isBusesLayoutSupported(const BusesLayout& layout) const {
 const auto in=layout.getMainInputChannelSet(), out=layout.getMainOutputChannelSet();
 if (out!=juce::AudioChannelSet::mono() && out!=juce::AudioChannelSet::stereo()) return false;
 if (in!=juce::AudioChannelSet::mono() && in!=juce::AudioChannelSet::stereo()) return false;
 return !(in==juce::AudioChannelSet::stereo() && out==juce::AudioChannelSet::mono());
}

void InversoFurnaceProcessor::prepareToPlay(double sampleRate,int samplesPerBlock) {
 amp.prepare(sampleRate);
 Values v{};
 for (std::size_t i=0;i<raw.size();++i) v[i]=raw[i]->load(std::memory_order_relaxed);
 amp.setValues(v);
 mono.setSize(1,std::max(1,samplesPerBlock),false,true,false);
 diBuffer.setSize(1,std::max(1,samplesPerBlock),false,true,false);
 convBlock=std::max(1,samplesPerBlock);
 cab.prepare({sampleRate,static_cast<juce::uint32>(convBlock),1});
 cab.reset();
 setLatencySamples(Amp::latency+cab.getLatency());
 if (std::abs(sampleRateNow.load()-sampleRate)>.5 || cabLoaded.load()<0) { sampleRateNow=sampleRate; requestCab(); }
 prepared=true;
}

void InversoFurnaceProcessor::releaseResources() { prepared=false; }

void InversoFurnaceProcessor::parameterChanged(const juce::String&,float) { requestCab(); }
void InversoFurnaceProcessor::requestCab() { cabDirty=true; wake.signal(); }

double InversoFurnaceProcessor::cabResponseDb(int cabChoice,int mic,double position,double axis,double distance,double f) {
 if (cabChoice<=cabOff || cabChoice>=cabUser) return 0.0;
 return cabMagnitudeDb(f,cabChoice-1,mic,fromKnobs(position,axis,distance))+Breakup(speakers()[static_cast<std::size_t>(cabChoice-1)])(f);
}

// Cabinets are designed here, a moment after the knobs stop moving, and handed
// to the convolver, which crossfades to the new one.
void InversoFurnaceProcessor::run() {
 while (!threadShouldExit()) {
  wake.wait(200);
  if (threadShouldExit()) return;
  if (!cabDirty.load()) continue;
  wait(40); // let a turning knob settle
  if (!cabDirty.exchange(false)) continue;
  const int choice=juce::roundToInt(raw[P::Cab]->load());
  const double sr=sampleRateNow.load();
  if (choice==cabUser) {
   juce::File file;
   { const juce::ScopedLock l(userIRLock); file=userIR; }
   if (file.existsAsFile())
    cab.loadImpulseResponse(file,juce::dsp::Convolution::Stereo::no,juce::dsp::Convolution::Trim::yes,0,juce::dsp::Convolution::Normalise::yes);
   cabLoaded=choice;
   continue;
  }
  if (choice<=cabOff) { cabLoaded=choice; continue; }
  const auto ir=designCab(choice-1,juce::roundToInt(raw[P::Mic]->load()),
                          fromKnobs(raw[P::Position]->load()/100.0,raw[P::Axis]->load()/100.0,raw[P::Distance]->load()/100.0),sr);
  if (cabDirty.load()) continue; // already outdated
  juce::AudioBuffer<float> buffer(1,static_cast<int>(ir.size()));
  buffer.copyFrom(0,0,ir.data(),static_cast<int>(ir.size()));
  cab.loadImpulseResponse(std::move(buffer),sr,juce::dsp::Convolution::Stereo::no,juce::dsp::Convolution::Trim::no,juce::dsp::Convolution::Normalise::no);
  cabLoaded=choice;
 }
}

bool InversoFurnaceProcessor::loadUserIR(const juce::File& file) {
 if (!file.existsAsFile()) return false;
 { const juce::ScopedLock l(userIRLock); userIR=file; }
 if (auto* p=apvts.getParameter("CAB")) {
  p->beginChangeGesture();
  p->setValueNotifyingHost(p->convertTo0to1(static_cast<float>(cabUser)));
  p->endChangeGesture();
 }
 requestCab();
 return true;
}
juce::String InversoFurnaceProcessor::userIRName() const { const juce::ScopedLock l(userIRLock); return userIR.getFileNameWithoutExtension(); }

void InversoFurnaceProcessor::processBlock(juce::AudioBuffer<float>& audio,juce::MidiBuffer&) {
 juce::ScopedNoDenormals noDenormals;
 const int n=audio.getNumSamples();
 if (!prepared || n==0) { audio.clear(); return; }
 if (mono.getNumSamples()<n) { mono.setSize(1,n,false,false,true); diBuffer.setSize(1,n,false,false,true); }
 Values v{};
 for (std::size_t i=0;i<raw.size();++i) v[i]=raw[i]->load(std::memory_order_relaxed);
 amp.setValues(v);
 const int inputs=getTotalNumInputChannels();
 auto* m=mono.getWritePointer(0);
 float inPeak=0.f;
 for (int i=0;i<n;++i) {
  float x=0.f;
  for (int c=0;c<inputs;++c) x+=audio.getReadPointer(c)[i];
  m[i]=inputs>1?x/static_cast<float>(inputs):x;
  inPeak=std::max(inPeak,std::abs(m[i]));
 }
 auto* di=diBuffer.getWritePointer(0);
 amp.process(m,m,di,n);
 const int choice=juce::roundToInt(v[P::Cab]);
 const int loaded=cabLoaded.load();
 float level=noCabLevel;
 if (choice>cabOff && loaded>cabOff) {
  // the convolver keeps the previous cabinet until the new one is designed, then crossfades
  // in pieces no longer than the convolver was prepared for (hosts may send more)
  for (int done=0;done<n;done+=convBlock) {
   const int len=std::min(convBlock,n-done);
   float* channel=m+done;
   juce::dsp::AudioBlock<float> block(&channel,1,static_cast<size_t>(len));
   cab.process(juce::dsp::ProcessContextReplacing<float>(block));
  }
  level=loaded==cabUser?1.4f:cabMakeup; // JUCE normalises a loaded IR; this matches FURNACE's own cabinets
 } else if (choice>cabOff) {
  level=0.f; // the first cabinet is still being designed: a few milliseconds of silence beat a blast of raw amp
 }
 const float blend=v[P::DIBlend]*.01f;
 float outPeak=0.f;
 for (int i=0;i<n;++i) {
  const float y=inverso_abismo::softCeiling(m[i]*level*(1.f-blend)+di[i]*blend*juce::Decibels::decibelsToGain(v[P::Output]));
  m[i]=y;
  outPeak=std::max(outPeak,std::abs(y));
 }
 for (int c=0;c<audio.getNumChannels();++c) audio.copyFrom(c,0,mono,0,0,n);
 inputPeak.store(std::max(inPeak,inputPeak.load(std::memory_order_relaxed)*.9f),std::memory_order_relaxed);
 outputPeak.store(std::max(outPeak,outputPeak.load(std::memory_order_relaxed)*.9f),std::memory_order_relaxed);
 gateLevel.store(amp.gateReduction(),std::memory_order_relaxed);
}

int InversoFurnaceProcessor::getNumPrograms(){return static_cast<int>(presets().size());}
const juce::String InversoFurnaceProcessor::getProgramName(int index) {
 return index>=0 && index<getNumPrograms()?juce::String(presets()[static_cast<std::size_t>(index)].name):juce::String();
}
void InversoFurnaceProcessor::setCurrentProgram(int index) {
 if (index<0 || index>=getNumPrograms() || index==currentProgram) return;
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

void InversoFurnaceProcessor::getStateInformation(juce::MemoryBlock& dest) {
 auto state=apvts.copyState();
 state.setProperty("program",currentProgram,nullptr);
 { const juce::ScopedLock l(userIRLock); state.setProperty("userIR",userIR.getFullPathName(),nullptr); }
 if (auto xml=state.createXml()) copyXmlToBinary(*xml,dest);
}
void InversoFurnaceProcessor::setStateInformation(const void* data,int size) {
 auto xml=getXmlFromBinary(data,size);
 if (xml && xml->hasTagName(apvts.state.getType())) {
  auto state=juce::ValueTree::fromXml(*xml);
  currentProgram=juce::jlimit(0,getNumPrograms()-1,static_cast<int>(state.getProperty("program",0)));
  const juce::String path=state.getProperty("userIR","").toString();
  { const juce::ScopedLock l(userIRLock); userIR=path.isNotEmpty() && juce::File::isAbsolutePath(path)?juce::File(path):juce::File(); }
  apvts.replaceState(state);
  restoreEveryParameter(apvts);
  requestCab();
 }
}

juce::AudioProcessorEditor* InversoFurnaceProcessor::createEditor(){return new InversoFurnaceEditor(*this);}
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter(){return new InversoFurnaceProcessor();}
