#pragma once
// Built-in sample so ABISMO SAMPLER is never silent before a file is loaded:
// a 5-second moaning vowel phrase rendered by the ABISMO synth engine itself.
#include "AbismoEngine.h"
#include "SamplerEngine.h"

namespace inverso_abismo::sampler {

inline SampleData makeDefaultSample(double sampleRate = 48000.0) {
 namespace S = ::inverso_abismo;
 S::Engine synth;
 auto v = S::presetValues(10); // "Coro Afogado"
 v[S::P::VoiceMix] = 85; v[S::P::Vowel] = 10; v[S::P::Attack] = 250; v[S::P::Release] = 900;
 v[S::P::RevMix] = 18; v[S::P::Output] = 2;
 synth.setValues(v);
 synth.prepare(sampleRate);
 SampleData d;
 d.sampleRate = sampleRate;
 d.name = "ABISMO voz interna";
 const int total = static_cast<int>(sampleRate * 5.0);
 d.left.assign(static_cast<std::size_t>(total), 0.f);
 d.right.assign(static_cast<std::size_t>(total), 0.f);
 // Note 60 is the root: the phrase moves around it and returns to it.
 const struct { double at; int note; bool on; } events[] = {
  {0.0, 60, true}, {1.4, 60, false}, {1.3, 61, true}, {2.6, 61, false}, {2.5, 55, true}, {3.9, 55, false},
  {3.7, 60, true}, {4.4, 60, false}};
 int position = 0;
 for (const auto& e : events) {
  const int at = std::min(total, static_cast<int>(e.at * sampleRate));
  if (at > position) {
   synth.render(d.left.data() + position, d.right.data() + position, at - position);
   position = at;
  }
  // Slow vowel movement inside the phrase: A -> O -> U.
  v[S::P::Vowel] = static_cast<float>(10.0 + 20.0 * e.at);
  synth.setValues(v);
  if (e.on) synth.noteOn(e.note, .8f); else synth.noteOff(e.note);
 }
 if (position < total) synth.render(d.left.data() + position, d.right.data() + position, total - position);
 // Normalise to -6 dBFS so the default sample sits like a typical recording.
 float peak = 0.f;
 for (std::size_t i = 0; i < d.left.size(); ++i) peak = std::max({peak, std::abs(d.left[i]), std::abs(d.right[i])});
 if (peak > 0.f) {
  const float g = .5f / peak;
  for (auto& x : d.left) x *= g;
  for (auto& x : d.right) x *= g;
 }
 return d;
}

} // namespace inverso_abismo::sampler
