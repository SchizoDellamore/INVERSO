#pragma once
// The "Plugin state restoration" check pluginval runs at strictness 10: put every parameter at a
// random raw value, save, put them at other random values, load, and every parameter must read
// back what it had when it was saved (within 0.1). Switches and choices at in-between raw values
// are what catch a plugin that only restores what it thinks has changed.
#include <JuceHeader.h>
#include <cstdio>
#include <random>

inline bool stateRoundTrip(juce::AudioProcessor& p, const char* name) {
 std::mt19937 rng(1234);
 std::uniform_real_distribution<float> any(0.f, 1.f);
 auto& params = p.getParameters();
 int bad = 0;
 for (int round = 0; round < 20; ++round) {
  for (auto* q : params) { q->setValue(any(rng)); q->sendValueChangedMessageToListeners(q->getValue()); } // as the VST3 wrapper does
  juce::MemoryBlock saved;
  p.getStateInformation(saved);
  std::vector<float> expected;
  // what the host reads back after saving: the legal (snapped) value of switches and choices
  for (auto* q : params) {
   auto* r = dynamic_cast<juce::RangedAudioParameter*>(q);
   expected.push_back(r ? r->convertTo0to1(r->convertFrom0to1(q->getValue())) : q->getValue());
  }
  for (auto* q : params) { q->setValue(any(rng)); q->sendValueChangedMessageToListeners(q->getValue()); } // as the VST3 wrapper does
  p.setStateInformation(saved.getData(), static_cast<int>(saved.getSize()));
  for (int i = 0; i < params.size(); ++i) {
   const float got = params[i]->getValue(), want = expected[static_cast<std::size_t>(i)];
   if (std::abs(got - want) > .1f) {
    if (bad < 8) std::printf("%s: %s not restored (wanted %.3f, got %.3f)\n", name, params[i]->getName(64).toRawUTF8(), want, got);
    ++bad;
   }
  }
 }
 if (bad == 0) std::printf("%s: state restoration (pluginval's check, 20 rounds) passed\n", name);
 return bad == 0;
}
