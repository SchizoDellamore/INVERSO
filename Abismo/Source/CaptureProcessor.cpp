#include "CaptureProcessor.h"
#include "StateRestore.h"
#include "CaptureEditor.h"
#include "AbismoUI.h"
#include <cmath>

using namespace inverso_abismo::capture;

juce::AudioProcessorValueTreeState::ParameterLayout InversoCaptureProcessor::makeParameters() {
 return abismo_ui::makeLayout(specs().data(),P::Count);
}

InversoCaptureProcessor::InversoCaptureProcessor()
 : AudioProcessor(BusesProperties().withInput("Input",juce::AudioChannelSet::stereo(),true).withOutput("Output",juce::AudioChannelSet::stereo(),true)),
   apvts(*this,nullptr,"PARAMETERS",makeParameters()) {
 for (std::size_t i=0;i<rawParams.size();++i) rawParams[i]=apvts.getRawParameterValue(specs()[i].id);
}

bool InversoCaptureProcessor::isBusesLayoutSupported(const BusesLayout& layout) const {
 const auto in=layout.getMainInputChannelSet(), out=layout.getMainOutputChannelSet();
 return in==out && (out==juce::AudioChannelSet::mono() || out==juce::AudioChannelSet::stereo());
}

void InversoCaptureProcessor::prepareToPlay(double sampleRate,int) {
 prepared=false;
 {
  const juce::ScopedLock lock(ringLock);
  rate=sampleRate;
  recording.store(false);
  ring.prepare(sampleRate);
 }
 player=Player{}; playerClip=nullptr;  // the audio thread is not running while the host prepares
 playing.store(false);
 command.store(0);
 prepared=true;
}
void InversoCaptureProcessor::releaseResources() { prepared=false; }

Edit InversoCaptureProcessor::currentEdit() const {
 Edit e;
 const double a=raw(P::Start)*.01, b=raw(P::End)*.01;
 e.start=std::min(a,b); e.end=std::max(a,b);
 e.gainDb=raw(P::Gain); e.pitchSt=raw(P::Pitch); e.reverse=raw(P::Reverse)>.5f; e.loop=raw(P::Loop)>.5f;
 e.fadeInMs=raw(P::FadeIn); e.fadeOutMs=raw(P::FadeOut);
 return e;
}

namespace { struct BlockScope { std::atomic<bool>& flag; explicit BlockScope(std::atomic<bool>& f):flag(f){flag.store(true);} ~BlockScope(){flag.store(false);} }; }

void InversoCaptureProcessor::processBlock(juce::AudioBuffer<float>& audio,juce::MidiBuffer&) {
 juce::ScopedNoDenormals noDenormals;
 const int n=audio.getNumSamples();
 if (!prepared || n==0) return;
 const BlockScope inBlock(blockActive); // until this returns, the clip it reads may not be freed
 float* left=audio.getWritePointer(0);
 float* right=audio.getNumChannels()>1?audio.getWritePointer(1):nullptr;

 // what passes is kept while the host plays (or REC is on); a host that does not say is taken as playing
 bool hostPlaying=true;
 if (auto* head=getPlayHead()) if (const auto pos=head->getPosition()) hostPlaying=pos->getIsPlaying();
 if (hostPlaying || recording.load(std::memory_order_relaxed)) ring.write(left,right,n);
 float peak=0.f;
 for (int i=0;i<n;++i) peak=std::max({peak,std::abs(left[i]),right?std::abs(right[i]):0.f});
 level.store(std::max(peak,level.load(std::memory_order_relaxed)*.92f),std::memory_order_relaxed);

 const float dry=raw(P::Dry)*.01f;
 const float sampleDb=raw(P::Sample);
 const float sampleGain=sampleDb<=-59.9f?0.f:std::pow(10.f,sampleDb/20.f);
 if (dry<.9999f) { for (int i=0;i<n;++i) { left[i]*=dry; if (right) right[i]*=dry; } }

 const Clip* clipNow=audioClip.load();
 if (clipNow!=playerClip) { player.setClip(nullptr); playerClip=clipNow; }
 switch (command.exchange(0)) {
  case 1: if (clipNow!=nullptr) player.play(clipNow,currentEdit(),rate); break;
  case 2: player.stop(); break;
  default: break;
 }
 if (player.active()) {
  player.setEdit(currentEdit());
  float* mixR=right?right:left;
  // in chunks, on arrays of its own: nothing is allocated on the audio thread
  for (int done=0;done<n;) {
   const int chunk=std::min(n-done,static_cast<int>(scratchL.size()));
   std::fill_n(scratchL.begin(),chunk,0.f); std::fill_n(scratchR.begin(),chunk,0.f);
   player.render(scratchL.data(),scratchR.data(),chunk);
   for (int i=0;i<chunk;++i) {
    const auto k=static_cast<std::size_t>(i);
    if (right) { left[done+i]+=scratchL[k]*sampleGain; mixR[done+i]+=scratchR[k]*sampleGain; }
    else left[done+i]+=.5f*(scratchL[k]+scratchR[k])*sampleGain;   // a mono track gets both sides of the clip
   }
   done+=chunk;
  }
  position.store(player.position(),std::memory_order_relaxed);
 }
 playing.store(player.active(),std::memory_order_relaxed);
}

// ---------------------------------------------------------------- the clip
std::shared_ptr<const Clip> InversoCaptureProcessor::clip() const {
 const juce::ScopedLock lock(clipLock);
 return current;
}
void InversoCaptureProcessor::housekeeping() {
 const juce::ScopedLock lock(clipLock);
 if (!blockActive.load()) retired.clear(); // no block is running, and a later one reads the new clip
}
void InversoCaptureProcessor::setClip(std::shared_ptr<Clip> c,bool resetRegion) {
 {
  const juce::ScopedLock lock(clipLock);
  if (current) retired.push_back(std::move(current));
  current=std::move(c);
  audioClip.store(current.get());      // published: a block that starts now reads the new one
  if (!blockActive.load()) retired.clear();
 }
 version.fetch_add(1);
 if (resetRegion) {
  for (int p : {P::Start,P::End}) if (auto* param=apvts.getParameter(specs()[static_cast<std::size_t>(p)].id)) {
   param->beginChangeGesture(); param->setValueNotifyingHost(param->convertTo0to1(specs()[static_cast<std::size_t>(p)].def)); param->endChangeGesture();
  }
 }
}

bool InversoCaptureProcessor::capture() {
 if (!prepared) return false;
 const double seconds=captureSeconds(juce::roundToInt(raw(P::CapLen)));
 std::shared_ptr<Clip> c;
 { const juce::ScopedLock lock(ringLock); c=std::make_shared<Clip>(ring.last(static_cast<std::size_t>(seconds*rate))); }
 if (c->frames()<64) return false;   // nothing has passed yet
 undoState.reset();
 setClip(std::move(c),true);
 return true;
}
void InversoCaptureProcessor::startRecording() { if (!prepared) return; const juce::ScopedLock lock(ringLock); recStart.store(ring.position()); recording.store(true); }
bool InversoCaptureProcessor::stopRecording() {
 if (!recording.exchange(false)) return false;
 std::shared_ptr<Clip> c;
 { const juce::ScopedLock lock(ringLock); c=std::make_shared<Clip>(ring.between(recStart.load(),ring.position())); }
 if (c->frames()<64) return false;
 undoState.reset();
 setClip(std::move(c),true);
 return true;
}
double InversoCaptureProcessor::ringSeconds() const { const juce::ScopedLock lock(const_cast<juce::CriticalSection&>(ringLock)); return rate>0?static_cast<double>(std::min<std::uint64_t>(ring.position(),static_cast<std::uint64_t>(rate*maxClipSeconds)))/rate:0.0; }

void InversoCaptureProcessor::play() { command.store(1); }
void InversoCaptureProcessor::stopPlaying() { command.store(2); }

void InversoCaptureProcessor::resetEditParams() {
 for (int p : {P::Start,P::End,P::Gain,P::Pitch,P::Reverse,P::Loop,P::FadeIn,P::FadeOut}) if (auto* param=apvts.getParameter(specs()[static_cast<std::size_t>(p)].id)) {
  param->beginChangeGesture(); param->setValueNotifyingHost(param->convertTo0to1(specs()[static_cast<std::size_t>(p)].def)); param->endChangeGesture();
 }
}
void InversoCaptureProcessor::bake() {
 const auto c=clip();
 if (!c || c->empty()) return;
 auto u=std::make_unique<Undo>();
 u->clip=std::const_pointer_cast<Clip>(c);
 for (std::size_t i=0;i<u->params.size();++i) u->params[i]=raw(static_cast<int>(i));
 auto baked=std::make_shared<Clip>(renderEdit(*c,currentEdit()));
 if (baked->empty()) return;
 undoState=std::move(u);
 setClip(std::move(baked),false);
 resetEditParams();
}
bool InversoCaptureProcessor::canUndo() const { return undoState!=nullptr; }
void InversoCaptureProcessor::undo() {
 if (!undoState) return;
 auto u=std::move(undoState);
 setClip(u->clip,false);
 for (int p : {P::Start,P::End,P::Gain,P::Pitch,P::Reverse,P::Loop,P::FadeIn,P::FadeOut}) if (auto* param=apvts.getParameter(specs()[static_cast<std::size_t>(p)].id)) {
  param->beginChangeGesture(); param->setValueNotifyingHost(param->convertTo0to1(u->params[static_cast<std::size_t>(p)])); param->endChangeGesture();
 }
}
void InversoCaptureProcessor::normalizeClip() {
 const auto c=clip();
 if (!c || c->empty()) return;
 auto u=std::make_unique<Undo>();
 u->clip=std::const_pointer_cast<Clip>(c);
 for (std::size_t i=0;i<u->params.size();++i) u->params[i]=raw(static_cast<int>(i));
 auto n=std::make_shared<Clip>(*c);
 normalize(*n,-1.0);
 undoState=std::move(u);
 setClip(std::move(n),false);
}

// ---------------------------------------------------------------- the file
bool InversoCaptureProcessor::exportWav(const juce::File& file) const {
 const auto c=clip();
 if (!c || c->empty()) return false;
 const Clip out=renderEdit(*c,currentEdit());
 if (out.empty()) return false;
 file.getParentDirectory().createDirectory();
 file.deleteFile();
 std::unique_ptr<juce::OutputStream> stream(file.createOutputStream().release());
 if (!stream) return false;
 juce::WavAudioFormat wav;
 auto writer=wav.createWriterFor(stream,juce::AudioFormatWriterOptions{}.withSampleRate(out.sampleRate).withNumChannels(2).withBitsPerSample(24));
 if (!writer) return false;
 juce::AudioBuffer<float> buffer(2,static_cast<int>(out.frames()));
 buffer.copyFrom(0,0,out.l.data(),static_cast<int>(out.frames()));
 buffer.copyFrom(1,0,out.r.data(),static_cast<int>(out.frames()));
 return writer->writeFromAudioSampleBuffer(buffer,0,buffer.getNumSamples());
}
juce::File InversoCaptureProcessor::makeDragFile() const {
 auto folder=juce::File::getSpecialLocation(juce::File::tempDirectory).getChildFile("INVERSO CAPTURE");
 folder.createDirectory();
 // old ones go: the DAW has copied what it wanted by the time the next drag starts
 // (a DAW may keep referring to a dropped file, so only the week-old ones go)
 for (const auto& f:folder.findChildFiles(juce::File::findFiles,false,"capture *.wav")) if (juce::Time::getCurrentTime().toMilliseconds()-f.getLastModificationTime().toMilliseconds()>7LL*24*3600*1000) f.deleteFile();
 static std::atomic<int> counter{0};
 const auto file=folder.getChildFile("capture "+juce::Time::getCurrentTime().formatted("%Y-%m-%d %H-%M-%S")+" "+juce::String(juce::Time::getMillisecondCounter()%100000)+"-"+juce::String(counter.fetch_add(1))+".wav");
 return exportWav(file)?file:juce::File();
}

// ---------------------------------------------------------------- state
void InversoCaptureProcessor::getStateInformation(juce::MemoryBlock& dest) {
 auto state=apvts.copyState();
 if (const auto c=clip(); c && !c->empty()) {
  if (cachedVersion==version.load() && cachedFlac.isNotEmpty()) { state.setProperty("clipFlac",cachedFlac,nullptr); }
  else {
  juce::MemoryBlock flac;
  {
   juce::FlacAudioFormat format;
   std::unique_ptr<juce::OutputStream> sink(new juce::MemoryOutputStream(flac,false));
   auto writer=format.createWriterFor(sink,juce::AudioFormatWriterOptions{}.withSampleRate(c->sampleRate).withNumChannels(2).withBitsPerSample(24));
   if (writer) {
    juce::AudioBuffer<float> buffer(2,static_cast<int>(c->frames()));
    buffer.copyFrom(0,0,c->l.data(),buffer.getNumSamples());
    buffer.copyFrom(1,0,c->r.data(),buffer.getNumSamples());
    writer->writeFromAudioSampleBuffer(buffer,0,buffer.getNumSamples());
   }
  }
  if (flac.getSize()>0) {
   cachedFlac=juce::Base64::toBase64(flac.getData(),flac.getSize()); cachedVersion=version.load();
   state.setProperty("clipFlac",cachedFlac,nullptr);
  }
  }
 }
 if (auto xml=state.createXml()) copyXmlToBinary(*xml,dest);
}
void InversoCaptureProcessor::setStateInformation(const void* data,int size) {
 auto xml=getXmlFromBinary(data,size);
 if (xml && xml->hasTagName(apvts.state.getType())) {
  auto state=juce::ValueTree::fromXml(*xml);
  const juce::String encoded=state.getProperty("clipFlac").toString();
  state.removeProperty("clipFlac",nullptr);
  apvts.replaceState(state);
  restoreEveryParameter(apvts);
  std::shared_ptr<Clip> loaded;
  if (encoded.isNotEmpty()) {
   juce::MemoryOutputStream decoded;
   if (juce::Base64::convertFromBase64(decoded,encoded)) {
    juce::FlacAudioFormat format;
    std::unique_ptr<juce::AudioFormatReader> reader(format.createReaderFor(new juce::MemoryInputStream(decoded.getData(),decoded.getDataSize(),true),true));
    if (reader && reader->lengthInSamples>0 && reader->lengthInSamples<static_cast<juce::int64>(reader->sampleRate*maxClipSeconds*1.01)) {
     juce::AudioBuffer<float> buffer(2,static_cast<int>(reader->lengthInSamples));
     reader->read(&buffer,0,buffer.getNumSamples(),0,true,true);
     loaded=std::make_shared<Clip>();
     loaded->sampleRate=reader->sampleRate;
     loaded->l.assign(buffer.getReadPointer(0),buffer.getReadPointer(0)+buffer.getNumSamples());
     loaded->r.assign(buffer.getReadPointer(1),buffer.getReadPointer(1)+buffer.getNumSamples());
    }
   }
  }
  undoState.reset();
  setClip(std::move(loaded),false);
 }
}

juce::AudioProcessorEditor* InversoCaptureProcessor::createEditor(){return new InversoCaptureEditor(*this);}
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter(){return new InversoCaptureProcessor();}
