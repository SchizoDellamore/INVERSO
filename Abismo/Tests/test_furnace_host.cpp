#include "../Source/FurnaceProcessor.h"
#include "FurnaceDI.h"
#include <JuceHeader.h>
#include "StateRoundTrip.h"
#include <cmath>
#include <cstdio>
#include <cstdlib>

using namespace inverso_abismo::furnace;

static void verify(bool okay,const char* message){
 if(!okay){std::fprintf(stderr,"HOST SMOKE FAILED: %s\n",message);std::exit(1);}
}
static void param(InversoFurnaceProcessor& p,const char* name,float value){
 auto* raw=dynamic_cast<juce::RangedAudioParameter*>(p.apvts.getParameter(name));
 verify(raw!=nullptr,"missing parameter");
 raw->setValueNotifyingHost(raw->convertTo0to1(value));
}
// Plays a DI through the plugin in host-sized blocks; returns the output (left).
static std::vector<float> play(InversoFurnaceProcessor& p,const std::vector<float>& di,int blockSize,int channels){
 std::vector<float> out;
 out.reserve(di.size());
 juce::MidiBuffer midi;
 for(std::size_t done=0;done<di.size();done+=static_cast<std::size_t>(blockSize)){
  const int n=static_cast<int>(std::min<std::size_t>(static_cast<std::size_t>(blockSize),di.size()-done));
  juce::AudioBuffer<float> block(channels,n);
  for(int c=0;c<channels;++c) block.copyFrom(c,0,di.data()+done,n);
  p.processBlock(block,midi);
  for(int i=0;i<n;++i){
   const float x=block.getSample(0,i);
   verify(std::isfinite(x),"non-finite output");
   verify(std::abs(x)<=1.f,"output above 0 dBFS");
   if(channels>1) verify(std::abs(block.getSample(1,i)-x)<1e-9f,"channels differ");
   out.push_back(x);
  }
 }
 return out;
}
static float rmsDb(const std::vector<float>& x,std::size_t from=0){
 double s=0; for(std::size_t i=from;i<x.size();++i) s+=static_cast<double>(x[i])*x[i];
 return static_cast<float>(10*std::log10(s/static_cast<double>(std::max<std::size_t>(1,x.size()-from))+1e-20));
}
// The cabinet is designed on a background thread; give it a moment.
static void settle(){ juce::Thread::sleep(400); }

int main(){
 juce::ScopedJuceInitialiser_GUI init;
 { InversoFurnaceProcessor rt; if (!stateRoundTrip(rt, "furnace")) return 1; }
 InversoFurnaceProcessor p;
 for(auto layout:{std::pair{juce::AudioChannelSet::mono(),juce::AudioChannelSet::mono()},
                  std::pair{juce::AudioChannelSet::mono(),juce::AudioChannelSet::stereo()},
                  std::pair{juce::AudioChannelSet::stereo(),juce::AudioChannelSet::stereo()}}){
  juce::AudioProcessor::BusesLayout l; l.inputBuses.add(layout.first); l.outputBuses.add(layout.second);
  verify(p.checkBusesLayoutSupported(l),"a guitar layout was rejected");
 }
 verify(p.getNumPrograms()>=12,"factory presets missing");
 for(double sr:{44100.0,48000.0,96000.0}){
  p.prepareToPlay(sr,512);
  settle();
  verify(p.getLatencySamples()==Amp::latency,"latency not reported");
  const auto riff=furnace_di::riff(sr), bass=furnace_di::bassLine(sr);
  for(int program=0;program<p.getNumPrograms();program+=2){
   p.setCurrentProgram(program);
   settle();
   const bool isBass=juce::String(p.getProgramName(program)).containsIgnoreCase("bass");
   for(int size:{1,64,1024}){
    if(size==1 && sr>50000) continue;
    std::vector<float> part(isBass?bass.begin():riff.begin(),(isBass?bass.begin():riff.begin())+static_cast<long>(sr*1.5));
    const auto out=play(p,part,size,2);
    verify(rmsDb(out,static_cast<std::size_t>(sr*.2))>-45.f,"a preset is silent");
   }
  }
 }
 // Cabinet off, user IR from a file, then back to a designed cabinet.
 p.prepareToPlay(48000,256);
 p.setCurrentProgram(1); p.setCurrentProgram(0);
 settle();
 const auto riff=furnace_di::riff(48000);
 std::vector<float> part(riff.begin(),riff.begin()+48000);
 const float withCab=rmsDb(play(p,part,256,2),4800);
 param(p,"CAB",0.f); settle();
 const float noCab=rmsDb(play(p,part,256,2),4800);
 std::printf("cab on %.1f dBFS, cab off %.1f dBFS\n",withCab,noCab);
 verify(noCab>-40.f,"cab off is silent");
 {
  juce::TemporaryFile tmp(".wav");
  {
   juce::WavAudioFormat wav;
   std::unique_ptr<juce::AudioFormatWriter> w(wav.createWriterFor(new juce::FileOutputStream(tmp.getFile()),48000,1,24,{},0));
   verify(w!=nullptr,"could not write a test IR");
   const auto ir=designCab(7,3,named("Cone"),48000.0);
   juce::AudioBuffer<float> b(1,static_cast<int>(ir.size()));
   b.copyFrom(0,0,ir.data(),static_cast<int>(ir.size()));
   w->writeFromAudioSampleBuffer(b,0,b.getNumSamples());
  }
  verify(p.loadUserIR(tmp.getFile()),"user IR refused");
  settle();
  verify(juce::roundToInt(p.apvts.getRawParameterValue("CAB")->load())==cabUser,"loading an IR did not select it");
  const float user=rmsDb(play(p,part,256,2),4800);
  std::printf("user IR %.1f dBFS\n",user);
  verify(user>-40.f,"user IR is silent");
  juce::MemoryBlock saved;
  p.getStateInformation(saved);
  InversoFurnaceProcessor another;
  another.setStateInformation(saved.getData(),static_cast<int>(saved.getSize()));
  verify(another.userIRName()==p.userIRName(),"user IR not restored");
 }
 // Silence in, silence out (no hum, no hiss) on a clean preset.
 p.setCurrentProgram(1); settle();
 std::vector<float> silence(48000,0.f);
 for(int k=0;k<4;++k) play(p,silence,256,2); // the spring and the cabinet ring out
 const auto quiet=play(p,silence,256,2);
 std::printf("silence in: %.1f dBFS out\n",rmsDb(quiet));
 verify(rmsDb(quiet)<-100.f,"noise with no input");
 // State round trip.
 param(p,"GAIN",7.25f);
 juce::MemoryBlock saved;
 p.getStateInformation(saved);
 InversoFurnaceProcessor another;
 another.setStateInformation(saved.getData(),static_cast<int>(saved.getSize()));
 verify(std::abs(another.apvts.getRawParameterValue("GAIN")->load()-7.25f)<.01f,"state restore failed");
 // Demos through the real plugin chain (FURNACE_DEMOS=<folder>).
 if(const char* dir=std::getenv("FURNACE_DEMOS")){
  const auto guitar=furnace_di::riff(48000), clean=furnace_di::clean(48000), bass=furnace_di::bassLine(48000);
  p.prepareToPlay(48000,512);
  for(int program=0;program<p.getNumPrograms();++program){
   p.setCurrentProgram(program); settle();
   const auto v=presetValues(static_cast<std::size_t>(program));
   const bool isBass=modelSpecs()[static_cast<std::size_t>(v[P::Model])].bass || v[P::Cab]>=9;
   const bool isClean=v[P::Model]<=1 && v[P::Gain]<=5.5f;
   const auto& in=isBass?bass:isClean?clean:guitar;
   auto out=play(p,in,512,2);
   // the plugin's latency: line the demo up with the beat
   out.erase(out.begin(),out.begin()+p.getLatencySamples());
   juce::File f=juce::File(dir).getChildFile(juce::String::formatted("furnace_%02d.wav",program));
   f.deleteFile();
   juce::WavAudioFormat wav;
   std::unique_ptr<juce::AudioFormatWriter> w(wav.createWriterFor(new juce::FileOutputStream(f),48000,1,24,{},0));
   juce::AudioBuffer<float> b(1,static_cast<int>(out.size()));
   b.copyFrom(0,0,out.data(),static_cast<int>(out.size()));
   w->writeFromAudioSampleBuffer(b,0,b.getNumSamples());
   std::printf("demo %02d %-22s %.1f dBFS\n",program,presets()[static_cast<std::size_t>(program)].name,rmsDb(out));
  }
  // the DI recordings too, for comparison
  for(auto [name,sig]:{std::pair{"di_riff",&guitar},std::pair{"di_clean",&clean},std::pair{"di_bass",&bass}}){
   juce::File f=juce::File(dir).getChildFile(juce::String(name)+".wav");
   f.deleteFile();
   juce::WavAudioFormat wav;
   std::unique_ptr<juce::AudioFormatWriter> w(wav.createWriterFor(new juce::FileOutputStream(f),48000,1,24,{},0));
   juce::AudioBuffer<float> b(1,static_cast<int>(sig->size()));
   b.copyFrom(0,0,sig->data(),static_cast<int>(sig->size()));
   w->writeFromAudioSampleBuffer(b,0,b.getNumSamples());
  }
 }
 std::unique_ptr<juce::AudioProcessorEditor> editor(p.createEditor());
 verify(editor!=nullptr && editor->getWidth()>0,"editor failed to open");
 if(const char* path=std::getenv("ABISMO_SNAPSHOT")){
  if(const char* program=std::getenv("ABISMO_SNAPSHOT_PROGRAM")){ p.setCurrentProgram(std::atoi(program)); editor.reset(p.createEditor()); }
  const auto image=editor->createComponentSnapshot(editor->getLocalBounds());
  juce::File file(juce::File::getCurrentWorkingDirectory().getChildFile(path));
  file.deleteFile();
  juce::FileOutputStream out(file);
  juce::PNGImageFormat().writeImageToStream(image,out);
 }
 editor.reset();
 std::puts("Furnace JUCE host smoke passed");
}
