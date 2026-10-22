#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <algorithm>
#include <cmath>

InversoDrumProcessor::InversoDrumProcessor()
 : AudioProcessor(BusesProperties().withOutput("Output",juce::AudioChannelSet::stereo(),true)){
 audioFormats.registerBasicFormats();
}
void InversoDrumProcessor::prepareToPlay(double sr,int){
 sampleRate=(std::isfinite(sr)&&sr>=8000)?sr:48000.;
 engine.prepare(sampleRate);
 setLatencySamples(0);prepared=true;
 outputPeak.store(0);
}
void InversoDrumProcessor::releaseResources(){prepared=false;engine.stopTransport();}
bool InversoDrumProcessor::isBusesLayoutSupported(const BusesLayout& layout) const{
 return layout.getMainOutputChannelSet()==juce::AudioChannelSet::stereo()
        && layout.getMainInputChannelSet().isDisabled();
}
void InversoDrumProcessor::processBlock(juce::AudioBuffer<float>& buffer,juce::MidiBuffer& midi){
 juce::ScopedNoDenormals noDenormals;
 buffer.clear();
 if(!prepared||buffer.getNumChannels()<2||buffer.getNumSamples()<=0)return;
 const int samples=buffer.getNumSamples();
 auto* l=buffer.getWritePointer(0),*r=buffer.getWritePointer(1);
 double startPpq=0,bpm=120.;bool playing=false,hasPpq=false;
 if(auto* head=getPlayHead()){
  if(auto pos=head->getPosition()){
   if(auto b=pos->getBpm())if(std::isfinite(*b)&&*b>1.)bpm=*b;
   if(auto q=pos->getPpqPosition())if(std::isfinite(*q)){startPpq=*q;hasPpq=true;}
   playing=pos->getIsPlaying();
  }
 }
 hostBpm.store(static_cast<float>(bpm),std::memory_order_relaxed);
 if(!hasPpq)playing=false; // Never free-run out of sync with Cubase.
 juce::MidiBuffer::Iterator iter(midi);
 juce::MidiMessage msg;
 int nextSample=0;
 bool hasEvent=iter.getNextEvent(msg,nextSample);
 float peak=0.f;
 const double quarterPerSample=bpm/(sampleRate*60.);
 for(int i=0;i<samples;++i){
  while(hasEvent&&nextSample<=i){
   if(msg.isNoteOn()){
    const int note=msg.getNoteNumber();
    if(note>=36&&note<52)engine.trigger(note-36,msg.getFloatVelocity());
   }
   hasEvent=iter.getNextEvent(msg,nextSample);
  }
  if(const int preview=pendingAudition.exchange(-1,std::memory_order_acq_rel);preview>=0)
   engine.trigger(preview,1.f);
  engine.transportSample(startPpq+static_cast<double>(i)*quarterPerSample,playing);
  engine.renderFrame(l[i],r[i]);
  peak=std::max({peak,std::abs(l[i]),std::abs(r[i])});
 }
 outputPeak.store(peak,std::memory_order_relaxed);
}
bool InversoDrumProcessor::loadPadFile(int pad,const juce::File& file){
 if(pad<0||pad>=16||!file.existsAsFile())return false;
 std::unique_ptr<juce::AudioFormatReader> reader(audioFormats.createReaderFor(file));
 if(!reader || reader->sampleRate<8000 || reader->numChannels<1 || reader->lengthInSamples<2)return false;
 const auto length=std::min<juce::int64>(reader->lengthInSamples,
                        static_cast<juce::int64>(reader->sampleRate*12.));
 if(length<2||length>std::numeric_limits<int>::max())return false;
 juce::AudioBuffer<float> buf(2,static_cast<int>(length));
 if(!reader->read(&buf,0,static_cast<int>(length),0,true,true))return false;
 auto sample=std::make_shared<inverso_drum::Sample>();
 sample->sampleRate=reader->sampleRate;
 sample->left.assign(buf.getReadPointer(0),buf.getReadPointer(0)+length);
 sample->right.assign(buf.getReadPointer(1),buf.getReadPointer(1)+length);
 engine.setSample(pad,std::move(sample));
 engine.setUserLoaded(pad,true);
 {juce::ScopedLock lock(pathLock);samplePaths[static_cast<size_t>(pad)]=file.getFullPathName();}
 return true;
}
juce::String InversoDrumProcessor::sampleName(int pad)const{
 if(pad<0||pad>=16)return {};
 juce::ScopedLock lock(pathLock);
 const auto& p=samplePaths[static_cast<size_t>(pad)];
 return p.isNotEmpty()?juce::File(p).getFileNameWithoutExtension():"Factory " + juce::String(pad+1);
}
void InversoDrumProcessor::getStateInformation(juce::MemoryBlock& dest){
 juce::XmlElement root("INVERSO_DRUM_STATE");
 root.setAttribute("version",1);
 root.setAttribute("pattern",engine.selectedPattern.load());
 root.setAttribute("chain",engine.chain.load()?1:0);
 root.setAttribute("swing",static_cast<double>(engine.swing.load()));
 for(int pad=0;pad<16;++pad){
  root.setAttribute("gain"+juce::String(pad),static_cast<double>(engine.gain[pad].load()));
  root.setAttribute("pan"+juce::String(pad),static_cast<double>(engine.pan[pad].load()));
 }
 {juce::ScopedLock lock(pathLock);
  for(int pad=0;pad<16;++pad)root.setAttribute("sample"+juce::String(pad),samplePaths[pad]);
 }
 for(int p=0;p<8;++p){
  auto* pattern=root.createNewChildElement("PATTERN");pattern->setAttribute("index",p);
  for(int lane=0;lane<16;++lane){
   juce::StringArray values;
   for(int step=0;step<16;++step)values.add(juce::String(static_cast<int>(engine.get(p,lane,step))));
   pattern->setAttribute("lane"+juce::String(lane),values.joinIntoString(","));
  }
 }
 copyXmlToBinary(root,dest);
}
void InversoDrumProcessor::setStateInformation(const void* data,int size){
 auto xml=getXmlFromBinary(data,size);
 if(!xml||!xml->hasTagName("INVERSO_DRUM_STATE"))return;
 engine.selectedPattern.store(juce::jlimit(0,7,xml->getIntAttribute("pattern",0)));
 engine.chain.store(xml->getBoolAttribute("chain",false));
 engine.swing.store(juce::jlimit(0.f,1.f,static_cast<float>(xml->getDoubleAttribute("swing",0))));
 for(int pad=0;pad<16;++pad){
  engine.gain[pad].store(juce::jlimit(0.f,2.f,static_cast<float>(xml->getDoubleAttribute("gain"+juce::String(pad),.8))));
  engine.pan[pad].store(juce::jlimit(-1.f,1.f,static_cast<float>(xml->getDoubleAttribute("pan"+juce::String(pad),0))));
 }
 for(auto* element:xml->getChildIterator()){
  if(!element->hasTagName("PATTERN"))continue;
  const int p=element->getIntAttribute("index",-1);
  if(p<0||p>=8)continue;
  for(int lane=0;lane<16;++lane){
   auto parts=juce::StringArray::fromTokens(element->getStringAttribute("lane"+juce::String(lane)),",","");
   if(parts.size()!=16)continue;
   for(int step=0;step<16;++step)engine.set(p,lane,step,parts[step].getIntValue());
  }
 }
 for(int pad=0;pad<16;++pad){
  const auto path=xml->getStringAttribute("sample"+juce::String(pad));
  if(path.isNotEmpty())loadPadFile(pad,juce::File(path));
 }
}
bool InversoDrumProcessor::exportMidi(const juce::File& file)const{
 juce::MidiMessageSequence notes;
 const bool chained=engine.chain.load();
 const int length=chained?8:1;
 constexpr int ticks=960;
 for(int segment=0;segment<length;++segment){
  const int pattern=chained?segment:engine.selectedPattern.load();
  for(int step=0;step<16;++step){
   const double start=static_cast<double>(segment*16+step)*ticks/4.;
   const double swung=start+((step&1)?engine.swing.load()*ticks*.115:0);
   for(int pad=0;pad<16;++pad){
    const int v=engine.get(pattern,pad,step);
    if(v==0)continue;
    auto on=juce::MidiMessage::noteOn(10,36+pad,static_cast<juce::uint8>(v));
    on.setTimeStamp(swung);notes.addEvent(on);
    auto off=juce::MidiMessage::noteOff(10,36+pad);
    off.setTimeStamp(swung+ticks/16.);notes.addEvent(off);
   }
  }
 }
 notes.updateMatchedPairs();
 juce::MidiFile midi;midi.setTicksPerQuarterNote(ticks);midi.addTrack(notes);
 auto stream=file.createOutputStream();
 return stream!=nullptr && midi.writeTo(*stream);
}
juce::AudioProcessorEditor* InversoDrumProcessor::createEditor(){return new InversoDrumEditor(*this);}
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter(){return new InversoDrumProcessor();}
