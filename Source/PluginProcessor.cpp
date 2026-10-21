#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace {
using namespace juce;
std::unique_ptr<RangedAudioParameter> boolean(const String& id, const String& label, bool init) {
    return std::make_unique<AudioParameterBool>(ParameterID {id, 1}, label, init);
}
std::unique_ptr<RangedAudioParameter> continuous(const String& id, const String& label,
                                                  float lo, float hi, float init, float skew = 1.f) {
    return std::make_unique<AudioParameterFloat>(ParameterID {id, 1}, label,
         NormalisableRange<float>(lo, hi, 0.f, skew), init);
}
std::unique_ptr<RangedAudioParameter> slope(const String& id, const String& label, int defaultIndex) {
    return std::make_unique<AudioParameterChoice>(ParameterID {id, 1}, label,
                                                  StringArray {"12 dB/oct", "24 dB/oct", "48 dB/oct"}, defaultIndex);
}
float parameter(const juce::AudioProcessorValueTreeState& state, const juce::String& id) {
    if (const auto* p = state.getRawParameterValue(id)) return p->load();
    return 0.f;
}
}

juce::AudioProcessorValueTreeState::ParameterLayout InversoEQAudioProcessor::createParameters() {
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> p;
    p.push_back(boolean("hp_on", "High-pass", false));
    p.push_back(continuous("hp_hz", "High-pass cutoff", 20.f, 2000.f, 35.f, 0.32f));
    p.push_back(slope("hp_slope", "High-pass slope", 1));
    p.push_back(boolean("lp_on", "Low-pass", false));
    p.push_back(continuous("lp_hz", "Low-pass cutoff", 500.f, 22000.f, 18000.f, 0.65f));
    p.push_back(slope("lp_slope", "Low-pass slope", 0));

    constexpr float frequencies[inverso::kBands] = {70, 145, 320, 700, 1500, 3500, 7500, 13000};
    for (int i=0; i<inverso::kBands; ++i) {
        const auto n = juce::String(i + 1);
        p.push_back(boolean("b" + n + "_on", "Band " + n + " enabled", i == 3));
        p.push_back(continuous("b" + n + "_hz", "Band " + n + " frequency", 20.f, 20000.f, frequencies[i], 0.26f));
        p.push_back(continuous("b" + n + "_db", "Band " + n + " gain", -18.f, 18.f, 0.f));
        p.push_back(continuous("b" + n + "_q", "Band " + n + " Q", 0.2f, 18.f, 1.f, 0.32f));
    }
    p.push_back(continuous("out_db", "Output gain", -24.f, 12.f, 0.f));
    p.push_back(boolean("safe_on", "Oversampled output protection", true));
    p.push_back(continuous("ceiling_db", "Approximate true-peak ceiling", -12.f, 0.f, -1.f));
    return {p.begin(), p.end()};
}

InversoEQAudioProcessor::InversoEQAudioProcessor()
    : AudioProcessor(BusesProperties()
          .withInput("Input", juce::AudioChannelSet::stereo(), true)
          .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      state(*this, nullptr, "PARAMETERS", createParameters()) {}

void InversoEQAudioProcessor::prepareToPlay(double rate, int) {
    eq.prepare(rate, getTotalNumInputChannels());
    setLatencySamples(eq.latencySamples());
    sampleCount = 0;
    publishedSampleCount.store(0, std::memory_order_release);
    for (auto& a: inputRing) a.store(0.f, std::memory_order_relaxed);
    for (auto& a: outputRing) a.store(0.f, std::memory_order_relaxed);
}
void InversoEQAudioProcessor::releaseResources() { eq.reset(); }

bool InversoEQAudioProcessor::isBusesLayoutSupported(const BusesLayout& l) const {
    const auto mainIn = l.getMainInputChannelSet();
    const auto mainOut = l.getMainOutputChannelSet();
    return (mainIn == juce::AudioChannelSet::mono() || mainIn == juce::AudioChannelSet::stereo())
            && mainIn == mainOut;
}

inverso::Settings InversoEQAudioProcessor::currentSettings() const {
    inverso::Settings s;
    s.highPassEnabled = parameter(state, "hp_on") > 0.5f;
    s.highPassHz = parameter(state, "hp_hz");
    const int slopeValues[3] = {12, 24, 48};
    s.highPassSlope = slopeValues[juce::jlimit(0, 2, static_cast<int>(parameter(state, "hp_slope")))];
    s.lowPassEnabled = parameter(state, "lp_on") > 0.5f;
    s.lowPassHz = parameter(state, "lp_hz");
    s.lowPassSlope = slopeValues[juce::jlimit(0, 2, static_cast<int>(parameter(state, "lp_slope")))];
    for (int i = 0; i < inverso::kBands; ++i) {
        const auto prefix = "b" + juce::String(i+1);
        auto& b = s.bands[static_cast<std::size_t>(i)];
        b.enabled = parameter(state, prefix + "_on") > 0.5f;
        b.frequency = parameter(state, prefix + "_hz");
        b.gainDb = parameter(state, prefix + "_db");
        b.q = parameter(state, prefix + "_q");
    }
    s.outputGainDb = parameter(state, "out_db");
    s.protectionEnabled = parameter(state, "safe_on") > 0.5f;
    s.protectionCeilingDb = parameter(state, "ceiling_db");
    return s;
}

void InversoEQAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&) {
    juce::ScopedNoDenormals noDenormals;
    const auto count = buffer.getNumSamples();
    const auto ch = juce::jmin(buffer.getNumChannels(), getTotalNumInputChannels());
    for (int channel=ch; channel<buffer.getNumChannels(); ++channel)
        buffer.clear(channel, 0, count);
    if (ch < 1 || count < 1) return;

    eq.setSettings(currentSettings());
    const bool capture = spectrumCaptureEnabled.load(std::memory_order_relaxed);
    if (capture) {
        const float* input = buffer.getReadPointer(0);
        for (int n=0; n<count; ++n)
            inputRing[static_cast<std::size_t>((sampleCount + static_cast<std::uint64_t>(n)) % ringSize)]
                .store(input[n], std::memory_order_relaxed);
    }

    const auto meters = eq.process(buffer.getArrayOfWritePointers(), ch, count);
    if (capture) {
        const float* output = buffer.getReadPointer(0);
        for (int n=0; n<count; ++n)
            outputRing[static_cast<std::size_t>((sampleCount + static_cast<std::uint64_t>(n)) % ringSize)]
                .store(output[n], std::memory_order_relaxed);
        sampleCount += static_cast<std::uint64_t>(count);
        publishedSampleCount.store(sampleCount, std::memory_order_release);
    }
    lastInputPeak.store(meters.inputPeak, std::memory_order_relaxed);
    lastPreProtectionPeak.store(meters.preProtectionPeak, std::memory_order_relaxed);
    lastOutputPeak.store(meters.outputPeak, std::memory_order_relaxed);
    lastReductionDb.store(meters.maximumReductionDb, std::memory_order_relaxed);
    lastPreTruePeak.store(meters.preProtectionTruePeak, std::memory_order_relaxed);
    lastOutputTruePeak.store(meters.outputTruePeak, std::memory_order_relaxed);
}

void InversoEQAudioProcessor::spectrumSnapshot(std::array<float,4096>& before,
                                                std::array<float,4096>& after) const noexcept {
    const auto published = publishedSampleCount.load(std::memory_order_acquire);
    for (std::size_t i=0; i<before.size(); ++i) {
        const auto index = (published + ringSize - before.size() + i) % ringSize;
        before[i] = inputRing[index].load(std::memory_order_relaxed);
        after[i] = outputRing[index].load(std::memory_order_relaxed);
    }
}

juce::AudioProcessorEditor* InversoEQAudioProcessor::createEditor() {
    return new InversoEQAudioProcessorEditor(*this);
}
void InversoEQAudioProcessor::getStateInformation(juce::MemoryBlock& destination) {
    if (const auto xml = state.copyState().createXml()) copyXmlToBinary(*xml, destination);
}
void InversoEQAudioProcessor::setStateInformation(const void* data, int size) {
    if (const auto xml = getXmlFromBinary(data, size)) {
        if (xml->hasTagName(state.state.getType())) state.replaceState(juce::ValueTree::fromXml(*xml));
    }
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new InversoEQAudioProcessor(); }
