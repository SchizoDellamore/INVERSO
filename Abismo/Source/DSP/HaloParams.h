#pragma once
// HALO: the reverbs of the 80s, gated reverb first. A plate, a hall, a room or
// the gated room (a big space cut short a moment after each hit, opened again
// by the next), with pre-delay, filters on what goes in, and the gate's own
// threshold, hold and release. Portable parameter table and presets (no JUCE).
#include "AbismoParams.h"
#include <array>
#include <cstddef>

namespace inverso_abismo::halo {

using inverso_abismo::Kind;
using inverso_abismo::ParamSpec;

namespace P {
enum : int {
 Mode, Mix, PreDelay, Size, Decay, Damp, LowCut, HighCut,
 GateThresh, GateHold, GateRelease, Width, Output,
 Count
};
}

// IDs are part of saved sessions: never rename or reorder meaning, only append.
inline const std::array<ParamSpec, P::Count>& specs() {
 static const std::array<ParamSpec, P::Count> table{{
  {"MODE","Reverb",0,3,3,0,1,Kind::Choice,"Plate|Hall|Room|Gated",""},
  {"MIX","Mix",0,100,30,0,.01f,Kind::Float,nullptr,"%"},
  {"PREDELAY","Pre-delay",0,250,15,40,.1f,Kind::Float,nullptr,"ms"},
  {"SIZE","Size",0,100,80,0,.01f,Kind::Float,nullptr,"%"},
  {"DECAY","Decay",.2f,20,4,3,.01f,Kind::Float,nullptr,"s"},
  {"DAMP","Darkness",0,100,35,0,.01f,Kind::Float,nullptr,"%"},
  {"LOWCUT","Low cut (into the reverb)",20,1000,120,150,.1f,Kind::Float,nullptr,"Hz"},
  {"HIGHCUT","High cut (into the reverb)",1000,20000,10000,6000,.1f,Kind::Float,nullptr,"Hz"},
  {"GATE_THRESH","Gate threshold",-60,0,-30,0,.1f,Kind::Float,nullptr,"thr"},
  {"GATE_HOLD","Gate hold",10,1000,250,200,.1f,Kind::Float,nullptr,"ms"},
  {"GATE_RELEASE","Gate release",5,500,40,60,.1f,Kind::Float,nullptr,"ms"},
  {"WIDTH","Width",0,200,100,0,.01f,Kind::Float,nullptr,"%"},
  {"OUTPUT","Output",-24,12,0,0,.01f,Kind::Float,nullptr,"dB"},
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
struct Preset { const char* name; PresetValue values[16]; };

// "Category/Name": the preset menu groups them.
inline const std::array<Preset, 14>& presets() {
 using namespace P;
 static const std::array<Preset, 14> table{{
  {"Gated/Snare 84", {{Mode, 3}, {Mix, 35}, {PreDelay, 8}, {Size, 85}, {Decay, 5}, {Damp, 30}, {LowCut, 200}, {GateThresh, -28}, {GateHold, 220}, {GateRelease, 30}, {-1, 0}}},
  {"Gated/Big Toms", {{Mode, 3}, {Mix, 40}, {PreDelay, 10}, {Size, 95}, {Decay, 7}, {Damp, 40}, {LowCut, 90}, {GateThresh, -30}, {GateHold, 380}, {GateRelease, 50}, {-1, 0}}},
  {"Gated/Tight Clap", {{Mode, 3}, {Mix, 30}, {PreDelay, 5}, {Size, 70}, {Decay, 3}, {Damp, 20}, {LowCut, 300}, {GateThresh, -26}, {GateHold, 140}, {GateRelease, 20}, {-1, 0}}},
  {"Gated/Chopped Pad", {{Mode, 3}, {Mix, 45}, {PreDelay, 20}, {Size, 90}, {Decay, 8}, {Damp, 35}, {LowCut, 150}, {GateThresh, -36}, {GateHold, 120}, {GateRelease, 25}, {-1, 0}}},
  {"Gated/Vocal Shout", {{Mode, 3}, {Mix, 25}, {PreDelay, 25}, {Size, 80}, {Decay, 4}, {Damp, 45}, {LowCut, 250}, {HighCut, 8000}, {GateThresh, -32}, {GateHold, 300}, {GateRelease, 60}, {-1, 0}}},
  {"Plate/Neon Plate", {{Mode, 0}, {Mix, 25}, {PreDelay, 20}, {Size, 70}, {Decay, 2.4f}, {Damp, 25}, {LowCut, 180}, {-1, 0}}},
  {"Plate/Vocal Plate", {{Mode, 0}, {Mix, 20}, {PreDelay, 60}, {Size, 60}, {Decay, 1.8f}, {Damp, 35}, {LowCut, 250}, {HighCut, 9000}, {-1, 0}}},
  {"Plate/Snare Plate", {{Mode, 0}, {Mix, 22}, {PreDelay, 5}, {Size, 55}, {Decay, 1.4f}, {Damp, 20}, {LowCut, 300}, {-1, 0}}},
  {"Hall/Night Hall", {{Mode, 1}, {Mix, 30}, {PreDelay, 30}, {Size, 85}, {Decay, 4.5f}, {Damp, 40}, {LowCut, 120}, {-1, 0}}},
  {"Hall/Endless Highway", {{Mode, 1}, {Mix, 40}, {PreDelay, 45}, {Size, 100}, {Decay, 12}, {Damp, 50}, {LowCut, 150}, {HighCut, 7000}, {-1, 0}}},
  {"Hall/Strings Hall", {{Mode, 1}, {Mix, 28}, {PreDelay, 25}, {Size, 80}, {Decay, 3.2f}, {Damp, 45}, {LowCut, 180}, {-1, 0}}},
  {"Room/Drum Room", {{Mode, 2}, {Mix, 22}, {PreDelay, 3}, {Size, 40}, {Decay, .9f}, {Damp, 30}, {LowCut, 100}, {-1, 0}}},
  {"Room/Small Booth", {{Mode, 2}, {Mix, 15}, {PreDelay, 2}, {Size, 25}, {Decay, .5f}, {Damp, 50}, {LowCut, 150}, {-1, 0}}},
  {"Room/Bright Chamber", {{Mode, 2}, {Mix, 25}, {PreDelay, 10}, {Size, 60}, {Decay, 1.6f}, {Damp, 15}, {LowCut, 200}, {-1, 0}}},
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

} // namespace inverso_abismo::halo
