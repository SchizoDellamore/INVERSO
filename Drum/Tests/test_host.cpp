#include "../Source/PluginProcessor.h"
#include <JuceHeader.h>
#include <cmath>
#include <cstdio>
#include <cstdlib>
static void check(bool b,const char* msg){if(!b){std::fprintf(stderr,"HOST TEST FAILED: %s\n",msg);std::exit(1);}}
int main(){
 juce::ScopedJuceInitialiser_GUI init;
 InversoDrumProcessor p;
 for(int rate:{44100,48000,96000}){
  p.prepareToPlay(rate,32);
  for(int sz:{1,17,64,511,2048,8192}){
   juce::AudioBuffer<float> buffer(2,sz);juce::MidiBuffer midi;
   midi.addEvent(juce::MidiMessage::noteOn(10,36,static_cast<juce::uint8>(110)),0);
   p.processBlock(buffer,midi);
   double energy=0;
   for(int i=0;i<sz;++i){const auto l=buffer.getSample(0,i),r=buffer.getSample(1,i);
    check(std::isfinite(l)&&std::isfinite(r),"finite output");energy+=l*l+r*r;}
   check(energy>0,"MIDI note must create audio");
  }
  juce::MemoryBlock mem;p.getStateInformation(mem);check(mem.getSize()>10,"save state");
  InversoDrumProcessor restored;restored.setStateInformation(mem.getData(),static_cast<int>(mem.getSize()));
  check(restored.engine.get(0,0,0)==p.engine.get(0,0,0),"pattern state restored");
  p.engine.setLaneLength(0,128);p.engine.setDivision(0,2,2);
  p.engine.set(0,0,0,17);p.engine.set(0,0,1,91);p.engine.set(0,0,127,127);
  p.engine.sampleStart[0].store(.2f);p.engine.samplePitch[0].store(7.f);
  p.engine.drive[0].store(.5f);p.effects.delayFeel.store(1);
  juce::MemoryBlock extended;p.getStateInformation(extended);
  InversoDrumProcessor expanded;expanded.setStateInformation(extended.getData(),static_cast<int>(extended.getSize()));
  check(expanded.engine.get(0,0,0)==17,"step 1 velocity restored");
  check(expanded.engine.get(0,0,1)==91,"step 2 velocity restored");
  check(expanded.engine.get(0,0,127)==127,"step 128 velocity restored");
  check(expanded.engine.laneLength[0].load()==128,"lane length state restored");
  check(expanded.engine.feel[0].load()==2,"triplet state restored");
  check(std::abs(expanded.engine.samplePitch[0].load()-7)<.01f,"sample pitch state restored");
  check(expanded.effects.delayFeel.load()==1,"effect division state restored");
  // MIDI export must preserve exact 1..127 per-step velocity values, not fixed presets.
  auto midiPath=juce::File::getSpecialLocation(juce::File::tempDirectory)
      .getNonexistentChildFile("INVERSO_velocity_test",".mid",false);
  p.engine.chain.store(false);p.engine.selectedPattern.store(0);
  check(p.exportMidi(midiPath),"MIDI file export failed");
  std::unique_ptr<juce::FileInputStream> stream(midiPath.createInputStream());
  check(stream!=nullptr&&stream->openedOk(),"MIDI file could not reopen");
  juce::MidiFile readBack;
  check(readBack.readFrom(*stream),"MIDI file parse failed");
  const auto* track=readBack.getTrack(0);
  check(track!=nullptr,"MIDI export track missing");
  bool found17=false,found91=false,found127=false;
  for(int i=0;i<track->getNumEvents();++i){
   const auto& msg=track->getEventPointer(i)->message;
   if(msg.isNoteOn()&&msg.getNoteNumber()==36){
    found17|=msg.getVelocity()==17;
    found91|=msg.getVelocity()==91;
    found127|=msg.getVelocity()==127;
   }
  }
  check(found17&&found91&&found127,"distinct step velocities lost in MIDI export");
  stream.reset();midiPath.deleteFile();
  std::unique_ptr<juce::AudioProcessorEditor> ui(p.createEditor());
  check(ui!=nullptr&&ui->getHeight()>=900,"v0.4 velocity editor did not open");
  // End-to-end external sample check: actually create WAV on disk, decode,
  // replace a factory pad, render MIDI audio, save, and restore external path.
  auto testWav=juce::File::getSpecialLocation(juce::File::tempDirectory)
     .getNonexistentChildFile("INVERSO_EXTERNAL_SNARE",".wav",false);
  {
   auto writer=testWav.createOutputStream();
   check(writer!=nullptr&&writer->openedOk(),"cannot create external WAV");
   constexpr int wavFrames=4800;
   writer->write("RIFF",4);
   writer->writeInt(36+wavFrames*2);
   writer->write("WAVE",4);
   writer->write("fmt ",4);
   writer->writeInt(16); // PCM fmt chunk
   writer->writeShort(1); // PCM
   writer->writeShort(1); // mono
   writer->writeInt(48000);
   writer->writeInt(48000*2);
   writer->writeShort(2);writer->writeShort(16);
   writer->write("data",4);
   writer->writeInt(wavFrames*2);
   for(int n=0;n<wavFrames;++n){
    const double y=.65*std::sin(2*juce::MathConstants<double>::pi*880*n/48000.);
    writer->writeShort(static_cast<short>(std::round(y*32767.)));
   }
   check(writer->flush(),"cannot flush external WAV");
  }
  const auto factory=p.engine.getSample(3);
  check(p.loadPadFile(3,testWav),"external WAV failed to load");
  const auto actual=p.engine.getSample(3);
  check(actual!=nullptr && actual!=factory && actual->left.size()==4800,
       "factory sample was not replaced by decoded external WAV");
  check(std::abs(actual->left[30])>.05f,"external WAV contains no decoded audio");
  p.engine.sampleStart[3].store(0.f);
  p.engine.sampleEnd[3].store(1.f);
  p.engine.samplePitch[3].store(0.f);
  p.engine.sampleAttack[3].store(0.f);
  p.engine.sampleDecay[3].store(1.f);
  {
   juce::AudioBuffer<float> playback(2,2048);
   juce::MidiBuffer notes;
   notes.addEvent(juce::MidiMessage::noteOn(10,39,static_cast<juce::uint8>(127)),0);
   p.processBlock(playback,notes);
   double energy=0;
   for(int n=0;n<playback.getNumSamples();++n)
    energy+=std::pow(playback.getSample(0,n),2.0);
   check(energy>1.,"imported WAV cannot be heard through the plugin processBlock");
  }
  juce::MemoryBlock externalState;
  p.getStateInformation(externalState);
  InversoDrumProcessor reopened;
  reopened.prepareToPlay(48000,32);
  reopened.setStateInformation(externalState.getData(),static_cast<int>(externalState.getSize()));
  check(reopened.sampleName(3).contains("INVERSO_EXTERNAL_SNARE"),"sample path not restored");
  check(reopened.engine.getSample(3)!=nullptr &&
        reopened.engine.getSample(3)->left.size()==4800,
        "external WAV not restored after session reload");
  reopened.releaseResources();
  testWav.deleteFile();
  // Step FX are stored individually and survive the full JUCE session-state route.
  p.engine.setEventFx(0,1,3,1,100); // only snare hit at step 4 gets reverb
  p.engine.setEventFx(0,1,3,2,0);   // no delay on that hit
  p.engine.setEventFx(0,1,11,2,83); // snare hit at step 12 gets delay
  p.engine.setEventFx(0,1,11,1,0);
  p.engine.setEventFx(0,1,3,0,57);
  juce::MemoryBlock eventState;p.getStateInformation(eventState);
  InversoDrumProcessor eventCopy;
  eventCopy.setStateInformation(eventState.getData(),static_cast<int>(eventState.getSize()));
  check(eventCopy.engine.getEventFx(0,1,3,1)==100,"reverb hit-specific state lost");
  check(eventCopy.engine.getEventFx(0,1,3,2)==0,"dry delay hit-specific state lost");
  check(eventCopy.engine.getEventFx(0,1,11,2)==83,"delay hit-specific state lost");
  check(eventCopy.engine.getEventFx(0,1,11,1)==0,"dry reverb hit-specific state lost");
  check(eventCopy.engine.getEventFx(0,1,3,0)==57,"drive hit-specific state lost");
  check(eventCopy.engine.getEventFx(0,1,4,1)==255,"adjacent step inherited effect was corrupted");
  p.releaseResources();
 }
 std::puts("Inverso Drum JUCE host smoke passed");
}
