#pragma once
#include <JuceHeader.h>
#include "DSP/ClearEngine.h"
#include <array>
#include <atomic>

// CLEAR: takes the mud and the harshness out of a sound only when they are there.
class InversoClearProcessor final : public juce::AudioProcessor {
public:
 InversoClearProcessor();
 ~InversoClearProcessor() override = default;
 void prepareToPlay(double sampleRate,int samplesPerBlock) override;
 void releaseResources() override;
 void processBlock(juce::AudioBuffer<float>&,juce::MidiBuffer&) override;
 using AudioProcessor::processBlock;
 bool isBusesLayoutSupported(const BusesLayout&) const override;
 juce::AudioProcessorEditor* createEditor() override;
 bool hasEditor() const override {return true;}
 const juce::String getName() const override {return "INVERSO CLEAR";}
 bool acceptsMidi()const override{return false;}
 bool producesMidi()const override{return false;}
 bool isMidiEffect()const override{return false;}
 double getTailLengthSeconds()const override{return .1;}
 int getNumPrograms()override;
 int getCurrentProgram()override{return currentProgram;}
 void setCurrentProgram(int)override;
 const juce::String getProgramName(int)override;
 void changeProgramName(int,const juce::String&)override{}
 void getStateInformation(juce::MemoryBlock&) override;
 void setStateInformation(const void*,int) override;

 juce::AudioProcessorValueTreeState apvts;
 inverso_abismo::clear::Engine engine; // the editor reads its display atomics
 std::atomic<float> outputPeak{0.f};
 static juce::AudioProcessorValueTreeState::ParameterLayout makeParameters();
private:
 std::array<std::atomic<float>*,inverso_abismo::clear::P::Count> raw{};
 int currentProgram=0;
 bool prepared=false;
 JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(InversoClearProcessor)
};
