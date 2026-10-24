#pragma once
#include <JuceHeader.h>
#include "DSP/FurnaceAmp.h"
#include "DSP/FurnaceCab.h"
#include <array>
#include <atomic>

// FURNACE: guitar and bass amplifiers with the INVERSO CABS inside.
class InversoFurnaceProcessor final : public juce::AudioProcessor, private juce::Thread, private juce::AudioProcessorValueTreeState::Listener {
public:
 InversoFurnaceProcessor();
 ~InversoFurnaceProcessor() override;
 void prepareToPlay(double sampleRate,int samplesPerBlock) override;
 void releaseResources() override;
 void processBlock(juce::AudioBuffer<float>&,juce::MidiBuffer&) override;
 using AudioProcessor::processBlock;
 bool isBusesLayoutSupported(const BusesLayout&) const override;
 juce::AudioProcessorEditor* createEditor() override;
 bool hasEditor() const override {return true;}
 const juce::String getName() const override {return "INVERSO FURNACE";}
 bool acceptsMidi()const override{return false;}
 bool producesMidi()const override{return false;}
 bool isMidiEffect()const override{return false;}
 double getTailLengthSeconds()const override{return 3.0;}
 int getNumPrograms()override;
 int getCurrentProgram()override{return currentProgram;}
 void setCurrentProgram(int)override;
 const juce::String getProgramName(int)override;
 void changeProgramName(int,const juce::String&)override{}
 void getStateInformation(juce::MemoryBlock&) override;
 void setStateInformation(const void*,int) override;

 // A cabinet IR from a file (WAV, AIFF...). Selects CAB = User IR.
 bool loadUserIR(const juce::File&);
 juce::String userIRName() const;
 // The cabinet as currently designed, for the editor's screen (magnitude in dB).
 static double cabResponseDb(int cabChoice,int mic,double position,double axis,double distance,double f);

 juce::AudioProcessorValueTreeState apvts;
 std::atomic<float> inputPeak{0.f},outputPeak{0.f},gateLevel{1.f};
 static juce::AudioProcessorValueTreeState::ParameterLayout makeParameters();
private:
 void run() override;            // designs cabinets off the audio thread
 void parameterChanged(const juce::String&,float) override;
 void requestCab();

 inverso_abismo::furnace::Amp amp;
 juce::dsp::Convolution cab{juce::dsp::Convolution::NonUniform{512}};
 std::array<std::atomic<float>*,inverso_abismo::furnace::P::Count> raw{};
 juce::AudioBuffer<float> mono,diBuffer;
 juce::WaitableEvent wake;
 std::atomic<bool> cabDirty{true};
 std::atomic<double> sampleRateNow{48000.0};
 std::atomic<int> cabLoaded{-1}; // which CAB choice the convolver holds (-1 none yet)
 juce::File userIR;
 mutable juce::CriticalSection userIRLock;
 int currentProgram=0;
 int convBlock=512;
 bool prepared=false;
 JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(InversoFurnaceProcessor)
};
