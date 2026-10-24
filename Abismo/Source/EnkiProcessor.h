#pragma once
#include <JuceHeader.h>
#include "AbismoUI.h"
#include "DSP/DrumsEngine.h"
#include "DSP/EnkiGenerator.h"
#include <array>
#include <atomic>
#include <memory>
#include <vector>

class InversoEnkiProcessor final : public juce::AudioProcessor, private juce::Timer {
public:
 InversoEnkiProcessor();
 ~InversoEnkiProcessor() override;
 void prepareToPlay(double sampleRate,int samplesPerBlock) override;
 void releaseResources() override;
 void processBlock(juce::AudioBuffer<float>&,juce::MidiBuffer&) override;
 using AudioProcessor::processBlock;
 bool isBusesLayoutSupported(const BusesLayout&) const override;
 juce::AudioProcessorEditor* createEditor() override;
 bool hasEditor() const override {return true;}
 const juce::String getName() const override {return "INVERSO ENKI";}
 bool acceptsMidi()const override{return true;}
 bool producesMidi()const override{return false;}
 bool isMidiEffect()const override{return false;}
 double getTailLengthSeconds()const override{return 20.0;}
 int getNumPrograms()override{return 1;}
 int getCurrentProgram()override{return 0;}
 void setCurrentProgram(int)override{}
 const juce::String getProgramName(int)override{return "ENKI";}
 void changeProgramName(int,const juce::String&)override{}
 void getStateInformation(juce::MemoryBlock&) override;
 void setStateInformation(const void*,int) override;

 // ---- the generator (message thread)
 void generate();
 void mutate();
 bool back();
 bool forward();
 int historyPosition() const {return historyAt;}
 int historySize() const {return static_cast<int>(history.size());}
 // A plain-language list of what the current sound is made of.
 juce::String recipe() const;

 // ---- sources: a library recording (0-47) or a file from ENKI's sound library
 bool loadSourceFile(int source,const juce::File&); // also points the source at it
 void useLibrarySample(int source,int index);
 juce::String sourceSampleName(int source) const;
 std::shared_ptr<const inverso_abismo::enki::Sample> sourceDisplay(int source) const;
 // The top-level folder the generator draws files from ("" = all, "-" = none, recordings only).
 juce::String drawFolder;

 // ---- favourites: files in Documents/INVERSO ENKI/Favorites
 bool saveFavorite(const juce::String& name);
 bool loadFavorite(const juce::File&);
 static juce::Array<juce::File> favorites();

 static const inverso_abismo::drums::SampleLibrary& library();
 juce::AudioProcessorValueTreeState apvts;
 abismo_ui::MidiLearn midiLearn{*this}; // right-click any knob: MIDI learn
 std::atomic<float> outputPeak{0.f};
 std::atomic<int> voicesSounding{0};
 std::atomic<float> lfoNow{0.f};
 std::atomic<int> soundVersion{0}; // bumps whenever the sound changes as a whole
 static juce::AudioProcessorValueTreeState::ParameterLayout makeParameters();
private:
 using Sample=inverso_abismo::enki::Sample;
 using Values=inverso_abismo::enki::Values;
 struct Snapshot { Values values{}; juce::String fileA,fileB; };
 void timerCallback() override {collectGarbage();}
 void collectGarbage();
 Values current() const;
 void apply(const Snapshot&,bool addToHistory);
 void pushHistory(const Snapshot&);
 Snapshot snapshot() const;
 std::shared_ptr<Sample> decode(const juce::File&,double maxSeconds) const;
 void queueSlot(int source,std::shared_ptr<Sample>,const juce::String& path);
 inverso_abismo::enki::SamplePool poolWith(const std::shared_ptr<Sample>& a,const std::shared_ptr<Sample>& b) const;
 std::uint32_t readySlots() const;

 inverso_abismo::enki::Engine engine;
 inverso_abismo::enki::Generator generator;
 juce::AudioFormatManager formats;
 juce::Random random;
 std::vector<Sample> librarySamples;
 struct Slot { std::shared_ptr<Sample> current,pending,retired; bool hasPending=false; juce::String path; std::shared_ptr<const Sample> display; };
 std::array<Slot,2> slots;
 juce::SpinLock swapLock;
 mutable juce::CriticalSection pathLock;
 std::vector<Snapshot> history;
 int historyAt=-1;
 std::array<std::atomic<float>*,inverso_abismo::enki::P::Count> raw{};
 bool prepared=false;
 JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(InversoEnkiProcessor)
};
