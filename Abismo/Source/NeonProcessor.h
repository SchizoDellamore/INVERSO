#pragma once
#include <JuceHeader.h>
#include "AbismoUI.h"
#include "DSP/NeonEngine.h"
#include <array>
#include <atomic>

class InversoNeonProcessor final : public juce::AudioProcessor {
public:
 InversoNeonProcessor();
 ~InversoNeonProcessor() override = default;
 void prepareToPlay(double sampleRate,int samplesPerBlock) override;
 void releaseResources() override;
 void processBlock(juce::AudioBuffer<float>&,juce::MidiBuffer&) override;
 using AudioProcessor::processBlock;
 bool isBusesLayoutSupported(const BusesLayout&) const override;
 juce::AudioProcessorEditor* createEditor() override;
 bool hasEditor() const override {return true;}
 const juce::String getName() const override {return "INVERSO NEON";}
 bool acceptsMidi()const override{return true;}
 bool producesMidi()const override{return false;}
 bool isMidiEffect()const override{return false;}
 double getTailLengthSeconds()const override{return 10.0;}
 int getNumPrograms()override;
 int getCurrentProgram()override{return currentProgram;}
 void setCurrentProgram(int)override; // from the host: never over your edits
 void selectProgram(int);            // from the preset bar: always applies
 const juce::String getProgramName(int)override;
 void changeProgramName(int,const juce::String&)override{}
 void getStateInformation(juce::MemoryBlock&) override;
 void setStateInformation(const void*,int) override;

 juce::AudioProcessorValueTreeState apvts;
 abismo_ui::MidiLearn midiLearn{*this}; // right-click any knob: MIDI learn
 std::atomic<float> outputPeak{0.f};
 std::atomic<int> voicesSounding{0};
 std::atomic<int> arpStep{-1}; // for the editor: the arpeggio step now playing
 static juce::AudioProcessorValueTreeState::ParameterLayout makeParameters();
private:
 inverso_abismo::neon::Engine engine;
 std::array<std::atomic<float>*,inverso_abismo::neon::P::Count> raw{};
 int currentProgram=0;
 bool prepared=false;
 JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(InversoNeonProcessor)
};
