#pragma once
#include <JuceHeader.h>
#include "DSP/TideEngine.h"
#include <array>
#include <atomic>

// TIDE: a dynamic EQ for instruments; each band can move only when its part of the spectrum gets loud.
class InversoTideProcessor final : public juce::AudioProcessor {
public:
 InversoTideProcessor();
 ~InversoTideProcessor() override = default;
 void prepareToPlay(double sampleRate,int samplesPerBlock) override;
 void releaseResources() override;
 void processBlock(juce::AudioBuffer<float>&,juce::MidiBuffer&) override;
 using AudioProcessor::processBlock;
 bool isBusesLayoutSupported(const BusesLayout&) const override;
 juce::AudioProcessorEditor* createEditor() override;
 bool hasEditor() const override {return true;}
 const juce::String getName() const override {return "INVERSO TIDE";}
 bool acceptsMidi()const override{return false;}
 bool producesMidi()const override{return false;}
 bool isMidiEffect()const override{return false;}
 double getTailLengthSeconds()const override{return 0.;}
 int getNumPrograms()override;
 int getCurrentProgram()override{return currentProgram;}
 void setCurrentProgram(int)override;
 const juce::String getProgramName(int)override;
 void changeProgramName(int,const juce::String&)override{}
 void getStateInformation(juce::MemoryBlock&) override;
 void setStateInformation(const void*,int) override;

 juce::AudioProcessorValueTreeState apvts;
 inverso_abismo::tide::Engine engine; // the editor reads its display atomics
 std::atomic<float> outputPeak{0.f};
 static juce::AudioProcessorValueTreeState::ParameterLayout makeParameters();
private:
 std::array<std::atomic<float>*,inverso_abismo::tide::P::Count> raw{};
 int currentProgram=0;
 bool prepared=false;
 JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(InversoTideProcessor)
};
