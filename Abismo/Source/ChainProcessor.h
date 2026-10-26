#pragma once
#include <JuceHeader.h>
#include "DSP/ChainEngine.h"
#include <array>
#include <atomic>

#ifndef INVERSO_CHAIN_STRIP
#define INVERSO_CHAIN_STRIP 0
#endif

// MASTER, KICK, SNARE, TOMS, HATS, CYMBALS and OVERHEADS: the same strip engine, each with its own
// modules and presets (see DSP/ChainParams.h). The plugin is picked at build time.
class InversoChainProcessor final : public juce::AudioProcessor {
public:
 static constexpr inverso_abismo::chain::Strip strip = static_cast<inverso_abismo::chain::Strip>(INVERSO_CHAIN_STRIP);
 InversoChainProcessor();
 ~InversoChainProcessor() override = default;
 void prepareToPlay(double sampleRate,int samplesPerBlock) override;
 void releaseResources() override;
 void processBlock(juce::AudioBuffer<float>&,juce::MidiBuffer&) override;
 using AudioProcessor::processBlock;
 bool isBusesLayoutSupported(const BusesLayout&) const override;
 juce::AudioProcessorEditor* createEditor() override;
 bool hasEditor() const override {return true;}
 const juce::String getName() const override {return juce::String("INVERSO ")+inverso_abismo::chain::stripName(strip);}
 bool acceptsMidi()const override{return false;}
 bool producesMidi()const override{return false;}
 bool isMidiEffect()const override{return false;}
 double getTailLengthSeconds()const override{return 0.;}
 int getNumPrograms()override;
 int getCurrentProgram()override{return currentProgram;}
 void setCurrentProgram(int)override; // from the host: never over your edits
 void selectProgram(int);            // from the preset bar: always applies
 const juce::String getProgramName(int)override;
 void changeProgramName(int,const juce::String&)override{}
 void getStateInformation(juce::MemoryBlock&) override;
 void setStateInformation(const void*,int) override;

 // Every global parameter: the ones this plugin has from the host, the rest at their defaults.
 inverso_abismo::chain::Values currentValues() const;

 juce::AudioProcessorValueTreeState apvts;
 inverso_abismo::chain::Engine engine{strip}; // the editor reads its meters
 std::atomic<float> outputPeak{0.f};
 static juce::AudioProcessorValueTreeState::ParameterLayout makeParameters();
private:
 std::vector<std::atomic<float>*> raw; // by local index
 int currentProgram=0;
 std::atomic<bool> prepared{false};
 JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(InversoChainProcessor)
};
