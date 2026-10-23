#pragma once
#include <JuceHeader.h>
#include "DSP/AbismoEngine.h"
#include <array>
#include <atomic>

class InversoAbismoProcessor final : public juce::AudioProcessor {
public:
 InversoAbismoProcessor();
 ~InversoAbismoProcessor() override = default;
 void prepareToPlay(double sampleRate,int samplesPerBlock) override;
 void releaseResources() override;
 void processBlock(juce::AudioBuffer<float>&,juce::MidiBuffer&) override;
 using AudioProcessor::processBlock;
 bool isBusesLayoutSupported(const BusesLayout&) const override;
 juce::AudioProcessorEditor* createEditor() override;
 bool hasEditor() const override {return true;}
 const juce::String getName() const override {return "INVERSO ABISMO";}
 bool acceptsMidi()const override{return true;}
 bool producesMidi()const override{return false;}
 bool isMidiEffect()const override{return false;}
 double getTailLengthSeconds()const override{return 30.0;}
 int getNumPrograms()override;
 int getCurrentProgram()override{return currentProgram;}
 void setCurrentProgram(int)override;
 const juce::String getProgramName(int)override;
 void changeProgramName(int,const juce::String&)override{}
 void getStateInformation(juce::MemoryBlock&) override;
 void setStateInformation(const void*,int) override;

 juce::AudioProcessorValueTreeState apvts;
 std::atomic<float> outputPeak{0.f};
 std::atomic<int> voicesSounding{0};
 static juce::AudioProcessorValueTreeState::ParameterLayout makeParameters();
private:
 inverso_abismo::Engine engine;
 std::array<std::atomic<float>*,inverso_abismo::P::Count> raw{};
 int currentProgram=0;
 bool prepared=false;
 JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(InversoAbismoProcessor)
};
