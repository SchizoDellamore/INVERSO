#include "EnkiProcessor.h"
#include "StateRestore.h"
#include "EnkiEditor.h"
#include "EnkiLibrary.h"
#include "AbismoUI.h"
#include <algorithm>
#include <cmath>

using namespace inverso_abismo::enki;

namespace {
constexpr double maxSourceSeconds=45.0, maxDrawSeconds=20.0;
constexpr int maxHistory=100;

juce::File drumLibraryFile() {
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
}

const inverso_abismo::drums::SampleLibrary& InversoEnkiProcessor::library() {
 static const inverso_abismo::drums::SampleLibrary lib=[]{
  inverso_abismo::drums::SampleLibrary l;
  juce::MemoryBlock bytes;
  const auto file=drumLibraryFile();
  if (file==juce::File() || !file.loadFileAsData(bytes)
      || !inverso_abismo::drums::SampleLibrary::parse(static_cast<const unsigned char*>(bytes.getData()),bytes.getSize(),l))
   l.objects.clear();
  return l;
 }();
 return lib;
}

juce::AudioProcessorValueTreeState::ParameterLayout InversoEnkiProcessor::makeParameters() {
 return abismo_ui::makeLayout(specs().data(),P::Count);
}

InversoEnkiProcessor::InversoEnkiProcessor()
 : AudioProcessor(BusesProperties().withOutput("Output",juce::AudioChannelSet::stereo(),true)),
   apvts(*this,nullptr,"PARAMETERS",makeParameters()),
   generator(static_cast<std::uint32_t>(juce::Time::currentTimeMillis())) {
 for (std::size_t i=0;i<raw.size();++i) raw[i]=apvts.getRawParameterValue(specs()[i].id);
 formats.registerBasicFormats();
 // The drum library's recordings, the loudest take of each object.
 const auto& lib=library();
 librarySamples.resize(lib.objects.size());
 for (std::size_t i=0;i<lib.objects.size() && i<static_cast<std::size_t>(numLibrarySamples);++i) {
  auto& d=librarySamples[i].data;
  d=lib.objects[i].layers.back();
  // like a file from disk: no silence before the hit, the loudest moment at -1 dBFS
  std::size_t first=0;
  float peak=0.f;
  for (float x : d) peak=std::max(peak,std::abs(x));
  while (first<d.size() && std::abs(d[first])<peak*.01f) ++first;
  d.erase(d.begin(),d.begin()+static_cast<std::ptrdiff_t>(first>8?first-8:0));
  if (peak>1e-5f) for (auto& x : d) x*=.89f/peak;
  librarySamples[i].sampleRate=lib.objects[i].sampleRate;
  engine.setSample(static_cast<int>(i),&librarySamples[i]);
 }
 pushHistory(snapshot());
 startTimer(500);
}

InversoEnkiProcessor::~InversoEnkiProcessor() { stopTimer(); }

bool InversoEnkiProcessor::isBusesLayoutSupported(const BusesLayout& layout) const {
 return layout.getMainOutputChannelSet()==juce::AudioChannelSet::stereo() && layout.getMainInputChannelSet().isDisabled();
}

void InversoEnkiProcessor::prepareToPlay(double sampleRate,int) {
 engine.setValues(current());
 engine.prepare(sampleRate);
 prepared=true;
 outputPeak.store(0.f);
}
void InversoEnkiProcessor::releaseResources() { prepared=false; }

Values InversoEnkiProcessor::current() const {
 Values v{};
 for (std::size_t i=0;i<raw.size();++i) v[i]=raw[i]->load(std::memory_order_relaxed);
 return v;
}

void InversoEnkiProcessor::processBlock(juce::AudioBuffer<float>& audio,juce::MidiBuffer& midi) {
 juce::ScopedNoDenormals noDenormals;
 const int samples=audio.getNumSamples();
 // New recordings for the sources, swapped in without freeing on this thread.
 if (swapLock.tryEnter()) {
  for (int k=0;k<2;++k) {
   auto& slot=slots[static_cast<std::size_t>(k)];
   if (slot.hasPending && slot.retired.size()<slot.retired.capacity()) {
    slot.retired.push_back(std::move(slot.current));
    slot.current=std::move(slot.pending);
    slot.hasPending=false;
    engine.setSample(numLibrarySamples+k,slot.current.get());
   }
  }
  swapLock.exit();
 }
 if (!prepared || audio.getNumChannels()<2) { audio.clear(); return; }
 engine.setValues(current());
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
 lfoNow.store(engine.lfoValue(),std::memory_order_relaxed);
}

// ---------------------------------------------------------------- samples

std::shared_ptr<InversoEnkiProcessor::Sample> InversoEnkiProcessor::decode(const juce::File& file,double maxSeconds) const {
 std::unique_ptr<juce::AudioFormatReader> reader(const_cast<juce::AudioFormatManager&>(formats).createReaderFor(file));
 if (!reader || reader->lengthInSamples<16 || reader->sampleRate<1000.0) return nullptr;
 const auto frames=static_cast<int>(std::min<juce::int64>(reader->lengthInSamples,static_cast<juce::int64>(reader->sampleRate*maxSeconds)));
 juce::AudioBuffer<float> buffer(static_cast<int>(std::min(2u,reader->numChannels)),frames);
 if (!reader->read(&buffer,0,frames,0,true,buffer.getNumChannels()>1)) return nullptr;
 auto s=std::make_shared<Sample>();
 s->sampleRate=reader->sampleRate;
 s->data.assign(static_cast<std::size_t>(frames),0.f);
 for (int c=0;c<buffer.getNumChannels();++c)
  for (int i=0;i<frames;++i) s->data[static_cast<std::size_t>(i)]+=buffer.getSample(c,i)/static_cast<float>(buffer.getNumChannels());
 // Recordings come at every level: bring the loudest moment to -1 dBFS.
 float peak=0.f;
 for (float x : s->data) peak=std::max(peak,std::abs(x));
 if (peak>1e-5f) for (auto& x : s->data) x*=.89f/peak;
 return s;
}

void InversoEnkiProcessor::queueSlot(int source,std::shared_ptr<Sample> s,const juce::String& path) {
 auto& slot=slots[static_cast<std::size_t>(source)];
 {
  const juce::SpinLock::ScopedLockType lock(swapLock);
  slot.pending=s;
  slot.hasPending=true;
  if (!prepared) {
   slot.current.reset();
   slot.current=std::move(slot.pending);
   slot.hasPending=false;
   engine.setSample(numLibrarySamples+source,slot.current.get());
  }
 }
 const juce::ScopedLock lock(pathLock);
 slot.path=path;
 slot.display=s;
}

void InversoEnkiProcessor::collectGarbage() {
 const juce::SpinLock::ScopedLockType lock(swapLock);
 for (auto& slot : slots) slot.retired.clear();
}

SamplePool InversoEnkiProcessor::poolWith(const std::shared_ptr<Sample>& a,const std::shared_ptr<Sample>& b) const {
 SamplePool pool;
 pool.fill(nullptr);
 for (std::size_t i=0;i<librarySamples.size() && i<static_cast<std::size_t>(numLibrarySamples);++i) pool[i]=&librarySamples[i];
 const juce::ScopedLock lock(pathLock);
 pool[numLibrarySamples]=a?a.get():slots[0].display.get();
 pool[numLibrarySamples+1]=b?b.get():slots[1].display.get();
 return pool;
}

std::uint32_t InversoEnkiProcessor::readySlots() const {
 const juce::ScopedLock lock(pathLock);
 return (slots[0].display?1u:0u)|(slots[1].display?2u:0u);
}

bool InversoEnkiProcessor::loadSourceFile(int source,const juce::File& file) {
 if (source<0 || source>1) return false;
 auto s=decode(file,maxSourceSeconds);
 if (!s) return false;
 queueSlot(source,s,file.getFullPathName());
 const int b=source*P::SourceStride;
 auto set=[this](int p,float v){ if (auto* param=apvts.getParameter(specs()[static_cast<std::size_t>(p)].id)) param->setValueNotifyingHost(param->convertTo0to1(v)); };
 set(P::ASample+b,static_cast<float>(numLibrarySamples+source));
 const int type=juce::roundToInt(raw[static_cast<std::size_t>(P::AType+b)]->load());
 if (type!=3 && type!=4) set(P::AType+b,3.f);
 if (raw[static_cast<std::size_t>(P::ALevel+b)]->load()<5.f) set(P::ALevel+b,70.f);
 playFromStart(source);
 ++soundVersion;
 pushHistory(snapshot());
 return true;
}

void InversoEnkiProcessor::useLibrarySample(int source,int index) {
 const int b=source*P::SourceStride;
 if (auto* param=apvts.getParameter(specs()[static_cast<std::size_t>(P::ASample+b)].id))
  param->setValueNotifyingHost(param->convertTo0to1(static_cast<float>(std::clamp(index,0,numLibrarySamples-1))));
 playFromStart(source);
 ++soundVersion;
 pushHistory(snapshot());
}

// A recording just chosen is heard from its beginning (CHAR is the start point
// for a Sample source; from the middle of a hit there is only the quiet tail).
void InversoEnkiProcessor::playFromStart(int source) {
 const int b=source*P::SourceStride;
 const int type=juce::roundToInt(raw[static_cast<std::size_t>(P::AType+b)]->load());
 if (type!=3 && type!=4) return;
 // Sample: from the top, played once; Grains: the grains start at the top too
 for (int p : {P::AChar+b,P::AMotion+b})
  if (p==P::AChar+b || type==3)
   if (auto* param=apvts.getParameter(specs()[static_cast<std::size_t>(p)].id)) param->setValueNotifyingHost(0.f);
}

void InversoEnkiProcessor::useLibraryRecording(int source,int index) {
 const int b=source*P::SourceStride;
 auto set=[this](int p,float v){ if (auto* param=apvts.getParameter(specs()[static_cast<std::size_t>(p)].id)) param->setValueNotifyingHost(param->convertTo0to1(v)); };
 const int type=juce::roundToInt(raw[static_cast<std::size_t>(P::AType+b)]->load());
 if (type!=3 && type!=4) set(P::AType+b,3.f);
 if (raw[static_cast<std::size_t>(P::ALevel+b)]->load()<5.f) set(P::ALevel+b,70.f);
 useLibrarySample(source,index);
}

juce::String InversoEnkiProcessor::sourceSampleName(int source) const {
 const int index=juce::roundToInt(raw[static_cast<std::size_t>(P::ASample+source*P::SourceStride)]->load());
 if (index<numLibrarySamples) return juce::String(inverso_abismo::drums::sampleObjectName(index));
 const juce::ScopedLock lock(pathLock);
 const auto& path=slots[static_cast<std::size_t>(source)].path;
 return path.isEmpty()?juce::String("(no file loaded)"):juce::File(path).getFileNameWithoutExtension();
}

std::shared_ptr<const Sample> InversoEnkiProcessor::sourceDisplay(int source) const {
 const int index=juce::roundToInt(raw[static_cast<std::size_t>(P::ASample+source*P::SourceStride)]->load());
 if (index<numLibrarySamples) {
  if (index>=0 && static_cast<std::size_t>(index)<librarySamples.size())
   return std::shared_ptr<const Sample>(std::shared_ptr<const Sample>{},&librarySamples[static_cast<std::size_t>(index)]);
  return nullptr;
 }
 const juce::ScopedLock lock(pathLock);
 return slots[static_cast<std::size_t>(source)].display;
}

// ---------------------------------------------------------------- generator, history

InversoEnkiProcessor::Snapshot InversoEnkiProcessor::snapshot() const {
 Snapshot s;
 s.values=current();
 const juce::ScopedLock lock(pathLock);
 s.fileA=slots[0].path;
 s.fileB=slots[1].path;
 return s;
}

void InversoEnkiProcessor::pushHistory(const Snapshot& s) {
 if (historyAt+1<static_cast<int>(history.size())) history.erase(history.begin()+historyAt+1,history.end());
 history.push_back(s);
 if (static_cast<int>(history.size())>maxHistory) history.erase(history.begin());
 historyAt=static_cast<int>(history.size())-1;
}

void InversoEnkiProcessor::apply(const Snapshot& s,bool addToHistory) {
 // Files first, so the sources never point at a slot that is still empty.
 const juce::String files[2]={s.fileA,s.fileB};
 for (int k=0;k<2;++k) {
  juce::String have;
  { const juce::ScopedLock lock(pathLock); have=slots[static_cast<std::size_t>(k)].path; }
  if (files[k].isNotEmpty() && files[k]!=have)
   if (auto smp=decode(juce::File(files[k]),maxSourceSeconds)) queueSlot(k,smp,files[k]);
 }
 for (int p=0;p<P::Count;++p) {
  if (sectionOf(p)==Section::Generator || p==P::PressureCC) continue; // the generator's own settings and the learned knob stay
  if (auto* param=apvts.getParameter(specs()[static_cast<std::size_t>(p)].id)) {
   const float v=s.values[static_cast<std::size_t>(p)];
   if (std::abs(param->convertFrom0to1(param->getValue())-v)>1e-6f) {
    param->beginChangeGesture();
    param->setValueNotifyingHost(param->convertTo0to1(v));
    param->endChangeGesture();
   }
  }
 }
 ++soundVersion;
 if (addToHistory) pushHistory(snapshot());
}

void InversoEnkiProcessor::generate() {
 // Candidate files for each source from ENKI's library (they are used if the draw picks them).
 std::shared_ptr<Sample> cand[2];
 juce::String candPath[2];
 auto& lib=EnkiLibrary::instance();
 if (drawFolder!="-")
  for (int k=0;k<2;++k) {
   for (int tries=0;tries<3 && !cand[k];++tries) {
    const auto f=lib.randomFile(drawFolder,random);
    if (f==juce::File()) break;
    cand[k]=decode(f,maxDrawSeconds);
    if (cand[k]) candPath[k]=f.getFullPathName();
   }
  }
 const auto pool=poolWith(cand[0],cand[1]);
 const std::uint32_t mask=(cand[0]?1u:0u)|(cand[1]?2u:0u);
 Snapshot s;
 s.values=generator.generateLevelled(current(),mask,pool);
 const auto now=snapshot();
 s.fileA=now.fileA; s.fileB=now.fileB;
 for (int k=0;k<2;++k)
  if (cand[k] && juce::roundToInt(s.values[static_cast<std::size_t>(P::ASample+k*P::SourceStride)])==numLibrarySamples+k) {
   queueSlot(k,cand[k],candPath[k]);
   (k==0?s.fileA:s.fileB)=candPath[k];
  }
 apply(s,true);
}

void InversoEnkiProcessor::mutate() {
 const auto amount=raw[static_cast<std::size_t>(P::Mutate)]->load()*.01f;
 Snapshot s=snapshot();
 s.values=generator.mutateLevelled(current(),std::max(.02f,amount),readySlots(),poolWith(nullptr,nullptr));
 apply(s,true);
}

bool InversoEnkiProcessor::back() {
 if (historyAt<=0) return false;
 --historyAt;
 apply(history[static_cast<std::size_t>(historyAt)],false);
 return true;
}
bool InversoEnkiProcessor::forward() {
 if (historyAt+1>=static_cast<int>(history.size())) return false;
 ++historyAt;
 apply(history[static_cast<std::size_t>(historyAt)],false);
 return true;
}

juce::String InversoEnkiProcessor::recipe() const {
 const auto v=current();
 static const char* types[]={"Wave","FM","Noise","Recording","Grains","Resonator","Reed"};
 static const char* filters[]={"low pass","band pass","high pass","comb"};
 static const char* mats[]={"string","wood","metal","glass","bell","abyss","marimba","pipe","membrane","gong","singing bowl","spring",
                            "piano wire","gamelan","cymbal","bone","minor chord","cluster","tritone","rust","wooden body","thunder sheet","underground","cracked bell"};
 auto src=[&](int k){
  const int b=k*P::SourceStride;
  const int t=std::clamp(juce::roundToInt(v[static_cast<std::size_t>(P::AType+b)]),0,6);
  juce::String d(types[t]);
  if (t==3 || t==4) d+=" of "+sourceSampleName(k);
  if (t==5) d+=juce::String(" (")+mats[std::clamp(static_cast<int>(v[static_cast<std::size_t>(P::AChar+b)]*.2399f),0,23)]
               +(v[static_cast<std::size_t>(P::AMotion+b)]>50.f?", bowed)":", struck)");
  const int pitch=juce::roundToInt(v[static_cast<std::size_t>(P::APitch+b)]);
  if (pitch!=0) d+=(pitch>0?" +":" ")+juce::String(pitch)+" st";
  return d;
 };
 juce::StringArray parts;
 parts.add(src(0));
 if (v[P::BLevel]>1.f) parts.add(src(1));
 juce::String text=parts.joinIntoString("  +  ");
 juce::StringArray tail;
 if (v[P::Ring]>5.f) tail.add("ring modulated");
 tail.add(filters[std::clamp(juce::roundToInt(v[P::FilterMode]),0,3)]);
 if (v[P::Drive]>10.f) tail.add("driven");
 if (v[P::Hold]>=.5f) tail.add("one-shot");
 if (std::abs(v[P::PitchEnv])>3.f) tail.add(v[P::PitchEnv]<0.f?"pitch drop":"pitch rise");
 if (v[P::Gate]>20.f) tail.add("rhythmic gate");
 if (v[P::Glitch]>5.f) tail.add("glitch");
 if (v[P::Reverse]>5.f) tail.add("reversed slices");
 if (v[P::LfoAmp]+v[P::LfoFilter]+v[P::LfoChar]+v[P::LfoPan]>40.f) tail.add("moving");
 if (v[P::EchoMix]>8.f) tail.add("echo");
 if (v[P::RevMix]>50.f) tail.add("big space");
 return text+"\n"+tail.joinIntoString(", ");
}

// ---------------------------------------------------------------- favourites, state

juce::Array<juce::File> InversoEnkiProcessor::favorites() {
 juce::Array<juce::File> list;
 for (const auto& e : juce::RangedDirectoryIterator(EnkiLibrary::favoritesFolder(),false,"*.enki",juce::File::findFiles)) list.add(e.getFile());
 std::sort(list.begin(),list.end(),[](const juce::File& a,const juce::File& b){return a.getFileName().compareNatural(b.getFileName())<0;});
 return list;
}

static std::unique_ptr<juce::XmlElement> soundXml(const juce::AudioProcessorValueTreeState& apvts,const juce::String& fileA,const juce::String& fileB) {
 auto xml=std::make_unique<juce::XmlElement>("ENKI_SOUND");
 for (int p=0;p<P::Count;++p) {
  const auto& s=specs()[static_cast<std::size_t>(p)];
  if (auto* param=apvts.getParameter(s.id)) xml->setAttribute(s.id,param->convertFrom0to1(param->getValue()));
 }
 xml->setAttribute("fileA",fileA);
 xml->setAttribute("fileB",fileB);
 return xml;
}

bool InversoEnkiProcessor::saveFavorite(const juce::String& name) {
 const auto folder=EnkiLibrary::favoritesFolder();
 folder.createDirectory();
 const auto snap=snapshot();
 auto xml=soundXml(apvts,snap.fileA,snap.fileB);
 return xml->writeTo(folder.getChildFile(juce::File::createLegalFileName(name.isEmpty()?juce::String("Sound"):name)+".enki"));
}

bool InversoEnkiProcessor::loadFavorite(const juce::File& file) {
 auto xml=juce::parseXML(file);
 if (!xml || !xml->hasTagName("ENKI_SOUND")) return false;
 Snapshot s=snapshot();
 for (int p=0;p<P::Count;++p) {
  const auto& spec=specs()[static_cast<std::size_t>(p)];
  if (xml->hasAttribute(spec.id)) s.values[static_cast<std::size_t>(p)]=static_cast<float>(xml->getDoubleAttribute(spec.id));
 }
 s.fileA=xml->getStringAttribute("fileA");
 s.fileB=xml->getStringAttribute("fileB");
 apply(s,true);
 return true;
}

void InversoEnkiProcessor::getStateInformation(juce::MemoryBlock& dest) {
 auto state=apvts.copyState();
 midiLearn.save(state);
 const auto snap=snapshot();
 state.setProperty("fileA",snap.fileA,nullptr);
 state.setProperty("fileB",snap.fileB,nullptr);
 state.setProperty("drawFolder",drawFolder,nullptr);
 if (auto xml=state.createXml()) copyXmlToBinary(*xml,dest);
}

void InversoEnkiProcessor::setStateInformation(const void* data,int size) {
 auto xml=getXmlFromBinary(data,size);
 if (!xml || !xml->hasTagName(apvts.state.getType())) return;
 auto state=juce::ValueTree::fromXml(*xml);
 const juce::String files[2]={state.getProperty("fileA",{}).toString(),state.getProperty("fileB",{}).toString()};
 drawFolder=state.getProperty("drawFolder",{}).toString();
 for (auto key : {"fileA","fileB","drawFolder"}) state.removeProperty(key,nullptr);
 apvts.replaceState(state);
 restoreEveryParameter(apvts);
 midiLearn.load(state);
 for (int k=0;k<2;++k)
  if (files[k].isNotEmpty())
   if (auto s=decode(juce::File(files[k]),maxSourceSeconds)) queueSlot(k,s,files[k]);
 ++soundVersion;
 history.clear();
 pushHistory(snapshot());
}

juce::AudioProcessorEditor* InversoEnkiProcessor::createEditor(){return new InversoEnkiEditor(*this);}
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter(){return new InversoEnkiProcessor();}
