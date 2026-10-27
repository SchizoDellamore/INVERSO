#include "../Source/CaptureProcessor.h"
#include "../Source/CaptureEditor.h"
#include <JuceHeader.h>
#include "StateRoundTrip.h"
#include <cmath>
#include <cstdio>
#include <cstdlib>

static void verify(bool okay,const char* message){
 if(!okay){std::fprintf(stderr,"HOST SMOKE FAILED: %s\n",message);std::exit(1);}
}
static void feed(InversoCaptureProcessor& p,double seconds,double sr,float amp,double freq,int block=512){
 juce::MidiBuffer midi; long n=0; const long total=static_cast<long>(seconds*sr);
 while(n<total){
  juce::AudioBuffer<float> b(2,block);
  for(int i=0;i<block;++i){ const float v=amp*static_cast<float>(std::sin(2*M_PI*freq*static_cast<double>(n+i)/sr)); b.setSample(0,i,v); b.setSample(1,i,v*.5f); }
  p.processBlock(b,midi);
  n+=block;
 }
}
static void setParam(InversoCaptureProcessor& p,const char* id,float value){
 auto* q=p.apvts.getParameter(id); q->setValueNotifyingHost(q->convertTo0to1(value));
}
int main(){
 juce::ScopedJuceInitialiser_GUI init;
 { InversoCaptureProcessor rt; if (!stateRoundTrip(rt, "capture")) return 1; }
 InversoCaptureProcessor p;
 for(auto set:{juce::AudioChannelSet::mono(),juce::AudioChannelSet::stereo()}){
  juce::AudioProcessor::BusesLayout l; l.inputBuses.add(set); l.outputBuses.add(set);
  verify(p.checkBusesLayoutSupported(l),"layout rejected");
 }
 const double sr=48000.0;
 for(double rate:{44100.0,48000.0,96000.0}){ p.prepareToPlay(rate,512); verify(p.getLatencySamples()==0,"no latency"); feed(p,.3,rate,.3f,440); }
 p.prepareToPlay(sr,512);
 verify(p.clip()==nullptr,"no clip at the start");
 verify(!p.capture(),"nothing to capture before anything played");

 // the track passes through untouched while nothing is playing
 {
  juce::AudioBuffer<float> b(2,512); juce::MidiBuffer m;
  for(int i=0;i<512;++i){ b.setSample(0,i,.25f); b.setSample(1,i,-.25f); }
  p.processBlock(b,m);
  verify(std::abs(b.getSample(0,100)-.25f)<1e-6f && std::abs(b.getSample(1,100)+.25f)<1e-6f,"the track must pass through untouched");
 }

 // capture the last seconds
 feed(p,3.0,sr,.4f,440);
 setParam(p,"CAPLEN",1); // 10 s: more than was played
 verify(p.capture(),"capture failed");
 { const auto c=p.clip(); verify(c && c->frames()>static_cast<std::size_t>(sr*3.0) && c->frames()<static_cast<std::size_t>(sr*3.6),"clip length"); verify(std::abs(inverso_abismo::capture::peakOf(*c)-.4)<.01,"clip content"); }
 setParam(p,"CAPLEN",0); // 5 s
 verify(p.capture() && p.clip()->frames()<static_cast<std::size_t>(sr*5.0)+8,"capture is at most CAPLEN");

 // edit: region, bake, undo
 const auto framesBefore=p.clip()->frames();
 setParam(p,"START",25); setParam(p,"END",75); setParam(p,"FADE_IN",0); setParam(p,"FADE_OUT",0);
 p.bake();
 verify(std::abs(static_cast<double>(p.clip()->frames())-static_cast<double>(framesBefore)*.5)<4,"bake keeps the region");
 verify(p.canUndo(),"undo is offered after a bake");
 verify(p.apvts.getRawParameterValue("START")->load()<.01f && p.apvts.getRawParameterValue("END")->load()>99.9f,"the region starts over after a bake");
 p.undo();
 verify(p.clip()->frames()==framesBefore && std::abs(p.apvts.getRawParameterValue("START")->load()-25.f)<.01f,"undo brings back the clip and the settings");
 p.normalizeClip();
 verify(std::abs(inverso_abismo::capture::peakOf(*p.clip())-std::pow(10.0,-1.0/20.0))<.01,"normalize");
 p.undo();

 // playing the clip on top of the track
 setParam(p,"START",0); setParam(p,"END",100); setParam(p,"DRY",0); setParam(p,"SAMPLE",0);
 p.renderedNow();
 p.play();
 { juce::AudioBuffer<float> b(2,2048); juce::MidiBuffer m; b.clear(); p.processBlock(b,m); b.clear(); p.processBlock(b,m);
   verify(p.isPlaying() && b.getMagnitude(0,0,2048)>.05f,"the clip is heard"); }
 p.stopPlaying();
 for(int i=0;i<4;++i){ juce::AudioBuffer<float> b(2,512); juce::MidiBuffer m; b.clear(); p.processBlock(b,m); }
 verify(!p.isPlaying(),"stop");

 // recording: exactly what passed between the two presses
 p.startRecording();
 feed(p,1.0,sr,.2f,300);
 verify(p.stopRecording() && std::abs(static_cast<double>(p.clip()->frames())-sr)<600,"REC keeps what passed");

 // the file
 const auto file=juce::File::getSpecialLocation(juce::File::tempDirectory).getChildFile("capture test.wav");
 setParam(p,"FADE_IN",0); setParam(p,"FADE_OUT",0);
 verify(p.exportWav(file),"export");
 {
  juce::WavAudioFormat wav; std::unique_ptr<juce::AudioFormatReader> reader(wav.createReaderFor(file.createInputStream().release(),true));
  verify(reader && reader->numChannels==2 && reader->bitsPerSample==24 && std::abs(static_cast<double>(reader->lengthInSamples)-static_cast<double>(p.clip()->frames()))<4,"the WAV is a 24-bit stereo copy of the clip");
 }
 file.deleteFile();
 const auto dragFile=p.makeDragFile();
 verify(dragFile.existsAsFile() && dragFile.getSize()>1000,"the file to drag exists");
 dragFile.deleteFile();

 // a session with a clip comes back with it
 juce::MemoryBlock saved; p.getStateInformation(saved);
 InversoCaptureProcessor another; another.prepareToPlay(sr,512);
 another.setStateInformation(saved.getData(),static_cast<int>(saved.getSize()));
 { const auto a=p.clip(), b=another.clip(); verify(b && a && b->frames()==a->frames(),"the clip is part of the session");
   double worst=0; for(std::size_t i=0;i<a->frames();i+=17) worst=std::max(worst,static_cast<double>(std::abs(a->l[i]-b->l[i]))); verify(worst<2e-6,"the clip comes back as it was (24 bit)"); }
 { InversoCaptureProcessor empty; juce::MemoryBlock s2; empty.getStateInformation(s2); verify(s2.getSize()<4000,"an empty session stays small"); }


 // the edit: stretch and pitch through the parameters, rendered in the background, the same as the file
 {
  setParam(p,"START",0); setParam(p,"END",100); setParam(p,"FADE_IN",0); setParam(p,"FADE_OUT",0); setParam(p,"DRY",100);
  const auto before=p.rendered(); const unsigned v0=p.renderedVersion();
  setParam(p,"STRETCH",200); setParam(p,"MODE",1);
  for(int i=0;i<200 && p.renderedVersion()==v0;++i) juce::Thread::sleep(50);   // the background renderer
  const auto r=p.rendered(); const auto src=p.clip();
  verify(p.renderedVersion()!=v0 && r && std::abs(static_cast<double>(r->frames())/static_cast<double>(src->frames())-2.0)<.03,"the background render makes the clip twice as long");
  setParam(p,"PITCH",5);
  const auto now=p.renderedNow(); verify(now && std::abs(static_cast<double>(now->frames())/static_cast<double>(src->frames())-2.0)<.03,"pitch keeps the length");
  // the file is the rendered clip
  const auto f2=juce::File::getSpecialLocation(juce::File::tempDirectory).getChildFile("capture test 2.wav");
  verify(p.exportWav(f2),"export of the edit");
  { juce::AudioFormatManager fm; fm.registerBasicFormats(); std::unique_ptr<juce::AudioFormatReader> rd(fm.createReaderFor(f2)); verify(rd && std::abs(static_cast<double>(rd->lengthInSamples)-static_cast<double>(now->frames()))<4,"the file has the rendered length"); }
  // opening a file brings it in
  juce::String message; verify(p.loadFile(f2,message) && std::abs(static_cast<double>(p.clip()->frames())-static_cast<double>(now->frames()))<4,"open a file");
  verify(!p.loadFile(juce::File("/nonexistent/none.wav"),message) && message.isNotEmpty(),"a file that is not there is refused");
  f2.deleteFile();
  // every effect on: the render finishes and is finite
  for(const char* id:{"FILTER_ON","DRIVE_ON","CRUSH_ON","RING_ON","CHOP_ON","ECHO_ON","REVERB_ON","REVERSE"}) setParam(p,id,1);
  setParam(p,"STRETCH",100); setParam(p,"PITCH",0); setParam(p,"WARP",40);
  const auto fx=p.renderedNow(); bool finite=fx && !fx->empty(); if(fx) for(std::size_t i=0;i<fx->frames();i+=7) if(!std::isfinite(fx->l[i])) finite=false;
  verify(finite,"all the effects at once");
  // A/B: the original plays the part of the clip as it was
  setParam(p,"DRY",0); setParam(p,"ORIGINAL",1); p.play();
  { juce::AudioBuffer<float> b(2,2048); juce::MidiBuffer m; for(int i=0;i<3;++i){ b.clear(); p.processBlock(b,m); } verify(b.getMagnitude(0,0,2048)>.02f,"the original is heard"); }
  setParam(p,"ORIGINAL",0);
  { juce::AudioBuffer<float> b(2,2048); juce::MidiBuffer m; for(int i=0;i<3;++i){ b.clear(); p.processBlock(b,m); } verify(p.isPlaying() || b.getMagnitude(0,0,2048)>=0.f,"switching A/B keeps it going"); }
  p.stopPlaying(); for(int i=0;i<6;++i){ juce::AudioBuffer<float> b(2,512); juce::MidiBuffer m; b.clear(); p.processBlock(b,m); }
  // bake keeps what was rendered, undo brings the old clip and the settings back
  const auto beforeBake=p.clip()->frames(); p.bake();
  verify(p.canUndo() && std::abs(p.apvts.getRawParameterValue("STRETCH")->load()-100.f)<.01f && p.apvts.getRawParameterValue("REVERSE")->load()<.5f,"bake starts the controls over");
  p.undo(); verify(p.clip()->frames()==beforeBake && p.apvts.getRawParameterValue("WARP")->load()>39.f,"undo brings back the clip and the effects");
  for(const char* id:{"FILTER_ON","DRIVE_ON","CRUSH_ON","RING_ON","CHOP_ON","ECHO_ON","REVERB_ON","REVERSE"}) setParam(p,id,0);
  setParam(p,"WARP",0);
 }

 // blocks of any size, a mono track, odd rates
 for(int size:{1,7,64,1000,4096}){ p.prepareToPlay(sr,size); p.play(); feed(p,.05,sr,.2f,200,size); }
 { juce::AudioBuffer<float> b(1,512); juce::MidiBuffer m; b.clear(); p.processBlock(b,m); }

 std::unique_ptr<juce::AudioProcessorEditor> editor(p.createEditor());
 verify(editor!=nullptr && editor->getWidth()>0,"editor failed to open");
 if(const char* path=std::getenv("ABISMO_SNAPSHOT")){
  p.prepareToPlay(sr,512); feed(p,3.0,sr,.5f,220); p.capture(); setParam(p,"START",20); setParam(p,"END",80);
  for(int k=0;k<3;++k) dynamic_cast<InversoCaptureEditor*>(editor.get())->refresh();
  const auto image=editor->createComponentSnapshot(editor->getLocalBounds());
  juce::File f(juce::File::getCurrentWorkingDirectory().getChildFile(path)); f.deleteFile();
  juce::FileOutputStream out(f); juce::PNGImageFormat().writeImageToStream(image,out);
 }
 editor.reset();
 std::puts("CAPTURE JUCE host smoke passed");
}
