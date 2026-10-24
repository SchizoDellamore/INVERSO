#pragma once
// TIDE: a dynamic EQ for instruments. Five bands (low shelf, three bells, high
// shelf), each fixed or dynamic: past its threshold a dynamic band cuts (or
// lifts) by up to its range. Plus a low cut and a high cut.
// Portable parameter table and factory presets (no JUCE).
#include "AbismoParams.h"
#include <array>
#include <cstddef>

namespace inverso_abismo::tide {

using inverso_abismo::Kind;
using inverso_abismo::ParamSpec;

constexpr int numBands = 5;

namespace P {
enum : int {
 // band k (0-based; IDs B1..B5) occupies [B1On + k * BandStride, ...)
 B1On, B1Freq, B1Gain, B1Q, B1Thresh, B1Range,
 B2On, B2Freq, B2Gain, B2Q, B2Thresh, B2Range,
 B3On, B3Freq, B3Gain, B3Q, B3Thresh, B3Range,
 B4On, B4Freq, B4Gain, B4Q, B4Thresh, B4Range,
 B5On, B5Freq, B5Gain, B5Q, B5Thresh, B5Range,
 LowCutOn, LowCut, HighCutOn, HighCut,
 Attack, Release, Output,
 Count
};
constexpr int BandStride = B2On - B1On;
enum Field : int { On, Freq, Gain, Q, Thresh, Range };
constexpr int band(int k, int field) { return B1On + k * BandStride + field; }
}

// IDs are part of saved sessions: never rename or reorder meaning, only append.
inline const std::array<ParamSpec, P::Count>& specs() {
 static const std::array<ParamSpec, P::Count> table{{
  {"B1_ON","Low shelf on",0,1,1,0,1,Kind::Bool,nullptr,""},
  {"B1_FREQ","Low shelf frequency",20,1000,120,120,.1f,Kind::Float,nullptr,"Hz"},
  {"B1_GAIN","Low shelf gain",-18,18,0,0,.01f,Kind::Float,nullptr,"dB"},
  {"B1_Q","Low shelf slope",.3f,2,.7f,.7f,.001f,Kind::Float,nullptr,"q"},
  {"B1_THRESH","Low shelf threshold",-60,0,-60,0,.1f,Kind::Float,nullptr,"thr"},
  {"B1_RANGE","Low shelf dynamic range",-18,18,0,0,.01f,Kind::Float,nullptr,"dB"},
  {"B2_ON","Band 2 on",0,1,1,0,1,Kind::Bool,nullptr,""},
  {"B2_FREQ","Band 2 frequency",20,20000,300,800,.1f,Kind::Float,nullptr,"Hz"},
  {"B2_GAIN","Band 2 gain",-18,18,0,0,.01f,Kind::Float,nullptr,"dB"},
  {"B2_Q","Band 2 width (Q)",.2f,12,1,1.5f,.001f,Kind::Float,nullptr,"q"},
  {"B2_THRESH","Band 2 threshold",-60,0,-60,0,.1f,Kind::Float,nullptr,"thr"},
  {"B2_RANGE","Band 2 dynamic range",-18,18,0,0,.01f,Kind::Float,nullptr,"dB"},
  {"B3_ON","Band 3 on",0,1,1,0,1,Kind::Bool,nullptr,""},
  {"B3_FREQ","Band 3 frequency",20,20000,1200,1200,.1f,Kind::Float,nullptr,"Hz"},
  {"B3_GAIN","Band 3 gain",-18,18,0,0,.01f,Kind::Float,nullptr,"dB"},
  {"B3_Q","Band 3 width (Q)",.2f,12,1,1.5f,.001f,Kind::Float,nullptr,"q"},
  {"B3_THRESH","Band 3 threshold",-60,0,-60,0,.1f,Kind::Float,nullptr,"thr"},
  {"B3_RANGE","Band 3 dynamic range",-18,18,0,0,.01f,Kind::Float,nullptr,"dB"},
  {"B4_ON","Band 4 on",0,1,1,0,1,Kind::Bool,nullptr,""},
  {"B4_FREQ","Band 4 frequency",20,20000,4000,2500,.1f,Kind::Float,nullptr,"Hz"},
  {"B4_GAIN","Band 4 gain",-18,18,0,0,.01f,Kind::Float,nullptr,"dB"},
  {"B4_Q","Band 4 width (Q)",.2f,12,1,1.5f,.001f,Kind::Float,nullptr,"q"},
  {"B4_THRESH","Band 4 threshold",-60,0,-60,0,.1f,Kind::Float,nullptr,"thr"},
  {"B4_RANGE","Band 4 dynamic range",-18,18,0,0,.01f,Kind::Float,nullptr,"dB"},
  {"B5_ON","High shelf on",0,1,1,0,1,Kind::Bool,nullptr,""},
  {"B5_FREQ","High shelf frequency",1000,20000,8000,6000,.1f,Kind::Float,nullptr,"Hz"},
  {"B5_GAIN","High shelf gain",-18,18,0,0,.01f,Kind::Float,nullptr,"dB"},
  {"B5_Q","High shelf slope",.3f,2,.7f,.7f,.001f,Kind::Float,nullptr,"q"},
  {"B5_THRESH","High shelf threshold",-60,0,-60,0,.1f,Kind::Float,nullptr,"thr"},
  {"B5_RANGE","High shelf dynamic range",-18,18,0,0,.01f,Kind::Float,nullptr,"dB"},
  {"LOWCUT_ON","Low cut on",0,1,0,0,1,Kind::Bool,nullptr,""},
  {"LOWCUT","Low cut",20,1000,60,100,.1f,Kind::Float,nullptr,"Hz"},
  {"HIGHCUT_ON","High cut on",0,1,0,0,1,Kind::Bool,nullptr,""},
  {"HIGHCUT","High cut",1000,20000,16000,8000,.1f,Kind::Float,nullptr,"Hz"},
  {"ATTACK","Attack",.5f,100,8,10,.01f,Kind::Float,nullptr,"ms"},
  {"RELEASE","Release",10,1000,120,150,.1f,Kind::Float,nullptr,"ms"},
  {"OUTPUT","Output",-18,18,0,0,.01f,Kind::Float,nullptr,"dB"},
 }};
 return table;
}

using Values = std::array<float, P::Count>;
inline Values defaults() {
 Values v{};
 for (std::size_t i = 0; i < v.size(); ++i) v[i] = specs()[i].def;
 return v;
}

// A band is dynamic when its range is not zero.
enum class BandShape { LowShelf, Bell, HighShelf };
inline BandShape shapeOf(int k) { return k == 0 ? BandShape::LowShelf : k == numBands - 1 ? BandShape::HighShelf : BandShape::Bell; }

struct PresetValue { int param; float value; };
struct Preset { const char* name; PresetValue values[24]; };

inline const std::array<Preset, 10>& presets() {
 using namespace P;
 static const std::array<Preset, 10> table{{
  {"Flat", {{-1, 0}}},
  // guitars
  {"Boomy Acoustic", {{B1Freq, 160}, {B1Thresh, -30}, {B1Range, -6}, {B2Freq, 220}, {B2Q, 1.4f}, {B2Thresh, -28}, {B2Range, -5},
                      {B4Freq, 3500}, {B4Gain, 1.5f}, {B4Q, .8f}, {LowCutOn, 1}, {LowCut, 70}, {-1, 0}}},
  {"Harsh Guitar Rider", {{B3Freq, 2600}, {B3Q, 2.5f}, {B3Thresh, -22}, {B3Range, -6}, {B4Freq, 4500}, {B4Q, 3}, {B4Thresh, -24}, {B4Range, -5},
                          {LowCutOn, 1}, {LowCut, 90}, {HighCutOn, 1}, {HighCut, 12000}, {-1, 0}}},
  {"Pick Attack", {{B4Freq, 3200}, {B4Q, 1.2f}, {B4Thresh, -24}, {B4Range, 5}, {B2Freq, 250}, {B2Q, 1.2f}, {B2Thresh, -26}, {B2Range, -3}, {Attack, 2}, {Release, 60}, {-1, 0}}},
  // bass
  {"Bass Note Evener", {{B1Freq, 90}, {B1Thresh, -22}, {B1Range, -6}, {B2Freq, 160}, {B2Q, 1.5f}, {B2Thresh, -24}, {B2Range, -4},
                        {B3Freq, 800}, {B3Gain, 1.5f}, {B3Q, .9f}, {LowCutOn, 1}, {LowCut, 35}, {Release, 200}, {-1, 0}}},
  {"Bass Growl", {{B3Freq, 900}, {B3Q, 1}, {B3Thresh, -26}, {B3Range, 6}, {B2Freq, 250}, {B2Q, 1.5f}, {B2Thresh, -24}, {B2Range, -4}, {Attack, 3}, {-1, 0}}},
  // voice
  {"Vocal De-Ess", {{B4Freq, 6500}, {B4Q, 3}, {B4Thresh, -30}, {B4Range, -9}, {B2Freq, 300}, {B2Q, 1.2f}, {B2Thresh, -26}, {B2Range, -3},
                    {LowCutOn, 1}, {LowCut, 90}, {Attack, 1}, {Release, 80}, {-1, 0}}},
  {"Vocal Presence", {{B4Freq, 3000}, {B4Gain, 2}, {B4Q, .8f}, {B5Freq, 10000}, {B5Gain, 2.5f}, {B5Thresh, -36}, {B5Range, -4},
                      {B2Freq, 350}, {B2Q, 1}, {B2Thresh, -26}, {B2Range, -4}, {LowCutOn, 1}, {LowCut, 100}, {-1, 0}}},
  // synths and keys
  {"Synth Resonance Tamer", {{B2Freq, 500}, {B2Q, 4}, {B2Thresh, -28}, {B2Range, -6}, {B3Freq, 1500}, {B3Q, 4}, {B3Thresh, -28}, {B3Range, -6},
                             {B4Freq, 3500}, {B4Q, 4}, {B4Thresh, -30}, {B4Range, -6}, {-1, 0}}},
  {"Bright When Loud", {{B5Freq, 5000}, {B5Thresh, -24}, {B5Range, 6}, {B1Freq, 150}, {B1Thresh, -24}, {B1Range, -3}, {Attack, 5}, {Release, 250}, {-1, 0}}},
 }};
 return table;
}

inline Values presetValues(std::size_t index) {
 Values v = defaults();
 if (index >= presets().size()) return v;
 for (const auto& pv : presets()[index].values) {
  if (pv.param < 0) break;
  v[static_cast<std::size_t>(pv.param)] = pv.value;
 }
 return v;
}

} // namespace inverso_abismo::tide
