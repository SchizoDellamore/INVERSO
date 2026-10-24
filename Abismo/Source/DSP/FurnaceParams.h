#pragma once
// FURNACE: guitar and bass amplifiers with the INVERSO CABS inside.
// Portable parameter table and factory presets (no JUCE).
#include "AbismoParams.h"
#include <array>
#include <cstddef>

namespace inverso_abismo::furnace {

using inverso_abismo::Kind;
using inverso_abismo::ParamSpec;

constexpr int numModels = 9;
inline const char* modelNames() { return "Clean US|Chime UK|Crunch UK|Lead UK|Modern|Fuzz|Bass Clean|Bass Grind|Dying Radio"; }
// Cabinet choices: Off, the 13 speakers of FurnaceCab.h, then a file of the user's.
constexpr int cabOff = 0;
constexpr int cabUser = 14;
inline const char* cabNames() {
 return "Off|Brit 4x12 V|Brit 4x12 G|Brit 2x12 Blue|Brit 1x12 Open|Amer 2x12 Open|Amer 1x12 Open|Amer 4x10 Open|Modern 4x12|"
        "Bass 8x10|Bass 1x15 Flip|Bass 4x10 Horn|Bass 2x12 Modern|Bass 1x18 Sub|User IR";
}

namespace P {
enum : int {
 Model, Gate, Tight, Gain, Bass, Mid, Treble, Presence, Master, Sag,
 Cab, Mic, Position, Axis, Distance,
 Spring, DIBlend, Output,
 Count
};
}

// IDs are part of saved sessions: never rename or reorder meaning, only append.
inline const std::array<ParamSpec, P::Count>& specs() {
 static const std::array<ParamSpec, P::Count> table{{
  {"MODEL","Amp model",0,numModels - 1,2,0,1,Kind::Choice,modelNames(),""},
  {"GATE","Noise gate",-90,-20,-90,0,.1f,Kind::Float,nullptr,"gate"},
  {"TIGHT","Tight",0,100,20,0,.01f,Kind::Float,nullptr,"%"},
  {"GAIN","Gain",0,10,5,0,.01f,Kind::Float,nullptr,"knob"},
  {"BASS","Bass",0,10,5,0,.01f,Kind::Float,nullptr,"knob"},
  {"MID","Middle",0,10,5,0,.01f,Kind::Float,nullptr,"knob"},
  {"TREBLE","Treble",0,10,5,0,.01f,Kind::Float,nullptr,"knob"},
  {"PRESENCE","Presence",0,10,5,0,.01f,Kind::Float,nullptr,"knob"},
  {"MASTER","Master",0,10,5,0,.01f,Kind::Float,nullptr,"knob"},
  {"SAG","Sag",0,100,40,0,.01f,Kind::Float,nullptr,"%"},
  {"CAB","Cabinet",0,cabUser,2,0,1,Kind::Choice,cabNames(),""},
  {"MIC","Microphone",0,3,0,0,1,Kind::Choice,"Dynamic 57|Dynamic 421|Condenser 87|Ribbon 121",""},
  {"MIC_POSITION","Mic position (centre to edge)",0,100,33,0,.01f,Kind::Float,nullptr,"%"},
  {"MIC_AXIS","Mic angle",0,100,0,0,.01f,Kind::Float,nullptr,"%"},
  {"MIC_DISTANCE","Mic distance",0,100,0,0,.01f,Kind::Float,nullptr,"%"},
  {"SPRING","Spring reverb",0,100,0,0,.01f,Kind::Float,nullptr,"%"},
  {"DI_BLEND","DI blend",0,100,0,0,.01f,Kind::Float,nullptr,"%"},
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
struct Preset { const char* name; PresetValue values[18]; };

// Each preset starts from the defaults and overrides what it lists (ends at param -1).
inline const std::array<Preset, 18>& presets() {
 using namespace P;
 static const std::array<Preset, 18> table{{
  {"Init", {{-1, 0}}},
  {"Clean Jangle", {{Model, 0}, {Gain, 3}, {Bass, 4}, {Mid, 4.5f}, {Treble, 6.5f}, {Cab, 5}, {Position, 20}, {Spring, 18}, {-1, 0}}},
  {"Surf Spring", {{Model, 0}, {Gain, 4}, {Bass, 4}, {Mid, 5}, {Treble, 7}, {Cab, 6}, {Position, 25}, {Spring, 60}, {-1, 0}}},
  {"Chime", {{Model, 1}, {Gain, 5.5f}, {Bass, 4}, {Treble, 6}, {Cab, 3}, {Position, 30}, {Spring, 10}, {-1, 0}}},
  {"Blues Edge", {{Model, 0}, {Gain, 7.5f}, {Mid, 6}, {Master, 7}, {Sag, 70}, {Cab, 7}, {Position, 40}, {-1, 0}}},
  {"Plexi Crunch", {{Model, 2}, {Gain, 6}, {Mid, 6.5f}, {Treble, 6}, {Cab, 2}, {Position, 35}, {-1, 0}}},
  {"Classic Rock", {{Model, 2}, {Gain, 8.5f}, {Mid, 7}, {Master, 6.5f}, {Cab, 2}, {Position, 30}, {Distance, 18}, {-1, 0}}},
  {"Hot Lead", {{Model, 3}, {Gain, 7}, {Mid, 7}, {Treble, 5.5f}, {Cab, 1}, {Position, 40}, {-1, 0}}},
  {"Modern Chug", {{Model, 4}, {Gain, 7}, {Tight, 70}, {Gate, -62}, {Bass, 6}, {Mid, 3.5f}, {Treble, 6}, {Cab, 8}, {Position, 30}, {-1, 0}}},
  {"Doom Fuzz", {{Model, 5}, {Gain, 9}, {Bass, 7}, {Mid, 3}, {Treble, 5}, {Cab, 2}, {Position, 55}, {Distance, 25}, {-1, 0}}},
  {"Crunch in the Room", {{Model, 2}, {Gain, 7}, {Mid, 6}, {Cab, 2}, {Position, 40}, {Distance, 100}, {-1, 0}}},
  {"Dark Ribbon Lead", {{Model, 3}, {Gain, 8}, {Mid, 6.5f}, {Treble, 6.5f}, {Cab, 1}, {Mic, 3}, {Position, 25}, {-1, 0}}},
  {"Bass Clean", {{Model, 6}, {Gain, 4}, {Bass, 6}, {Mid, 5}, {Treble, 5}, {Cab, 9}, {Mic, 1}, {Position, 25}, {DIBlend, 30}, {-1, 0}}},
  {"Bass Motown", {{Model, 6}, {Gain, 4.5f}, {Bass, 6.5f}, {Mid, 5.5f}, {Treble, 2.5f}, {Cab, 10}, {Mic, 1}, {Position, 40}, {-1, 0}}},
  {"Bass Grind", {{Model, 7}, {Gain, 7}, {Bass, 6}, {Mid, 6}, {Treble, 6}, {Cab, 11}, {Mic, 1}, {Position, 30}, {DIBlend, 40}, {-1, 0}}},
  {"Bass Modern", {{Model, 6}, {Gain, 5}, {Bass, 6.5f}, {Mid, 3.5f}, {Treble, 6.5f}, {Cab, 12}, {Mic, 1}, {Position, 20}, {DIBlend, 20}, {-1, 0}}},
  {"Dying Radio", {{Model, 8}, {Gain, 6}, {Mid, 6}, {Cab, 6}, {Position, 80}, {Spring, 20}, {-1, 0}}},
  {"Basement Fuzz Bass", {{Model, 5}, {Gain, 7}, {Bass, 8}, {Mid, 4}, {Treble, 4}, {Cab, 10}, {Mic, 1}, {Position, 50}, {DIBlend, 45}, {Output, 5}, {-1, 0}}},
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

} // namespace inverso_abismo::furnace
