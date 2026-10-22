#pragma once
#include <JuceHeader.h>
#include "DrumEngine.h"
#include <array>
#include <atomic>
#include <memory>

class InversoDrumProcessor final : public juce::AudioProcessor {
public:
 InversoDrumProcessor();
 ~InversoDrumProcessor() override=default;
 void prepareToPlay(double,int) override;
 void releaseResources() override;
 void processBlock(juce::AudioBuffer<float>&,juce::MidiBuffer&) override;
 bool isBusesLayoutSupported(const BusesLayout&) const override;
 juce::AudioProcessorEditor* createEditor() override;
 bool hasEditor()const override{return true;}
 const juce::String getName()const override{return "INVERSO DRUM";}
 bool acceptsMidi()const override{return true;}
 bool producesMidi()const override{return false;}
 bool isMidiEffect()const override{return false;}
 double getTailLengthSeconds()const override{return 1.;}
 int getNumPrograms()override{return 1;}
 int getCurrentProgram()override{return 0;}
 void setCurrentProgram(int)override{}
 const juce::String getProgramName(int)override{return {};}
 void changeProgramName(int,const juce::String&)override{}
 void getStateInformation(juce::MemoryBlock&) override;
 void setStateInformation(const void*,int) override;
 bool loadPadFile(int,const juce::File&);
 juce::String sampleName(int)const;
 void auditionPad(int pad) noexcept {pendingAudition.store(pad,std::memory_order_release);}
 bool exportMidi(const juce::File&)const;
 inverso_drum::Engine engine;
 std::atomic<float> outputPeak{0.f},hostBpm{120.f};
private:
 juce::AudioFormatManager audioFormats;
 mutable juce::CriticalSection pathLock;
 std::array<juce::String,16> samplePaths{};
 std::atomic<int> pendingAudition{-1};
 double sampleRate=48000;
 bool prepared=false;
 JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(InversoDrumProcessor)
};
