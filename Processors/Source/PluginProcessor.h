#pragma once
#include <JuceHeader.h>
#include "DSP/AdaptiveSuppressor.h"
#include <array>
#include <atomic>

class InversoProcessor final : public juce::AudioProcessor {
public:
 InversoProcessor();
 void prepareToPlay(double sampleRate,int maximumExpectedSamplesPerBlock) override;
 void releaseResources() override {}
 void processBlock(juce::AudioBuffer<float>&,juce::MidiBuffer&) override;
 bool isBusesLayoutSupported(const BusesLayout&) const override;
 juce::AudioProcessorEditor* createEditor() override;
 bool hasEditor() const override{return true;}
 const juce::String getName() const override {return JucePlugin_Name;}
 bool acceptsMidi() const override{return false;}
 bool producesMidi() const override{return false;}
 bool isMidiEffect() const override{return false;}
 double getTailLengthSeconds() const override{return 0.0;}
 int getNumPrograms() override{return 1;}
 int getCurrentProgram() override{return 0;}
 void setCurrentProgram(int) override{}
 const juce::String getProgramName(int) override{return {};}
 void changeProgramName(int,const juce::String&) override{}
 void getStateInformation(juce::MemoryBlock&) override;
 void setStateInformation(const void*,int) override;
 static juce::AudioProcessorValueTreeState::ParameterLayout makeParameters();
 juce::AudioProcessorValueTreeState apvts;
 std::atomic<float> outputPeak{0},reductionDb{0};
 std::array<std::atomic<float>,inverso_proc::maxBands> bandReductionDb{};
 static constexpr bool isMudder = (INVERSO_DEMUDDER != 0);
 int bandCount()const noexcept{return isMudder?3:7;}
private:
 inverso_proc::AdaptiveSuppressor dsp {isMudder?inverso_proc::Kind::Mudder:inverso_proc::Kind::Annoyance};
 JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(InversoProcessor)
};
