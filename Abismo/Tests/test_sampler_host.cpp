#include "../Source/SamplerEditor.h"
#include "../Source/SamplerProcessor.h"
#include <JuceHeader.h>
#include <cmath>
#include <cstdio>
#include <cstdlib>

static void verify(bool okay,const char* message){
 if(!okay){std::fprintf(stderr,"SAMPLER HOST SMOKE FAILED: %s\n",message);std::exit(1);}
}
static void param(InversoAbismoSamplerProcessor& p,const char* name,float value){
 auto* raw=dynamic_cast<juce::RangedAudioParameter*>(p.apvts.getParameter(name));
 verify(raw!=nullptr,"missing parameter");
 raw->setValueNotifyingHost(raw->convertTo0to1(value));
}
static juce::File writeTestWav(const juce::File& file,double freq,int sr,int channels){
 file.deleteFile();
 juce::WavAudioFormat wav;
 std::unique_ptr<juce::OutputStream> stream=std::make_unique<juce::FileOutputStream>(file);
 auto writer=wav.createWriterFor(stream,juce::AudioFormatWriterOptions{}.withSampleRate(sr).withNumChannels(channels).withBitsPerSample(24));
 verify(writer!=nullptr,"no wav writer");
 juce::AudioBuffer<float> b(channels,sr*2);
 for(int c=0;c<channels;++c)for(int i=0;i<sr*2;++i)b.setSample(c,i,.4f*static_cast<float>(std::sin(2.0*juce::MathConstants<double>::pi*freq*i/sr)));
 writer->writeFromAudioSampleBuffer(b,0,b.getNumSamples());
 return file;
}
static float playNote(InversoAbismoSamplerProcessor& p,int sr,int blockSize,int note,float seconds,std::vector<float>* capture=nullptr){
 double sum=0.0;long count=0;
 const int total=static_cast<int>(sr*seconds);
 for(int done=0;done<total;done+=blockSize){
  juce::AudioBuffer<float> block(2,blockSize);
  block.clear();
  juce::MidiBuffer midi;
  if(done==0) midi.addEvent(juce::MidiMessage::noteOn(1,note,.9f),std::min(blockSize-1,5));
  p.processBlock(block,midi);
  for(int i=0;i<blockSize;++i){
   for(int c=0;c<2;++c){
    const float x=block.getSample(c,i);
    verify(std::isfinite(x) && std::abs(x)<=1.f,"invalid output");
    sum+=x*x;++count;
   }
   if(capture) capture->push_back(block.getSample(0,i));
  }
 }
 juce::MidiBuffer off;off.addEvent(juce::MidiMessage::allNotesOff(1),0);
 juce::AudioBuffer<float> b(2,blockSize);b.clear();p.processBlock(b,off);
 return static_cast<float>(std::sqrt(sum/static_cast<double>(count)));
}
static double zeroCrossFrequency(const std::vector<float>& x,int sr,std::size_t from){
 double first=-1,last=-1;int n=0;
 for(std::size_t i=from+1;i<x.size();++i)
  if(x[i-1]<0.f && x[i]>=0.f){const double t=static_cast<double>(i-1)+x[i-1]/static_cast<double>(x[i-1]-x[i]);if(first<0)first=t;else ++n;last=t;}
 return n>0?sr*n/(last-first):0.0;
}

int main(){
 juce::ScopedJuceInitialiser_GUI init;
 const auto dir=juce::File::getSpecialLocation(juce::File::tempDirectory).getChildFile("abismo_sampler_smoke");
 dir.createDirectory();
 const auto stereo=writeTestWav(dir.getChildFile("tone440_stereo.wav"),440.0,44100,2);
 const auto mono=writeTestWav(dir.getChildFile("tone220_mono.wav"),220.0,96000,1);

 InversoAbismoSamplerProcessor p;
 juce::AudioProcessor::BusesLayout layout;
 layout.outputBuses.add(juce::AudioChannelSet::stereo());
 verify(p.setBusesLayout(layout),"stereo instrument layout rejected");
 verify(p.getNumPrograms()>=8,"factory presets missing");
 verify(p.getDisplaySample()!=nullptr && p.getDisplaySample()->length()>0,"built-in sample missing");

 // Built-in voice plays out of the box, every preset, several host rates/blocks.
 for(int sr:{44100,48000,96000}){
  p.prepareToPlay(sr,128);
  for(int program=0;program<p.getNumPrograms();program+=2){
   p.setCurrentProgram(program);
   for(int size:{1,64,1024}) verify(playNote(p,sr,size,60,size==1?.5f:1.5f)>1e-4f,"built-in sample produced no audio");
  }
 }

 // A real 44.1 kHz stereo file, loaded while the "host" is running, at the right pitch on a 48 kHz host.
 p.prepareToPlay(48000,256);
 p.setCurrentProgram(0);
 param(p,"ROOT_NOTE",69);
 param(p,"REV_MIX",0);
 verify(p.loadSampleFile(stereo),"failed to load stereo wav");
 std::vector<float> captured;
 playNote(p,48000,256,81,1.f,&captured);
 const double f=zeroCrossFrequency(captured,48000,4800);
 std::printf("loaded 440 Hz wav, note 81 at 48 kHz host -> %.2f Hz\n",f);
 verify(std::abs(f-880.0)<2.0,"loaded sample must be pitched and rate-converted");
 p.collectGarbage();

 // Swap to a mono 96 kHz file in the middle of a sounding note: no crash, new pitch.
 {
  juce::MidiBuffer midi;midi.addEvent(juce::MidiMessage::noteOn(1,69,1.f),0);
  juce::AudioBuffer<float> b(2,512);b.clear();p.processBlock(b,midi);
 }
 verify(p.loadSampleFile(mono),"failed to load mono wav");
 captured.clear();
 playNote(p,48000,256,69,1.f,&captured);
 verify(std::abs(zeroCrossFrequency(captured,48000,4800)-220.0)<1.5,"mono 96 kHz sample must play at its own pitch");
 p.collectGarbage();
 verify(!p.loadSampleFile(dir.getChildFile("does_not_exist.wav")),"missing file must be rejected");

 // Granular mode on the loaded file.
 param(p,"MODE",1);param(p,"SCAN",0);
 verify(playNote(p,48000,512,69,2.f)>1e-3f,"granular mode produced no audio");

 // Session round trip keeps the path; a missing file falls back to the built-in voice.
 juce::MemoryBlock saved;
 p.getStateInformation(saved);
 InversoAbismoSamplerProcessor restored;
 restored.setStateInformation(saved.getData(),static_cast<int>(saved.getSize()));
 verify(restored.getSamplePath()==mono.getFullPathName(),"sample path not restored");
 verify(restored.getDisplaySample()->right.empty(),"restored sample is not the mono file");
 verify(restored.apvts.getRawParameterValue("MODE")->load()>.5f,"parameters not restored");
 mono.deleteFile();
 InversoAbismoSamplerProcessor orphan;
 orphan.setStateInformation(saved.getData(),static_cast<int>(saved.getSize()));
 verify(orphan.getMissingPath()==mono.getFullPathName(),"missing sample must be reported");
 orphan.prepareToPlay(48000,512);
 verify(playNote(orphan,48000,512,60,1.f)>1e-4f,"missing sample must fall back to the built-in voice");
 juce::MemoryBlock again;
 orphan.getStateInformation(again);
 InversoAbismoSamplerProcessor third;
 third.setStateInformation(again.getData(),static_cast<int>(again.getSize()));
 verify(third.getMissingPath()==mono.getFullPathName(),"missing path must survive a re-save");

 // Editor opens and lays out; optional snapshot for review.
 std::unique_ptr<juce::AudioProcessorEditor> editor(p.createEditor());
 verify(editor!=nullptr && editor->getWidth()>0,"editor failed to open");
 if(const char* path=std::getenv("ABISMO_SNAPSHOT")){
  p.setCurrentProgram(1);
  verify(p.loadSampleFile(stereo),"reload for snapshot");
  p.useBuiltInSample();
  if(auto* e=dynamic_cast<InversoAbismoSamplerEditor*>(editor.get())) e->refreshFromProcessor();
  const auto image=editor->createComponentSnapshot(editor->getLocalBounds());
  juce::File file(juce::File::getCurrentWorkingDirectory().getChildFile(path));
  file.deleteFile();
  juce::FileOutputStream out(file);
  juce::PNGImageFormat().writeImageToStream(image,out);
 }
 editor.reset();
 dir.deleteRecursively();
 std::puts("Abismo sampler JUCE host smoke passed");
}
