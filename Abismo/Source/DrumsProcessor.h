#pragma once
#include <JuceHeader.h>
#include "AbismoUI.h"
#include "DSP/DrumsEngine.h"
#include <array>
#include <atomic>
#include <memory>

class InversoAbismoDrumsProcessor final : public juce::AudioProcessor,private juce::Timer {
public:
 InversoAbismoDrumsProcessor();
 ~InversoAbismoDrumsProcessor() override;
 void prepareToPlay(double sampleRate,int samplesPerBlock) override;
 void releaseResources() override;
 void processBlock(juce::AudioBuffer<float>&,juce::MidiBuffer&) override;
 using AudioProcessor::processBlock;
 bool isBusesLayoutSupported(const BusesLayout&) const override;
 juce::AudioProcessorEditor* createEditor() override;
 bool hasEditor() const override {return true;}
 const juce::String getName() const override {return "INVERSO ABISMO DRUMS";}
 bool acceptsMidi()const override{return true;}
 bool producesMidi()const override{return false;}
 bool isMidiEffect()const override{return false;}
 double getTailLengthSeconds()const override{return 10.0;}
 int getNumPrograms()override;
 int getCurrentProgram()override{return currentProgram;}
 void setCurrentProgram(int)override;
 const juce::String getProgramName(int)override;
 void changeProgramName(int,const juce::String&)override{}
 void getStateInformation(juce::MemoryBlock&) override;
 void setStateInformation(const void*,int) override;

 // Message thread: play a pad from the editor (picked up by the next audio block).
 void auditionPad(int pad){ if(pad>=0 && pad<inverso_abismo::drums::numPads) auditions.fetch_or(1u<<pad); }
 // Pads that sounded since the last call (editor lights).
 std::uint32_t takePadLights(){ return padLights.exchange(0); }

 using SampleSet=inverso_abismo::drums::SampleSet;
 // Message thread. A pad can hold the user's own recordings instead of its OBJECT:
 // one file, or several (up to 16) used as dynamics and round robin, ordered by loudness.
 // Returns false (and keeps what the pad had) when nothing could be read.
 bool loadPadFiles(int pad,const juce::Array<juce::File>&);
 void clearPadSample(int pad);
 // Releases replaced recordings on the message thread; the timer also calls it.
 void collectGarbage();
 // The pad's own recordings: display name ("" when it plays its OBJECT) and missing files.
 juce::String getPadSampleName(int pad) const;
 juce::String getPadMissing(int pad) const;
 // The pad's own recordings for display (null when it plays its OBJECT).
 std::shared_ptr<const SampleSet> getPadDisplay(int pad) const;
 bool isAudioFile(const juce::String& path) const;
 // The built-in recordings (empty only if the embedded library failed to parse).
 static const inverso_abismo::drums::SampleLibrary& library();
 static juce::File libraryFile();
 std::atomic<int> sampleVersion{0};

 juce::AudioProcessorValueTreeState apvts;
 abismo_ui::MidiLearn midiLearn{*this}; // right-click any knob: MIDI learn
 std::atomic<float> outputPeak{0.f};
 std::atomic<int> hitsSounding{0};
 static juce::AudioProcessorValueTreeState::ParameterLayout makeParameters();
private:
 void timerCallback() override {collectGarbage();}
 void queuePad(int pad,std::shared_ptr<SampleSet>,const juce::StringArray& paths);
 inverso_abismo::drums::Engine engine;
 juce::AudioFormatManager formats;
 // Audio thread owns `current`; `pending` and `retired` cross threads under swapLock.
 struct PadSlot {
  std::shared_ptr<SampleSet> current,pending,retired;
  bool hasPending=false; // a pending null clears the pad
  juce::StringArray paths,missing;
  std::shared_ptr<const SampleSet> display; // message thread, under pathLock
 };
 std::array<PadSlot,inverso_abismo::drums::numPads> slots;
 juce::SpinLock swapLock;
 mutable juce::CriticalSection pathLock;
 std::array<std::atomic<float>*,inverso_abismo::drums::P::Count> raw{};
 std::atomic<std::uint32_t> auditions{0},padLights{0};
 // Kit change: fade the current block out, then switch cleanly on the next one.
 std::atomic<bool> kitChanged{false};
 bool kitFadePending=false;
 juce::MidiBuffer deferredMidi; // notes that arrive while the old kit fades: played on the new one
 void applyKit(int index);
 int currentProgram=0;
 std::atomic<bool> prepared{false};
 JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(InversoAbismoDrumsProcessor)
};
