#pragma once
// After apvts.replaceState: put every parameter exactly where the loaded state says. JUCE only
// pushes a value it thinks has changed, and it compares the snapped value: a switch a host left
// at 0.39 counts as "off", so loading "off" would leave it at 0.39. pluginval catches exactly that.
#include <JuceHeader.h>
#include <cmath>

inline void restoreEveryParameter(juce::AudioProcessorValueTreeState& apvts) {
 for (auto* q : apvts.processor.getParameters())
  if (auto* r = dynamic_cast<juce::RangedAudioParameter*>(q))
   if (auto* stored = apvts.getRawParameterValue(r->paramID)) {
    const float normalised = r->convertTo0to1(stored->load());
    if (std::abs(r->getValue() - normalised) > 1e-6f) r->setValueNotifyingHost(normalised);
   }
}
