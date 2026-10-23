#pragma once
#include <JuceHeader.h>
#include "DSP/DrumsEngine.h"
#include <array>
#include <atomic>

class InversoAbismoDrumsProcessor final : public juce::AudioProcessor {
public:
 InversoAbismoDrumsProcessor();
 ~InversoAbismoDrumsProcessor() override = default;
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

 juce::AudioProcessorValueTreeState apvts;
 std::atomic<float> outputPeak{0.f};
 std::atomic<int> hitsSounding{0};
 static juce::AudioProcessorValueTreeState::ParameterLayout makeParameters();
private:
 inverso_abismo::drums::Engine engine;
 std::array<std::atomic<float>*,inverso_abismo::drums::P::Count> raw{};
 std::atomic<std::uint32_t> auditions{0},padLights{0};
 int currentProgram=0;
 bool prepared=false;
 JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(InversoAbismoDrumsProcessor)
};
