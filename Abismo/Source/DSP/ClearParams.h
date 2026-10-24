#pragma once
// CLEAR: takes the mud and the harshness out of a sound only when they are
// there. Portable parameter table and factory presets (no JUCE).
#include "AbismoParams.h"
#include <array>
#include <cstddef>

namespace inverso_abismo::clear {

using inverso_abismo::Kind;
using inverso_abismo::ParamSpec;

namespace P {
enum : int {
 MudDepth, MudFocus, MudSense,
 HarshDepth, HarshLow, HarshHigh, HarshSense, Sharpness,
 Speed, Mix, Output, Delta,
 Count
};
}

// IDs are part of saved sessions: never rename or reorder meaning, only append.
inline const std::array<ParamSpec, P::Count>& specs() {
 static const std::array<ParamSpec, P::Count> table{{
  {"MUD_DEPTH","Mud: most it takes away",0,12,4,0,.01f,Kind::Float,nullptr,"dB"},
  {"MUD_FOCUS","Mud: where",100,600,250,250,.1f,Kind::Float,nullptr,"Hz"},
  {"MUD_SENSE","Mud: sensitivity",0,100,50,0,.01f,Kind::Float,nullptr,"%"},
  {"HARSH_DEPTH","Harshness: most it takes away",0,12,5,0,.01f,Kind::Float,nullptr,"dB"},
  {"HARSH_LOW","Harshness: from",500,8000,1800,2000,.1f,Kind::Float,nullptr,"Hz"},
  {"HARSH_HIGH","Harshness: up to",2000,18000,10000,8000,.1f,Kind::Float,nullptr,"Hz"},
  {"HARSH_SENSE","Harshness: sensitivity",0,100,50,0,.01f,Kind::Float,nullptr,"%"},
  {"SHARPNESS","Resonance width (wide to narrow)",0,100,50,0,.01f,Kind::Float,nullptr,"%"},
  {"SPEED","Speed (slow to fast)",0,100,50,0,.01f,Kind::Float,nullptr,"%"},
  {"MIX","Mix",0,100,100,0,.01f,Kind::Float,nullptr,"%"},
  {"OUTPUT","Output",-12,12,0,0,.01f,Kind::Float,nullptr,"dB"},
  {"DELTA","Listen to what is taken away",0,1,0,0,1,Kind::Bool,nullptr,""},
 }};
 return table;
}

using Values = std::array<float, P::Count>;
inline Values defaults() {
 Values v{};
 for (std::size_t i = 0; i < v.size(); ++i) v[i] = specs()[i].def;
 return v;
}

struct PresetValue { int param; float value; };
struct Preset { const char* name; PresetValue values[12]; };

inline const std::array<Preset, 10>& presets() {
 using namespace P;
 static const std::array<Preset, 10> table{{
  {"Gentle", {{-1, 0}}},
  {"Harsh Guitars", {{MudDepth, 2}, {HarshDepth, 7}, {HarshLow, 1800}, {HarshHigh, 7000}, {HarshSense, 60}, {Sharpness, 45}, {-1, 0}}},
  {"Muddy Mix", {{MudDepth, 8}, {MudFocus, 280}, {MudSense, 75}, {HarshDepth, 0}, {-1, 0}}},
  {"Vocal Resonances", {{MudDepth, 3}, {MudFocus, 320}, {HarshDepth, 6}, {HarshLow, 900}, {HarshHigh, 9000}, {Sharpness, 70}, {Speed, 65}, {-1, 0}}},
  {"Sibilance", {{MudDepth, 0}, {HarshDepth, 8}, {HarshLow, 4500}, {HarshHigh, 12000}, {HarshSense, 65}, {Sharpness, 30}, {Speed, 80}, {-1, 0}}},
  {"Bass Clarity", {{MudDepth, 6}, {MudFocus, 220}, {MudSense, 55}, {HarshDepth, 0}, {Speed, 40}, {-1, 0}}},
  {"Cymbal Tamer", {{MudDepth, 0}, {HarshDepth, 6}, {HarshLow, 3000}, {HarshHigh, 16000}, {HarshSense, 55}, {Sharpness, 35}, {Speed, 70}, {-1, 0}}},
  {"Boxy Room", {{MudDepth, 5}, {MudFocus, 450}, {MudSense, 60}, {HarshDepth, 3}, {HarshLow, 700}, {HarshHigh, 3000}, {Sharpness, 75}, {-1, 0}}},
  {"Master Polish", {{MudDepth, 2.5f}, {MudSense, 40}, {HarshDepth, 3}, {HarshSense, 40}, {Sharpness, 55}, {Speed, 40}, {-1, 0}}},
  {"Hear What Goes", {{MudDepth, 6}, {HarshDepth, 8}, {Delta, 1}, {-1, 0}}},
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

} // namespace inverso_abismo::clear
