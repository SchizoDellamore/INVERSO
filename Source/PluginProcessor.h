#pragma once

#include <JuceHeader.h>
#include "DSP/EQCore.h"
#include <array>
#include <atomic>

class InversoEQAudioProcessor : public juce::AudioProcessor {
public:
    InversoEQAudioProcessor();
    ~InversoEQAudioProcessor() override = default;

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    bool isBusesLayoutSupported(const BusesLayout&) const override;
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int, const juce::String&) override {}
    void getStateInformation(juce::MemoryBlock&) override;
    void setStateInformation(const void*, int) override;

    juce::AudioProcessorValueTreeState state;
    inverso::Settings currentSettings() const;
    void spectrumSnapshot(std::array<float, 4096>& before,
                          std::array<float, 4096>& after) const noexcept;

    std::atomic<float> lastInputPeak {0.f}, lastPreProtectionPeak {0.f};
    std::atomic<float> lastOutputPeak {0.f}, lastReductionDb {0.f};
    std::atomic<float> lastPreTruePeak {0.f}, lastOutputTruePeak {0.f};

    static juce::AudioProcessorValueTreeState::ParameterLayout createParameters();
    void setSpectrumCaptureEnabled(bool enabled) noexcept { spectrumCaptureEnabled.store(enabled, std::memory_order_relaxed); }

private:
    inverso::EQCore eq;
    static constexpr std::size_t ringSize = 8192;
    std::array<std::atomic<float>, ringSize> inputRing {}, outputRing {};
    std::atomic<bool> spectrumCaptureEnabled {false};
    std::atomic<std::uint64_t> publishedSampleCount {0};
    std::uint64_t sampleCount = 0; // accessed only by the audio thread

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(InversoEQAudioProcessor)
};
