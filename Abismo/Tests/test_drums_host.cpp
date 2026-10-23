#include "../Source/DrumsProcessor.h"
#include <JuceHeader.h>
#include <cmath>
#include <cstdio>
#include <cstdlib>

static void verify(bool okay,const char* message){
 if(!okay){std::fprintf(stderr,"DRUMS HOST SMOKE FAILED: %s\n",message);std::exit(1);}
}
static void param(InversoAbismoDrumsProcessor& p,const char* name,float value){
 auto* raw=dynamic_cast<juce::RangedAudioParameter*>(p.apvts.getParameter(name));
 verify(raw!=nullptr,"missing parameter");
 raw->setValueNotifyingHost(raw->convertTo0to1(value));
}
// Plays a short groove across host block sizes; returns RMS.
static float groove(InversoAbismoDrumsProcessor& p,int sr,int blockSize){
 double sum=0.0;long count=0;
 const int step=sr/6;
 for(int done=0;done<sr*2;done+=blockSize){
  juce::AudioBuffer<float> block(2,blockSize);
  block.clear();
  juce::MidiBuffer midi;
  for(int i=0;i<blockSize;++i){
   const int t=done+i;
   if(t%step==0){
    const int n=36+(t/step)%16;
    midi.addEvent(juce::MidiMessage::noteOn(10,n,.9f),i);
    midi.addEvent(juce::MidiMessage::noteOff(10,n),std::min(blockSize-1,i+1));
   }
  }
  p.processBlock(block,midi);
  for(int c=0;c<2;++c)for(int i=0;i<blockSize;++i){
   const float x=block.getSample(c,i);
   verify(std::isfinite(x) && std::abs(x)<=1.f,"invalid output");
   sum+=x*x;++count;
  }
 }
 return static_cast<float>(std::sqrt(sum/static_cast<double>(count)));
}

int main(){
 juce::ScopedJuceInitialiser_GUI init;
 InversoAbismoDrumsProcessor p;
 juce::AudioProcessor::BusesLayout layout;
 layout.outputBuses.add(juce::AudioChannelSet::stereo());
 verify(p.setBusesLayout(layout),"stereo instrument layout rejected");
 verify(p.getNumPrograms()>=6,"kits missing");
 verify(p.apvts.getParameter("P16_PAN")!=nullptr && p.apvts.getParameter("SAW_WOBBLE")!=nullptr,"parameter table incomplete");
 for(int sr:{44100,48000,96000}){
  p.prepareToPlay(sr,128);
  for(int kit=0;kit<p.getNumPrograms();++kit){
   p.setCurrentProgram(kit);
   for(int size:{1,64,1024}) verify(groove(p,sr,size)>1e-3f,"groove produced no audio");
  }
 }
 // Audition from the editor path, and pad lights.
 p.prepareToPlay(48000,512);
 p.setCurrentProgram(0);
 param(p,"STUMBLE",0);
 p.takePadLights();
 p.auditionPad(4);
 juce::AudioBuffer<float> block(2,512);block.clear();
 juce::MidiBuffer none;
 p.processBlock(block,none);
 verify(block.getMagnitude(0,512)>1e-3f,"audition produced no audio");
 verify(p.takePadLights()==(1u<<4),"pad light missing");
 // Musical saw on C3 and above, released by note-off.
 juce::MidiBuffer saw;saw.addEvent(juce::MidiMessage::noteOn(1,69,1.f),0);
 for(int i=0;i<40;++i){block.clear();p.processBlock(block,saw);saw.clear();}
 verify(block.getMagnitude(0,512)>1e-3f,"saw silent");
 // State round trip with a reassigned pad.
 param(p,"P03_OBJECT",13.f);
 param(p,"P03_TUNE",-7.f);
 juce::MemoryBlock saved;
 p.getStateInformation(saved);
 InversoAbismoDrumsProcessor restored;
 restored.setStateInformation(saved.getData(),static_cast<int>(saved.getSize()));
 verify(std::lround(restored.apvts.getRawParameterValue("P03_OBJECT")->load())==13,"pad object not restored");
 verify(std::abs(restored.apvts.getRawParameterValue("P03_TUNE")->load()+7.f)<.05f,"pad tune not restored");
 // The recordings are found (source tree in this build) and play by default.
 verify(static_cast<int>(InversoAbismoDrumsProcessor::library().objects.size())==inverso_abismo::drums::numSampleObjects,"drum library missing");
 // A pad loaded with the user's own takes: several files become dynamics, survive a session
 // round trip, fall back to the OBJECT when the files are gone, and clear back to the OBJECT.
 {
  auto dir=juce::File::getSpecialLocation(juce::File::tempDirectory).getChildFile("abismo_drums_host_takes");
  dir.deleteRecursively();dir.createDirectory();
  juce::Array<juce::File> takes;
  for(int k=0;k<3;++k){
   auto f=dir.getChildFile("take"+juce::String(k)+".wav");
   juce::WavAudioFormat wav;
   std::unique_ptr<juce::OutputStream> stream=f.createOutputStream();
   auto writer=wav.createWriterFor(stream,juce::AudioFormatWriterOptions{}.withSampleRate(44100).withNumChannels(1).withBitsPerSample(16));
   verify(writer!=nullptr,"wav writer");
   juce::AudioBuffer<float> b(1,8000);
   for(int i=0;i<8000;++i) b.setSample(0,i,(.15f+.3f*static_cast<float>(k))*std::sin(6.2831853f*220.f*static_cast<float>(i)/44100.f)*std::exp(-static_cast<float>(i)/3000.f));
   writer->writeFromAudioSampleBuffer(b,0,8000);
   takes.add(f);
  }
  p.prepareToPlay(48000,512);
  verify(p.loadPadFiles(2,takes),"pad files not loaded");
  verify(p.getPadSampleName(2).contains("(3)"),"pad sample name");
  param(p,"STUMBLE",0);
  juce::AudioBuffer<float> b(2,512);juce::MidiBuffer m;
  for(int i=0;i<20;++i){b.clear();p.processBlock(b,m);} // swap in, let earlier hits die
  m.addEvent(juce::MidiMessage::noteOn(10,38,1.f),0);
  b.clear();p.processBlock(b,m);
  verify(b.getMagnitude(0,512)>1e-3f,"pad sample silent");
  juce::MemoryBlock withTakes;p.getStateInformation(withTakes);
  InversoAbismoDrumsProcessor again;
  again.setStateInformation(withTakes.getData(),static_cast<int>(withTakes.getSize()));
  verify(again.getPadSampleName(2)==p.getPadSampleName(2),"pad sample not restored");
  dir.deleteRecursively();
  InversoAbismoDrumsProcessor lost;
  lost.setStateInformation(withTakes.getData(),static_cast<int>(withTakes.getSize()));
  verify(lost.getPadSampleName(2).isEmpty() && lost.getPadMissing(2).contains("take0"),"missing takes not reported");
  juce::MemoryBlock keep;lost.getStateInformation(keep);
  InversoAbismoDrumsProcessor kept;kept.setStateInformation(keep.getData(),static_cast<int>(keep.getSize()));
  verify(kept.getPadMissing(2).contains("take2"),"missing paths forgotten on save");
  p.clearPadSample(2);
  for(int i=0;i<3;++i){b.clear();m.clear();p.processBlock(b,m);}
  p.collectGarbage();
  verify(p.getPadSampleName(2).isEmpty(),"pad not cleared");
 }
 p.setCurrentProgram(0); // editor opens on the default kit
 std::unique_ptr<juce::AudioProcessorEditor> editor(p.createEditor());
 verify(editor!=nullptr && editor->getWidth()>0,"editor failed to open");
 if(const char* path=std::getenv("ABISMO_SNAPSHOT")){
  const auto image=editor->createComponentSnapshot(editor->getLocalBounds());
  juce::File file(juce::File::getCurrentWorkingDirectory().getChildFile(path));
  file.deleteFile();
  juce::FileOutputStream out(file);
  juce::PNGImageFormat().writeImageToStream(image,out);
 }
 editor.reset();
 std::puts("Abismo drums JUCE host smoke passed");
}
