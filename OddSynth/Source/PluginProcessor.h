#pragma once
#include <JuceHeader.h>
#include "OddCore.h"
#include <atomic>
#include <memory>

class OddProcessor final : public juce::AudioProcessor {
public:
    OddProcessor();
    ~OddProcessor() override = default;
    const juce::String getName() const override { return "INVERSO OddSynth"; }
    void prepareToPlay(double, int) override;
    void releaseResources() override {}
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    bool isBusesLayoutSupported(const BusesLayout&) const override;
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 15; }
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int, const juce::String&) override {}
    void getStateInformation(juce::MemoryBlock&) override;
    void setStateInformation(const void*, int) override;

    juce::UndoManager undoManager{128000, 40};
    juce::AudioProcessorValueTreeState state;
    juce::MidiKeyboardState keyboard;
    std::array<std::atomic<float>,odd::dimensions> live{};
    std::atomic<float> peak{0};
    std::atomic<int> voices{0};
    static const std::array<juce::String,odd::dimensions> ids;
    static const std::array<juce::String,odd::dimensions> names;
    static juce::AudioProcessorValueTreeState::ParameterLayout layout();
    float value(const juce::String&) const;
    void setValue(const juce::String&, float);
    void randomise();
    void restart();
    void panic();
    void captureSlot(int);
    bool recallSlot(int);
    bool hasSlot(int) const;
    bool morph(float);
    void factoryPreset(int);
    bool loadSample(const juce::File&, juce::String& error);
    void sampleEdit(int action); // reverse, crop, erase, fade-in, fade-out, half speed, double speed
    std::shared_ptr<const odd::Sample> sampleCopy() const;
    juce::String sampleName() const;
    bool exportSample(const juce::File&);
    bool exportCapture(const juce::File&);
    bool savePreset(const juce::File&);
    bool loadPreset(const juce::File&);
private:
    class SampleAction;
    void swapSample(std::shared_ptr<const odd::Sample>, const juce::String&);
    juce::ValueTree makeState(bool withSlots);
    bool applyState(const juce::ValueTree&, bool withSlots);
    static juce::String encodeSample(const odd::Sample&);
    static std::shared_ptr<const odd::Sample> decodeSample(const juce::String&);
    odd::Settings settings() const;
    void midi(const juce::MidiMessage&, const odd::Settings&);
    odd::Engine engine;
    std::shared_ptr<const odd::Sample> currentSample;
    juce::String currentSampleName;
    juce::ValueTree slotA, slotB;
    juce::Reverb reverb;
    std::vector<float> delayL,delayR;
    size_t delayWrite=0;
    double sr=48000;
    bool wasPlaying=false;
    float wheel=0, expression=1;
    std::atomic<bool> restartRequest{false},panicRequest{false};
    struct CaptureBuffer {
        explicit CaptureBuffer(double rate):sampleRate(rate),frames(static_cast<size_t>(rate*25)),audio(std::make_unique<std::atomic<float>[]>(frames*2)) { for(size_t i=0;i<frames*2;++i)audio[i].store(0,std::memory_order_relaxed); }
        double sampleRate;size_t frames;
        std::unique_ptr<std::atomic<float>[]> audio;
        std::atomic<uint64_t> written{0};
    };
    std::shared_ptr<CaptureBuffer> capture;
    juce::AudioBuffer<float> effectBuffer;
    std::array<float,2> dcIn{},dcOut{};
    std::array<float,8> smoothLevels{};
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(OddProcessor)
};
