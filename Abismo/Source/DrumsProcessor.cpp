#include "DrumsProcessor.h"
#include "StateRestore.h"
#include "DrumsEditor.h"
#include "AbismoUI.h"
#include <algorithm>
#include <cmath>

using namespace inverso_abismo::drums;

namespace { constexpr double maxPadSeconds=20.0; constexpr int maxPadFiles=16; }

// The recordings live in the plugin bundle (Contents/Resources/drums.bin), next to
// the binary in Contents/x86_64-win. ABISMO_DRUMS_LIBRARY overrides the location.
juce::File InversoAbismoDrumsProcessor::libraryFile() {
 juce::Array<juce::File> candidates;
 if (const auto custom=juce::SystemStats::getEnvironmentVariable("ABISMO_DRUMS_LIBRARY",{}); custom.isNotEmpty())
  candidates.add(juce::File(custom));
 const auto binary=juce::File::getSpecialLocation(juce::File::currentExecutableFile);
 candidates.add(binary.getParentDirectory().getParentDirectory().getChildFile("Resources").getChildFile("drums.bin"));
 candidates.add(binary.getParentDirectory().getChildFile("drums.bin"));
#ifdef ABISMO_DRUMS_DEV_LIBRARY
 candidates.add(juce::File(juce::String::fromUTF8(ABISMO_DRUMS_DEV_LIBRARY)));
#endif
 for (const auto& f : candidates) if (f.existsAsFile()) return f;
 return {};
}

const SampleLibrary& InversoAbismoDrumsProcessor::library() {
 // Read once and shared by every instance of the plugin. Empty when the file is
 // missing or damaged: pads then fall back to the synthesised objects.
 static const SampleLibrary lib=[]{
  SampleLibrary l;
  juce::MemoryBlock bytes;
  const auto file=libraryFile();
  if (file==juce::File() || !file.loadFileAsData(bytes)
      || !SampleLibrary::parse(static_cast<const unsigned char*>(bytes.getData()),bytes.getSize(),l)
      || static_cast<int>(l.objects.size())!=numSampleObjects)
   l.objects.clear();
  l.equaliseLoudness(); // every recording at the same perceived level
  return l;
 }();
 return lib;
}

juce::AudioProcessorValueTreeState::ParameterLayout InversoAbismoDrumsProcessor::makeParameters() {
 return abismo_ui::makeLayout(specs().data(),P::Count);
}

InversoAbismoDrumsProcessor::InversoAbismoDrumsProcessor()
 : AudioProcessor(BusesProperties().withOutput("Output",juce::AudioChannelSet::stereo(),true)),
   apvts(*this,nullptr,"PARAMETERS",makeParameters()) {
 for (std::size_t i=0;i<raw.size();++i) raw[i]=apvts.getRawParameterValue(specs()[i].id);
 formats.registerBasicFormats();
 if (!library().objects.empty()) engine.setLibrary(&library());
 applyKit(0); // the plugin opens on the first kit, matching the program the host sees
 playedGeneration=kitGeneration.load(); // ...already playing it: no fade on the first block
 startTimer(500);
}

InversoAbismoDrumsProcessor::~InversoAbismoDrumsProcessor() { stopTimer(); }

bool InversoAbismoDrumsProcessor::isBusesLayoutSupported(const BusesLayout& layout) const {
 return layout.getMainOutputChannelSet()==juce::AudioChannelSet::stereo()
     && layout.getMainInputChannelSet().isDisabled();
}

void InversoAbismoDrumsProcessor::prepareToPlay(double sampleRate,int) {
 Values v{};
 for (std::size_t i=0;i<raw.size();++i) v[i]=raw[i]->load(std::memory_order_relaxed);
 engine.setValues(v);
 engine.prepare(sampleRate);
 deferredMidi.ensureSize(deferredReserve); // notes held during a kit change stay held across a re-prepare
 prepared=true;
 outputPeak.store(0.f);
}

void InversoAbismoDrumsProcessor::releaseResources() { prepared=false; }

void InversoAbismoDrumsProcessor::processBlock(juce::AudioBuffer<float>& audio,juce::MidiBuffer& midi) {
 juce::ScopedNoDenormals noDenormals;
 const int samples=audio.getNumSamples();
 // A kit change is a sequence, owned by this thread (kitStage). Each kit written bumps
 // kitGeneration; this thread starts a change when it sees a generation it has not played yet.
 //  Playing  the kit plays. While a change is being written (kitBusy: values, pad recordings
 //           queued) the old kit plays on untouched; once written (a new generation), one block
 //           fades it out and the stage becomes Faded.
 //  Faded    silence; notes wait. The new kit starts once it is fully written (no kitBusy) and
 //           its pad recordings are swapped in: then Playing again, with that generation.
 // Pad recordings swap without freeing memory here: the replaced ones are parked in `retired`
 // and released by the message thread. Outside a kit change they swap at once; during one, only
 // when the new kit starts. kitBusy and kitGeneration are read under the lock: kitBusy is raised
 // before anything is queued, and the generation bumped before it is lowered.
 bool kitBeingWritten=true, swapped=false;
 std::uint32_t generation=playedGeneration;
 if (swapLock.tryEnter()) {
  kitBeingWritten=kitBusy.load()>0;
  generation=kitGeneration.load();
  if (!kitBeingWritten && (kitStage==KitStage::Faded || generation==playedGeneration)) {
   for (int pad=0;pad<numPads;++pad) {
    auto& slot=slots[static_cast<std::size_t>(pad)];
    if (slot.hasPending && !slot.retired) {
     slot.retired=std::move(slot.current);
     slot.current=std::move(slot.pending);
     slot.hasPending=false;
     engine.setPadSample(pad,slot.current.get());
    }
   }
   swapped=true;
  }
  swapLock.exit();
 }
 if (!prepared || audio.getNumChannels()<2) { audio.clear(); return; }
 Values v{};
 for (std::size_t i=0;i<raw.size();++i) v[i]=raw[i]->load(std::memory_order_relaxed);
 juce::MidiBuffer* events=&midi; // what this block plays
 bool fadeThisBlock=false;
 if (kitStage==KitStage::Faded) {
  if (!swapped) { // the new kit is not ready (being written, or the lock was busy): stay silent
   deferNotes(midi);
   audio.clear();
   outputPeak.store(outputPeak.load(std::memory_order_relaxed)*.9f,std::memory_order_relaxed);
   hitsSounding.store(0,std::memory_order_relaxed);
   return;
  }
  playedGeneration=generation;
  engine.switchKit(v);
  kitStage=KitStage::Playing;
  // Notes held back start this block, on the new kit (played from deferredMidi, which keeps its storage).
  if (deferredCount>0) { deferredMidi.addEvents(midi,0,-1,0); events=&deferredMidi; }
 } else if (!kitBeingWritten && generation!=playedGeneration) {
  fadeThisBlock=true;
  deferNotes(midi);
 } else if (!kitBeingWritten) engine.setValues(v); // while a kit is written, the old one keeps its values
 if (auto* head=getPlayHead())
  if (const auto position=head->getPosition())
   engine.setTransport(position->getBpm().orFallback(120.0),position->getPpqPosition().orFallback(0.0),position->getIsPlaying());
 if (const auto pads=fadeThisBlock?0u:auditions.exchange(0))
  for (int pad=0;pad<numPads;++pad) if (pads & (1u<<pad)) engine.noteOn(firstPadNote+pad,.8f);
 auto* left=audio.getWritePointer(0);
 auto* right=audio.getWritePointer(1);
 int position=0;
 for (const auto metadata : *events) {
  const int at=std::clamp(metadata.samplePosition,0,samples);
  if (at>position) { engine.render(left+position,right+position,at-position); position=at; }
  const auto m=metadata.getMessage();
  midiLearn.handle(m);
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
 if (fadeThisBlock) {
  // The old kit fades out over this block; the new kit starts once it is ready.
  for (int i=0;i<samples;++i) { const float g=1.f-static_cast<float>(i+1)/static_cast<float>(samples); left[i]*=g; right[i]*=g; }
  kitStage=KitStage::Faded;
 }
 if (events==&deferredMidi) { deferredMidi.clear(); deferredCount=0; deferredBytes=0; }
 float peak=0.f;
 for (int i=0;i<samples;++i) peak=std::max({peak,std::abs(left[i]),std::abs(right[i])});
 outputPeak.store(std::max(peak,outputPeak.load(std::memory_order_relaxed)*.9f),std::memory_order_relaxed);
 hitsSounding.store(engine.activeHits(),std::memory_order_relaxed);
 padLights.fetch_or(engine.takeTriggeredPads(),std::memory_order_relaxed);
}

// Notes arriving during a kit change are held for the new kit, within the storage reserved in
// prepareToPlay (no allocation on the audio thread). Past the budget, new notes and controllers
// are dropped, but what releases something (note-off, sustain off, all notes / sound off) is
// kept, in a reserve of its own.
void InversoAbismoDrumsProcessor::deferNotes(juce::MidiBuffer& midi) {
 for (const auto metadata : midi) {
  const auto m=metadata.getMessage();
  const int bytes=m.getRawDataSize()+8;
  const bool releases=m.isNoteOff() || m.isSustainPedalOff() || m.isAllNotesOff() || m.isAllSoundOff();
  if (deferredBytes+bytes > (releases?deferredReserve:deferredReserve-releaseReserve)) continue;
  deferredMidi.addEvent(m,0);
  deferredBytes+=bytes;
  ++deferredCount;
 }
 midi.clear();
}

void InversoAbismoDrumsProcessor::queuePad(int pad,std::shared_ptr<SampleSet> set,const juce::StringArray& paths) {
 auto& slot=slots[static_cast<std::size_t>(pad)];
 std::shared_ptr<const SampleSet> display=set;
 {
  const juce::SpinLock::ScopedLockType lock(swapLock);
  slot.pending=std::move(set); // an older, never-played pending set is freed here, off the audio thread
  slot.hasPending=true;
  if (!prepared) {
   // No audio callback running (e.g. session restore before playback): swap now.
   slot.retired=std::move(slot.current);
   slot.current=std::move(slot.pending);
   slot.hasPending=false;
   engine.setPadSample(pad,slot.current.get());
  }
 }
 {
  const juce::ScopedLock lock(pathLock);
  slot.paths=paths;
  slot.missing.clear();
  slot.display=std::move(display);
 }
 ++sampleVersion;
}

bool InversoAbismoDrumsProcessor::loadPadFiles(int pad,const juce::Array<juce::File>& files) {
 if (pad<0 || pad>=numPads) return false;
 auto set=std::make_shared<SampleSet>();
 juce::StringArray paths;
 std::vector<std::pair<float,std::vector<float>>> takes;
 double rate=0.0;
 for (const auto& file : files) {
  if (static_cast<int>(takes.size())>=maxPadFiles) break;
  std::unique_ptr<juce::AudioFormatReader> reader(formats.createReaderFor(file));
  if (!reader || reader->lengthInSamples<4 || reader->sampleRate<1000.0) continue;
  // Every take of a pad plays at one rate: the first file's.
  juce::AudioBuffer<float> buffer;
  const double sourceRate=reader->sampleRate;
  if (rate<=0.0) rate=sourceRate;
  const auto frames=static_cast<int>(std::min<juce::int64>(reader->lengthInSamples,static_cast<juce::int64>(sourceRate*maxPadSeconds)));
  buffer.setSize(static_cast<int>(std::min(2u,reader->numChannels)),frames);
  if (!reader->read(&buffer,0,frames,0,true,buffer.getNumChannels()>1)) continue;
  std::vector<float> mono(static_cast<std::size_t>(frames));
  for (int c=0;c<buffer.getNumChannels();++c)
   for (int i=0;i<frames;++i) mono[static_cast<std::size_t>(i)]+=buffer.getSample(c,i)/static_cast<float>(buffer.getNumChannels());
  if (std::abs(sourceRate-rate)>1.0) {
   // Resample to the pad's rate (linear is enough for a take recorded at another rate).
   const double ratio=sourceRate/rate;
   std::vector<float> out(static_cast<std::size_t>(static_cast<double>(frames)/ratio));
   for (std::size_t i=0;i<out.size();++i) {
    const double at=static_cast<double>(i)*ratio;
    const auto k=static_cast<std::size_t>(at);
    const float f=static_cast<float>(at-static_cast<double>(k));
    out[i]=k+1<mono.size()?mono[k]+(mono[k+1]-mono[k])*f:mono[std::min(k,mono.size()-1)];
   }
   mono=std::move(out);
  }
  // Loudness of the attack decides the take's place among the dynamics.
  const auto attack=std::min<std::size_t>(mono.size(),static_cast<std::size_t>(rate*.05));
  double sum=0.0;
  for (std::size_t i=0;i<attack;++i) sum+=static_cast<double>(mono[i])*mono[i];
  takes.emplace_back(static_cast<float>(std::sqrt(sum/static_cast<double>(std::max<std::size_t>(1,attack)))),std::move(mono));
  paths.add(file.getFullPathName());
 }
 if (takes.empty()) return false;
 std::stable_sort(takes.begin(),takes.end(),[](const auto& a,const auto& b){return a.first<b.first;});
 // Level the loudest take to the library's -6 dBFS so pads sit together.
 float peak=0.f;
 for (const auto& t : takes) for (const float x : t.second) peak=std::max(peak,std::abs(x));
 const float gain=peak>1e-6f?.5f/peak:1.f;
 for (auto& t : takes) {
  for (auto& x : t.second) x*=gain;
  set->layers.push_back(std::move(t.second));
 }
 set->sampleRate=rate;
 set->name=(paths.size()==1?juce::File(paths[0]).getFileNameWithoutExtension()
                           :juce::File(paths[0]).getParentDirectory().getFileName()+" ("+juce::String(paths.size())+")").toStdString();
 queuePad(pad,std::move(set),paths);
 return true;
}

void InversoAbismoDrumsProcessor::clearPadSample(int pad) {
 if (pad<0 || pad>=numPads) return;
 queuePad(pad,nullptr,{});
}

void InversoAbismoDrumsProcessor::collectGarbage() {
 std::array<std::shared_ptr<SampleSet>,numPads> old;
 {
  const juce::SpinLock::ScopedLockType lock(swapLock);
  for (std::size_t i=0;i<old.size();++i) old[i]=std::move(slots[i].retired);
 }
 // `old` is destroyed here, on the calling (message) thread.
}

juce::String InversoAbismoDrumsProcessor::getPadSampleName(int pad) const {
 if (pad<0 || pad>=numPads) return {};
 const juce::ScopedLock lock(pathLock);
 const auto& paths=slots[static_cast<std::size_t>(pad)].paths;
 if (paths.isEmpty()) return {};
 return paths.size()==1?juce::File(paths[0]).getFileNameWithoutExtension()
                       :juce::File(paths[0]).getParentDirectory().getFileName()+" ("+juce::String(paths.size())+")";
}
std::shared_ptr<const SampleSet> InversoAbismoDrumsProcessor::getPadDisplay(int pad) const {
 if (pad<0 || pad>=numPads) return {};
 const juce::ScopedLock lock(pathLock);
 return slots[static_cast<std::size_t>(pad)].display;
}
juce::String InversoAbismoDrumsProcessor::getPadMissing(int pad) const {
 if (pad<0 || pad>=numPads) return {};
 const juce::ScopedLock lock(pathLock);
 return slots[static_cast<std::size_t>(pad)].missing.joinIntoString("\n");
}
bool InversoAbismoDrumsProcessor::isAudioFile(const juce::String& path) const {
 return formats.findFormatForFileExtension(juce::File(path).getFileExtension())!=nullptr;
}

int InversoAbismoDrumsProcessor::getNumPrograms() { return static_cast<int>(kits().size()); }
const juce::String InversoAbismoDrumsProcessor::getProgramName(int index) {
 if (index<0 || index>=getNumPrograms()) return {};
 return juce::String::fromUTF8(kits()[static_cast<std::size_t>(index)].name);
}
// Hosts re-send the current program (Cubase does on load and while playing);
// re-applying it would wipe the user's edits, so only a different kit is applied.
void InversoAbismoDrumsProcessor::setCurrentProgram(int index) {
 if (index<0 || index>=getNumPrograms() || index==currentProgram) return;
 applyKit(index);
 // Tell the host which program is now active. Without this its program
 // parameter keeps the old kit and the host switches back to it.
 updateHostDisplay(ChangeDetails().withProgramChanged(true));
}

// From the editor: always applies, even the kit already selected, so choosing a kit again
// brings its own sounds back.
// Only here, a deliberate choice in the editor: a program change from the host or automation
// never discards your recordings (or the paths of missing ones).
void InversoAbismoDrumsProcessor::selectKit(int index) {
 if (index<0 || index>=getNumPrograms()) return;
 // Every pad's recording is queued to go before the kit starts fading (kitBusy holds the swaps
 // meanwhile), so all of them leave together with the old kit, faded, not cut.
 {
  const KitBusy busy(kitBusy);
  for (int pad=0;pad<numPads;++pad) {
   bool user;
   { const juce::ScopedLock lock(pathLock);
     const auto& slot=slots[static_cast<std::size_t>(pad)];
     user=!slot.paths.isEmpty() || !slot.missing.isEmpty() || slot.display!=nullptr; }
   if (user) clearPadSample(pad);
  }
  applyKit(index); // bumps the generation: the fade, then the swaps with the new kit
 }
 updateHostDisplay(ChangeDetails().withProgramChanged(true)); // after the kit is written: the host may call back
}

void InversoAbismoDrumsProcessor::applyKit(int index) {
 const KitBusy busy(kitBusy); // the kit's values are written while the old kit plays on, then it fades
 currentProgram=index;
 const auto values=kitValues(static_cast<std::size_t>(index));
 for (std::size_t i=0;i<values.size();++i) {
  if (auto* param=apvts.getParameter(specs()[i].id)) {
   param->beginChangeGesture();
   param->setValueNotifyingHost(param->convertTo0to1(values[i]));
   param->endChangeGesture();
  }
 }
 ++kitGeneration; // written: the audio thread fades the old kit and starts this one
}

// Pad recordings are stored by path, not embedded: sessions stay small, but a
// moved or deleted file means the pad plays its OBJECT until it is reloaded.
void InversoAbismoDrumsProcessor::getStateInformation(juce::MemoryBlock& dest) {
 auto state=apvts.copyState();
 midiLearn.save(state);
 state.setProperty("program",currentProgram,nullptr);
 {
  const juce::ScopedLock lock(pathLock);
  for (int pad=0;pad<numPads;++pad) {
   const auto& slot=slots[static_cast<std::size_t>(pad)];
   const auto& paths=slot.missing.isEmpty()?slot.paths:slot.missing;
   if (!paths.isEmpty()) state.setProperty("padSample"+juce::String(pad),paths.joinIntoString("\n"),nullptr);
  }
 }
 if (auto xml=state.createXml()) copyXmlToBinary(*xml,dest);
}
void InversoAbismoDrumsProcessor::setStateInformation(const void* data,int size) {
 auto xml=getXmlFromBinary(data,size);
 if (xml && xml->hasTagName(apvts.state.getType())) {
  auto state=juce::ValueTree::fromXml(*xml);
  currentProgram=juce::jlimit(0,getNumPrograms()-1,static_cast<int>(state.getProperty("program",0)));
  std::array<juce::StringArray,numPads> wanted;
  for (int pad=0;pad<numPads;++pad) {
   const juce::Identifier key("padSample"+juce::String(pad));
   wanted[static_cast<std::size_t>(pad)]=juce::StringArray::fromLines(state.getProperty(key,juce::String()).toString());
   wanted[static_cast<std::size_t>(pad)].removeEmptyStrings();
   state.removeProperty(key,nullptr);
  }
  apvts.replaceState(state);
  restoreEveryParameter(apvts);
  midiLearn.load(state);
  for (int pad=0;pad<numPads;++pad) {
   const auto& paths=wanted[static_cast<std::size_t>(pad)];
   {
    const juce::ScopedLock lock(pathLock);
    if (paths==slots[static_cast<std::size_t>(pad)].paths) continue;
   }
   if (paths.isEmpty()) { clearPadSample(pad); continue; }
   juce::Array<juce::File> files;
   juce::StringArray missing;
   for (const auto& path : paths) { const juce::File f(path); if (f.existsAsFile()) files.add(f); else missing.add(path); }
   if (!loadPadFiles(pad,files)) {
    clearPadSample(pad);
    missing=paths;
   }
   if (!missing.isEmpty()) {
    // Keep every saved path so a later save does not forget the missing ones.
    const juce::ScopedLock lock(pathLock);
    slots[static_cast<std::size_t>(pad)].missing=paths;
    ++sampleVersion;
   }
  }
 }
}

juce::AudioProcessorEditor* InversoAbismoDrumsProcessor::createEditor(){return new InversoAbismoDrumsEditor(*this);}
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter(){return new InversoAbismoDrumsProcessor();}
