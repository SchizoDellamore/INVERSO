#pragma once
#include <JuceHeader.h>
#include "DSP/SamplerEngine.h"
#include <array>
#include <atomic>
#include <memory>

class InversoAbismoSamplerProcessor final : public juce::AudioProcessor,private juce::Timer {
public:
 using SampleData=inverso_abismo::sampler::SampleData;
 InversoAbismoSamplerProcessor();
 ~InversoAbismoSamplerProcessor() override;
 void prepareToPlay(double sampleRate,int samplesPerBlock) override;
 void releaseResources() override;
 void processBlock(juce::AudioBuffer<float>&,juce::MidiBuffer&) override;
 using AudioProcessor::processBlock;
 bool isBusesLayoutSupported(const BusesLayout&) const override;
 juce::AudioProcessorEditor* createEditor() override;
 bool hasEditor() const override {return true;}
 const juce::String getName() const override {return "INVERSO ABISMO SAMPLER";}
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

 // Message thread. Returns false (and keeps the current sample) on failure.
 bool loadSampleFile(const juce::File&);
 void useBuiltInSample();
 // Releases a replaced sample on the message thread; the timer also calls it.
 void collectGarbage();
 std::shared_ptr<const SampleData> getDisplaySample() const;
 juce::String getSamplePath() const;
 juce::String getMissingPath() const;
 bool isAudioFile(const juce::String& path) const;

 juce::AudioProcessorValueTreeState apvts;
 std::atomic<float> outputPeak{0.f};
 // Knob-as-aftertouch learn: the editor arms it, the audio thread reports the first CC it sees.
 std::atomic<bool> learnPressure{false};
 std::atomic<int> learnedCC{-1};
 std::atomic<float> playhead{-1.f};
 std::atomic<int> voicesSounding{0};
 std::atomic<int> sampleVersion{0};
 static juce::AudioProcessorValueTreeState::ParameterLayout makeParameters();
private:
 void timerCallback() override {collectGarbage();}
 void queueSample(std::shared_ptr<SampleData>,const juce::String& path);
 inverso_abismo::sampler::Engine engine;
 std::array<std::atomic<float>*,inverso_abismo::sampler::P::Count> raw{};
 juce::AudioFormatManager formats;
 // Audio thread owns `current`; `pending` and `retired` cross threads under swapLock.
 std::shared_ptr<SampleData> current,pending,retired;
 std::shared_ptr<const SampleData> display;
 juce::SpinLock swapLock;
 mutable juce::CriticalSection pathLock;
 juce::String samplePath,missingPath;
 int currentProgram=0;
 std::atomic<bool> prepared{false};
 JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(InversoAbismoSamplerProcessor)
};
