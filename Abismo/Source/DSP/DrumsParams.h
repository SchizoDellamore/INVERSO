#pragma once
// ABISMO DRUMS parameter table, junk-object models and factory kits.
// 16 pads (MIDI 36-51) x 9 parameters, then the global and saw parameters.
// IDs are saved in sessions: never rename; only append.
#include "AbismoParams.h"
#include <algorithm>
#include <cstdio>
#include <string>
#include <vector>

namespace inverso_abismo::drums {

constexpr int numPads = 16;
constexpr int firstPadNote = 36;   // C1 (Cubase / Studio One naming)
constexpr int firstSawNote = 60;   // C3 and up: melodic musical saw

// How an object's vibration modes are spread: the physics of its shape.
enum class Shape { Membrane, Plate, Bar, Shell, Wood, Irregular };

// One struck / shaken object. Frequencies in Hz at TUNE 0.
struct ObjectModel {
 const char* name;
 Shape shape;
 float f0;              // lowest mode
 int modes;             // dense banks: 12-44 modes
 float t60;             // decay of the lowest mode (s); higher modes die faster
 float dampPow;         // how fast decay shortens with frequency (metal ~0.8, wood ~1.3)
 float tilt;            // spectral tilt of mode amplitudes (higher = darker)
 float contactMs;       // stick / hand contact time: longer = softer, darker hit
 int pieces;            // separate parts struck at random (cutlery, chain links): 1-3
 int impacts;           // 1 = single hit; >1 = a shaken / dropped bundle
 float impactRate, impactDecel;
 int bounces; float bounceMs; // the object jumping back onto the stick / floor
 float membrane, memBase, memStart, memDropMs, memDecay; // pitched head (drums only)
 float looseF;          // loose part / wires that rattle against the body
 float rattle;          // default RATTLE for pads that start with this object
 float gain;            // level calibration, measured: full-velocity hit peaks near -6 dBFS
                        // (-9 to -11 dBFS for cutlery, chain and long-ringing metal)
};

inline const std::array<ObjectModel, 16>& objects() {
 using S = Shape;
 static const std::array<ObjectModel, 16> table{{
  //  name                 shape        f0   modes t60  damp tilt contact pcs imp rate decel bnc bMs  memb base start drop decay looseF rattle gain
  {"Tambor de Oleo",       S::Shell,    95,  24, .45f,.9f, .8f, 3.5f, 1, 1, 0,  1,    0, 0,   1.f, 48, 2.2f, 35, .4f,  900, 15, 0.63f},
  {"Colheres",             S::Bar,    1300,  16, .25f,1.f, .6f, .6f,  2, 2, 22, 1,    0, 0,   0,   1,  1,    1,  0,  2600,  0, 5.1f},
  {"Caixa de Fanfarra",    S::Membrane,190,  20, .2f, .8f, .5f, 1.2f, 1, 1, 0,  1,    0, 0,  .5f, 190, 1.3f, 12, .12f,3200, 75, 0.67f},
  {"Porta de Garagem",     S::Plate,    70,  30, .5f, 1.1f,1.f, 6.f,  1, 1, 0,  1,    0, 0,  .6f,  50, 1.4f, 30, .15f, 600, 50, 0.74f},
  {"Tampa de Lixo",        S::Plate,   260,  36, .9f, .9f, .7f, 1.5f, 1, 1, 0,  1,    1, 40,  0,   1,  1,    1,  0,  1500, 45, 6.25f},
  {"Caixote",              S::Wood,    110,  14, .22f,1.3f,.9f, 3.f,  1, 1, 0,  1,    0, 0,  .4f,  85, 1.3f, 20, .12f, 800, 15, 0.9f},
  {"Talheres",             S::Bar,    1900,  24, .3f, 1.1f,.75f,.6f,  3, 3, 60, 1,    0, 0,   0,   1,  1,    1,  0,  3000,  0, 7.f},
  {"Panela",               S::Plate,   210,  28, .7f, .9f, .8f, 2.f,  1, 1, 0,  1,    0, 0,   0,   1,  1,    1,  0,  1200, 20, 7.1f},
  {"Corrente",             S::Irregular,1400,24, .12f,1.2f,.7f, .4f,  3, 9, 70, 1.1f, 0, 0,   0,   1,  1,    1,  0,  2200, 10, 1.42f},
  {"Cano de Ferro",        S::Bar,     280,  18, 1.1f,.8f, .7f, 1.f,  1, 1, 0,  1,    0, 0,   0,   1,  1,    1,  0,  1800,  0, 8.8f},
  {"Mola de Cama",         S::Irregular,300, 16, .5f, 1.f, .8f, 1.5f, 1, 1, 0,  1,    0, 0,  .7f, 120, 5.f,  90, .5f, 1800, 50, 0.59f},
  {"Lata de Lixo",         S::Shell,   160,  30, .6f, .9f, .8f, 2.5f, 1, 1, 0,  1,    0, 0,  .5f,  70, 1.8f, 25, .25f,1400, 30, 0.77f},
  {"Chapa de Metal",       S::Plate,    45,  40, 1.8f,.7f, .9f, 8.f,  1, 1, 0,  1,    0, 0,   0,   1,  1,    1,  0,   700, 20, 5.8f},
  {"Prato Rachado",        S::Plate,   420,  44, 1.6f,.75f,.55f,.8f,  1, 1, 0,  1,    0, 0,   0,   1,  1,    1,  0,  2500, 70, 6.5f},
  {"Bigorna",              S::Bar,     900,  16, .9f, 1.1f,.8f, .4f,  1, 1, 0,  1,    2, 70,  0,   1,  1,    1,  0,  2000,  0, 6.8f},
  {"Garrafa",              S::Shell,   700,  12, .5f, 1.1f,1.f, .8f,  1, 1, 0,  1,    0, 0,   0,   1,  1,    1,  0,  2000,  0, 6.7f},
 }};
 return table;
}

// Default object per pad, laid out like General MIDI drums so DAW drum maps
// make sense: C1 kick (oil drum), D1 snare (parade drum), F#1 closed hat
// (cutlery), G#1 pedal hat (chain), C#2 crash (cracked cymbal)...
inline int defaultObject(int pad) { return std::clamp(pad, 0, 15); }

namespace P {
// Per-pad parameter offsets: index = pad * PadCount + offset.
enum : int { Object, Tune, Decay, Tone, Rattle, Rust, Variation, Level, Pan, PadCount };
constexpr int padBase(int pad) { return pad * PadCount; }
enum : int {
 Stumble = numPads * PadCount, StutterDiv, StutterDepth, Chaos, Crush,
 Megaphone, Gramophone, RoomMix, RoomSize, RoomDecay, RoomDamp,
 SawLevel, SawAttack, SawRelease, SawVibrato, SawVibRate, SawGlide, SawTone, SawWobble,
 VelSens, Output,
 Count
};
}

inline const std::array<ParamSpec, P::Count>& specs() {
 static std::vector<std::string> storage; // keeps generated ids/names alive
 static const std::array<ParamSpec, P::Count> table = [] {
  std::array<ParamSpec, P::Count> t{};
  storage.reserve(numPads * P::PadCount * 2);
  static std::string objectChoices;
  for (std::size_t i = 0; i < objects().size(); ++i) objectChoices += (i ? "|" : "") + std::string(objects()[i].name);
  auto keep = [](std::string s) -> const char* { storage.push_back(std::move(s)); return storage.back().c_str(); };
  for (int pad = 0; pad < numPads; ++pad) {
   char prefix[8];
   std::snprintf(prefix, sizeof prefix, "P%02d_", pad + 1);
   const std::string id(prefix), name = "Pad " + std::to_string(pad + 1) + " ";
   const float defaultPan = static_cast<float>(((pad * 7) % 9) - 4) * 9.f; // gentle spread, -36..+36
   const int b = P::padBase(pad);
   t[static_cast<std::size_t>(b + P::Object)] = {keep(id + "OBJECT"), keep(name + "object"), 0, 15, static_cast<float>(defaultObject(pad)), 0, 1, Kind::Choice, objectChoices.c_str(), ""};
   t[static_cast<std::size_t>(b + P::Tune)] = {keep(id + "TUNE"), keep(name + "tune"), -24, 24, 0, 0, .01f, Kind::Float, nullptr, "st"};
   t[static_cast<std::size_t>(b + P::Decay)] = {keep(id + "DECAY"), keep(name + "decay"), 10, 400, 100, 100, .1f, Kind::Float, nullptr, "%"};
   t[static_cast<std::size_t>(b + P::Tone)] = {keep(id + "TONE"), keep(name + "tone"), 0, 100, 50, 0, .01f, Kind::Float, nullptr, "%"};
   t[static_cast<std::size_t>(b + P::Rattle)] = {keep(id + "RATTLE"), keep(name + "rattle"), 0, 100, objects()[static_cast<std::size_t>(defaultObject(pad))].rattle, 0, .01f, Kind::Float, nullptr, "%"};
   t[static_cast<std::size_t>(b + P::Rust)] = {keep(id + "RUST"), keep(name + "rust"), 0, 100, 20, 0, .01f, Kind::Float, nullptr, "%"};
   t[static_cast<std::size_t>(b + P::Variation)] = {keep(id + "VARIATION"), keep(name + "variation"), 0, 100, 35, 0, .01f, Kind::Float, nullptr, "%"};
   t[static_cast<std::size_t>(b + P::Level)] = {keep(id + "LEVEL"), keep(name + "level"), -36, 6, 0, 0, .01f, Kind::Float, nullptr, "dB"};
   t[static_cast<std::size_t>(b + P::Pan)] = {keep(id + "PAN"), keep(name + "pan"), -100, 100, defaultPan, 0, .01f, Kind::Float, nullptr, "pan"};
  }
  auto set = [&](int i, ParamSpec s) { t[static_cast<std::size_t>(i)] = s; };
  set(P::Stumble, {"STUMBLE","Stumble",0,100,10,0,.01f,Kind::Float,nullptr,"%"});
  set(P::StutterDiv, {"STUTTER_DIV","Stutter",0,6,0,0,1,Kind::Choice,"Off|1/4|1/8|1/16|1/32|1/8T|1/16T",""});
  set(P::StutterDepth, {"STUTTER_DEPTH","Stutter depth",0,100,60,0,.01f,Kind::Float,nullptr,"%"});
  set(P::Chaos, {"CHAOS","Stutter chaos",0,100,40,0,.01f,Kind::Float,nullptr,"%"});
  set(P::Crush, {"CRUSH","Bit crush",0,100,0,0,.01f,Kind::Float,nullptr,"%"});
  set(P::Megaphone, {"MEGAPHONE","Megaphone",0,100,0,0,.01f,Kind::Float,nullptr,"%"});
  set(P::Gramophone, {"GRAMOPHONE","Gramophone",0,100,0,0,.01f,Kind::Float,nullptr,"%"});
  set(P::RoomMix, {"ROOM_MIX","Room mix",0,100,22,0,.01f,Kind::Float,nullptr,"%"});
  set(P::RoomSize, {"ROOM_SIZE","Room size",0,100,35,0,.01f,Kind::Float,nullptr,"%"});
  set(P::RoomDecay, {"ROOM_DECAY","Room decay",.2f,8,1.2f,1.5f,.01f,Kind::Float,nullptr,"s"});
  set(P::RoomDamp, {"ROOM_DAMP","Room darkness",0,100,50,0,.01f,Kind::Float,nullptr,"%"});
  set(P::SawLevel, {"SAW_LEVEL","Saw level",0,100,70,0,.01f,Kind::Float,nullptr,"%"});
  set(P::SawAttack, {"SAW_ATTACK","Saw bow attack",5,3000,300,300,.1f,Kind::Float,nullptr,"ms"});
  set(P::SawRelease, {"SAW_RELEASE","Saw release",20,5000,800,800,.1f,Kind::Float,nullptr,"ms"});
  set(P::SawVibrato, {"SAW_VIBRATO","Saw vibrato",0,100,35,0,.01f,Kind::Float,nullptr,"%"});
  set(P::SawVibRate, {"SAW_VIB_RATE","Saw vibrato rate",1,10,5.5f,0,.01f,Kind::Float,nullptr,"Hz"});
  set(P::SawGlide, {"SAW_GLIDE","Saw glide",0,2000,180,250,.1f,Kind::Float,nullptr,"ms"});
  set(P::SawTone, {"SAW_TONE","Saw bow noise",0,100,30,0,.01f,Kind::Float,nullptr,"%"});
  set(P::SawWobble, {"SAW_WOBBLE","Saw wobble",0,100,30,0,.01f,Kind::Float,nullptr,"%"});
  set(P::VelSens, {"VEL_SENS","Velocity sensitivity",0,100,70,0,.01f,Kind::Float,nullptr,"%"});
  set(P::Output, {"OUTPUT","Output",-36,6,-3,0,.01f,Kind::Float,nullptr,"dB"});
  return t;
 }();
 return table;
}

using Values = std::array<float, P::Count>;

inline Values defaults() {
 Values v{};
 for (std::size_t i = 0; i < v.size(); ++i) v[i] = specs()[i].def;
 return v;
}

// Kits: per-pad overrides written as {pad, offset, value}; globals use pad -1.
struct KitValue { int pad, param; float value; };
struct Kit { const char* name; KitValue values[64]; };

inline const std::array<Kit, 6>& kits() {
 using namespace P;
 static const std::array<Kit, 6> table{{
  {"Ferro-Velho", {{-1, -1, 0}}},
  {"Galpao Abandonado", {{-1,RoomMix,45},{-1,RoomSize,85},{-1,RoomDecay,3.5f},{-1,RoomDamp,65},{-1,Stumble,20},
   {0,Tune,-3},{0,Decay,140},{3,Decay,150},{12,Decay,160},{13,Decay,130},{-1,Output,-3},{-1,-1,0}}},
  {"Fanfarra Bebada", {{-1,Stumble,60},{-1,Gramophone,35},{-1,RoomMix,25},{2,Rattle,85},{2,Tune,2},
   {5,Object,2},{5,Tune,-7},{7,Object,2},{7,Tune,-4},{9,Object,2},{9,Tune,-10},{11,Object,2},{11,Tune,-12},{-1,Output,-2},{-1,-1,0}}},
  {"Cozinha Assombrada", {{0,Object,5},{0,Tune,-6},{2,Object,4},{2,Tune,3},{4,Object,7},{4,Tune,4},{5,Object,7},{5,Tune,-5},
   {7,Object,7},{9,Object,15},{9,Tune,-3},{11,Object,15},{11,Tune,4},{-1,RoomMix,28},{-1,RoomSize,25},{-1,Stumble,15},{-1,Output,-3},{-1,-1,0}}},
  {"Oficina de Graxa", {{-1,StutterDiv,3},{-1,StutterDepth,35},{-1,Chaos,45},{-1,Crush,12},{-1,Stumble,25},
   {3,Object,12},{4,Object,9},{4,Tune,5},{6,Object,8},{6,Tune,4},{10,Object,14},{10,Tune,-5},{13,Object,12},{13,Tune,7},
   {-1,RoomMix,18},{-1,RoomSize,40},{-1,Output,-3},{-1,-1,0}}},
  {"Radio de Valvula", {{-1,Megaphone,70},{-1,Gramophone,60},{-1,RoomMix,12},{-1,RoomSize,20},{-1,Stumble,25},
   {-1,SawVibrato,55},{-1,Output,-4},{-1,-1,0}}},
 }};
 return table;
}

inline Values kitValues(std::size_t index) {
 Values v = defaults();
 if (index >= kits().size()) return v;
 for (const auto& kv : kits()[index].values) {
  if (kv.param < 0) break;
  const int i = kv.pad < 0 ? kv.param : P::padBase(kv.pad) + kv.param;
  v[static_cast<std::size_t>(i)] = kv.value;
 }
 return v;
}

} // namespace inverso_abismo::drums
