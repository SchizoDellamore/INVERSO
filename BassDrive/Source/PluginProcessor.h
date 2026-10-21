#pragma once
#include <JuceHeader.h>
#include "DSP/BassDriveCore.h"
#include <atomic>
#include <memory>

class InversoBassDriveProcessor final : public juce::AudioProcessor {
public:
 InversoBassDriveProcessor();
 ~InversoBassDriveProcessor() override = default;
 void prepareToPlay(double sampleRate,int samplesPerBlock) override;
 void releaseResources() override;
 void processBlock(juce::AudioBuffer<float>&,juce::MidiBuffer&) override;
 bool isBusesLayoutSupported(const BusesLayout&) const override;
 juce::AudioProcessorEditor* createEditor() override;
 bool hasEditor() const override {return true;}
 const juce::String getName() const override {return "INVERSO BASS DRIVE";}
 bool acceptsMidi()const override{return false;}
 bool producesMidi()const override{return false;}
 bool isMidiEffect()const override{return false;}
 double getTailLengthSeconds()const override{return 0.0;}
 int getNumPrograms()override{return 1;}
 int getCurrentProgram()override{return 0;}
 void setCurrentProgram(int)override{}
 const juce::String getProgramName(int)override{return {};}
 void changeProgramName(int,const juce::String&)override{}
 void getStateInformation(juce::MemoryBlock&) override;
 void setStateInformation(const void*,int) override;

 juce::AudioProcessorValueTreeState apvts;
 std::atomic<float> inputPeak{0.f};
 std::atomic<float> outputPeak{0.f};
 std::atomic<float> oversampledPeak{0.f};
 static juce::AudioProcessorValueTreeState::ParameterLayout makeParameters();
private:
 inverso_bass::Core core;
 std::unique_ptr<juce::dsp::Oversampling<float>> oversampling;
 int preparedChannels=0;
 JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(InversoBassDriveProcessor)
};
