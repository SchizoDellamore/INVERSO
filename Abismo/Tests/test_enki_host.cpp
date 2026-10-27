#include "../Source/EnkiProcessor.h"
#include <JuceHeader.h>
#include "StateRoundTrip.h"
#include <cmath>
#include <cstdio>
#include <cstdlib>

static void verify(bool okay,const char* message){
 if(!okay){std::fprintf(stderr,"HOST SMOKE FAILED: %s\n",message);std::exit(1);}
}
static void param(InversoEnkiProcessor& p,const char* name,float value){
 auto* raw=dynamic_cast<juce::RangedAudioParameter*>(p.apvts.getParameter(name));
 verify(raw!=nullptr,"missing parameter");
 raw->setValueNotifyingHost(raw->convertTo0to1(value));
}
static float raw(InversoEnkiProcessor& p,const char* name){return p.apvts.getRawParameterValue(name)->load();}
// Plays a short chord across host block sizes; returns RMS.
static float play(InversoEnkiProcessor& p,int sr,int blockSize){
 double sum=0.0;long count=0;
 const int total=sr*2;
 for(int done=0;done<total;done+=blockSize){
  juce::AudioBuffer<float> block(2,blockSize);
  block.clear();
  juce::MidiBuffer midi;
  if(done==0) for(int n:{48,55,60}) midi.addEvent(juce::MidiMessage::noteOn(1,n,.9f),std::min(blockSize-1,7));
  if(done<=sr && done+blockSize>sr) for(int n:{48,55,60}) midi.addEvent(juce::MidiMessage::noteOff(1,n),sr-done);
  p.processBlock(block,midi);
  for(int c=0;c<2;++c)for(int i=0;i<blockSize;++i){
   const float x=block.getSample(c,i);
   verify(std::isfinite(x),"non-finite output");
   verify(std::abs(x)<=1.f,"output above 0 dBFS");
   sum+=x*x;++count;
  }
 }
 return static_cast<float>(std::sqrt(sum/static_cast<double>(count)));
}

int main(){
 juce::ScopedJuceInitialiser_GUI init;
 { InversoEnkiProcessor rt; if (!stateRoundTrip(rt, "enki")) return 1; }
 InversoEnkiProcessor p;
 juce::AudioProcessor::BusesLayout layout;
 layout.outputBuses.add(juce::AudioChannelSet::stereo());
 verify(p.setBusesLayout(layout),"stereo instrument layout rejected");
 verify(p.acceptsMidi(),"instrument must accept MIDI");
 p.drawFolder="-";
 p.reseed(std::getenv("ENKI_SEED")?static_cast<std::uint32_t>(std::atoi(std::getenv("ENKI_SEED"))):12345u); // the same sounds on every run, any seed on request
 // recordings only: the test must not depend on the user's library
 for(int sr:{44100,48000,96000}){
  p.prepareToPlay(sr,64);
  verify(play(p,sr,256)>1e-4f,"the initial sound produced no audio");
  for(int category=0;category<5;++category){
   param(p,"CATEGORY",static_cast<float>(category));
   for(int k=0;k<3;++k){
    p.generate();
    for(int size:{1,64,2048}){ const float lv=play(p,sr,size); if(lv<=1e-5f) std::fprintf(stderr,"silent: sr %d category %d try %d block %d (%g)\n",sr,category,k,size,lv); verify(lv>1e-5f,"a generated sound produced no audio"); }
   }
  }
  p.mutate();
  { const float lv=play(p,sr,512); if(lv<=1e-5f) std::fprintf(stderr,"silent mutation: sr %d level %g: %s\n",sr,lv,p.recipe().toRawUTF8()); verify(lv>1e-5f,"a mutated sound produced no audio"); }
  juce::MidiBuffer midi;
  midi.addEvent(juce::MidiMessage::allSoundOff(1),0);
  param(p,"REV_MIX",0.f); param(p,"ECHO_MIX",0.f);
  juce::AudioBuffer<float> block(2,sr);
  block.clear();
  p.processBlock(block,midi);
  midi.clear();
  block.clear();
  p.processBlock(block,midi);
  if(block.getMagnitude(0,sr)>=1e-5f) std::fprintf(stderr,"residual %g at sr %d, first %g last %g voices %d\n",block.getMagnitude(0,sr),sr,block.getMagnitude(0,0,256),block.getMagnitude(0,sr-256,256),p.voicesSounding.load());
  verify(block.getMagnitude(0,sr)<1e-5f,"all sound off must silence the instrument");
 }
 // History: back and forward return the same sounds.
 p.prepareToPlay(48000,512);
 verify(p.historySize()>=2,"history missing");
 const float cutoffNow=raw(p,"CUTOFF");
 verify(p.back(),"back failed");
 verify(p.forward(),"forward failed");
 verify(std::abs(raw(p,"CUTOFF")-cutoffNow)<.5f,"history did not restore the sound");
 // Locks: a locked section survives GENERATE.
 param(p,"LOCK_SPACE",1.f);
 param(p,"REV_MIX",42.f);
 for(int k=0;k<5;++k) p.generate();
 verify(std::abs(raw(p,"REV_MIX")-42.f)<.5f,"locked space changed");
 param(p,"LOCK_SPACE",0.f);
 // A recording from the library in a source.
 p.useLibraryRecording(0,5);
 param(p,"A_TYPE",3.f);
 verify(play(p,48000,512)>1e-5f,"a library recording produced no audio");
 // The recording must be what sounds: source B silent, then source A on and off.
 {
  for(auto* prm:p.getParameters()) if(auto* r=dynamic_cast<juce::RangedAudioParameter*>(prm)) r->setValueNotifyingHost(r->getDefaultValue());
  param(p,"B_LEVEL",0.f); param(p,"A_LEVEL",0.f);
  p.allSoundOffForTests();
  const float off=play(p,48000,512);
  std::printf("both sources at zero: rms %.6f\n",off);
  param(p,"A_LEVEL",80.f);
  for(int type:{3,4}){
   param(p,"A_TYPE",static_cast<float>(type));
   for(int index:{0,5,20,47}){
    param(p,"A_CHAR",50.f); // the default; choosing the recording must still be heard from its start
    p.useLibraryRecording(0,index);
    p.allSoundOffForTests();
    const float on=play(p,48000,512);
    std::printf("library recording %2d as %s: rms %.5f\n",index,type==3?"sample":"grains",on);
    verify(on>(type==3?3e-3f:1.5e-3f),"a library recording is silent or nearly so in a source");
   }
  }
  // Files from disk, loaded one after another while the plugin runs.
  auto dir=juce::File::getSpecialLocation(juce::File::tempDirectory).getChildFile("enki_host_files");
  dir.createDirectory();
  juce::WavAudioFormat wav;
  auto write=[&](const char* name,float freq,float amp){
   auto f=dir.getChildFile(name);
   f.deleteFile();
   std::unique_ptr<juce::FileOutputStream> out(f.createOutputStream().release());
   std::unique_ptr<juce::AudioFormatWriter> w(wav.createWriterFor(out.get(),44100,1,16,{},0));
   verify(w!=nullptr,"cannot write a test file");
   out.release();
   juce::AudioBuffer<float> b(1,44100);
   for(int i=0;i<44100;++i) b.setSample(0,i,amp*std::sin(6.2831853f*freq*static_cast<float>(i)/44100.f));
   w->writeFromAudioSampleBuffer(b,0,44100);
   return f;
  };
  const auto tone=write("tone.wav",261.6f,.5f), quiet=write("silence.wav",261.6f,0.f), tone2=write("tone2.wav",523.2f,.5f);
  int k=0;
  for(const auto& f:{tone,quiet,tone2,quiet,tone,quiet,tone2}){
   param(p,"A_TYPE",3.f); param(p,"A_CHAR",50.f);
   verify(p.loadSourceFile(0,f),"a file did not load");
   juce::Thread::sleep(20); juce::Timer::callPendingTimersSynchronously(); // a host's message thread runs briefly between loads
   p.allSoundOffForTests();
   const float level=play(p,48000,512);
   const bool silent=f==quiet;
   std::printf("file %d (%s): rms %.5f\n",k++,f.getFileName().toRawUTF8(),level);
   verify(silent?level<1e-5f:level>1e-2f,"a file loaded into a source is not what plays");
  }
  dir.deleteRecursively();
 }

 // MIDI learn: arm a knob, turn CC 21, the knob follows; the assignment is saved with the session.
 {
  const int index=p.midiLearn.indexOf("CUTOFF");
  verify(index>=0,"parameter not found for MIDI learn");
  p.midiLearn.arm(index);
  juce::MidiBuffer cc;
  cc.addEvent(juce::MidiMessage::controllerEvent(1,21,127),2);
  juce::AudioBuffer<float> block(2,512);
  block.clear();
  p.processBlock(block,cc);
  p.midiLearn.flush();
  verify(p.midiLearn.ccFor(index)==21,"MIDI learn must take CC 21");
  auto* param=p.apvts.getParameter("CUTOFF");
  verify(param->getValue()>.99f,"CC 21 at 127 must turn the knob fully up");
  cc.clear(); cc.addEvent(juce::MidiMessage::controllerEvent(1,21,0),0);
  block.clear(); p.processBlock(block,cc); p.midiLearn.flush();
  verify(param->getValue()<.01f,"CC 21 at 0 must turn the knob fully down");
  juce::MemoryBlock saved;
  p.getStateInformation(saved);
  InversoEnkiProcessor other;
  other.setStateInformation(saved.getData(),static_cast<int>(saved.getSize()));
  verify(other.midiLearn.ccFor(index)==21,"MIDI learn not restored with the session");
  p.midiLearn.forget(index);
  verify(p.midiLearn.ccFor(index)<0,"forget did not forget");
 }
 // State round trip.
 param(p,"CUTOFF",1234.f);
 juce::MemoryBlock saved;
 p.getStateInformation(saved);
 verify(saved.getSize()>0,"empty plugin state");
 InversoEnkiProcessor another;
 another.setStateInformation(saved.getData(),static_cast<int>(saved.getSize()));
 verify(std::abs(raw(another,"CUTOFF")-1234.f)<1.f,"state restore failed");

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
 std::puts("Enki JUCE host smoke passed");
}
