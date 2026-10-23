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

// One struck / scraped / rattled object. Frequencies in Hz at TUNE 0.
struct ObjectModel {
 const char* name;
 int clicks;             // 1 = single strike; >1 = train (chain, ratchet, typewriter)
 float clickRate, clickDecel, clickJitter;
 float burstMs;          // strike length: short = hard mallet
 float baseFreq;         // membrane pitch (and mode reference unless modeBase set)
 float membrane, memStart, memDropMs, memDecay; // membrane level 0 = none
 float modeBase;         // first mode frequency
 int modes;
 float ratios[8], decays[8], amps[8];
 float noise, noiseDecay, noiseCutoff;
 float rattle;           // default RATTLE for pads that start with this object
};

inline const std::array<ObjectModel, 16>& objects() {
 static const std::array<ObjectModel, 16> table{{
  {"Lata de Lixo",1,0,1,0, 2.f, 52,1.f,3.f,40,.35f, 210,6,{1,1.47f,2.09f,2.56f,3.14f,3.9f},{.5f,.4f,.35f,.3f,.25f,.2f},{.35f,.3f,.25f,.2f,.18f,.15f}, .3f,.05f,3000, 30},
  {"Tambor de Fanfarra",1,0,1,0, 1.f, 185,.8f,1.6f,15,.18f, 185,5,{1,1.59f,2.14f,2.3f,2.65f},{.12f,.1f,.09f,.08f,.07f},{.3f,.25f,.2f,.18f,.15f}, 1.f,.22f,7000, 55},
  {"Mala-Bumbo",1,0,1,0, 3.f, 70,1.f,2.f,25,.25f, 175,4,{1,1.9f,2.8f,4.1f},{.08f,.07f,.06f,.05f},{.4f,.3f,.2f,.15f}, .25f,.03f,1500, 15},
  {"Freio de Carro",1,0,1,0, 1.f, 520,0,1,1,0, 520,7,{1,2.32f,2.92f,4.16f,5.4f,6.9f,8.2f},{2.5f,1.8f,1.5f,1.2f,1.f,.8f,.6f},{1,.8f,.7f,.6f,.5f,.4f,.3f}, .2f,.02f,9000, 5},
  {"Cano",1,0,1,0, 1.5f, 330,0,1,1,0, 330,4,{1,2.76f,5.4f,8.93f},{1.6f,1.f,.6f,.4f},{1,.5f,.3f,.2f}, .1f,.02f,6000, 0},
  {"Bigorna",1,0,1,0, .7f, 1150,0,1,1,0, 1150,5,{1,2.52f,4.18f,6.33f,8.7f},{3.f,2.2f,1.6f,1.f,.7f},{1,.7f,.5f,.4f,.3f}, .3f,.015f,12000, 0},
  {"Tampa de Panela",1,0,1,0, 1.f, 300,0,1,1,0, 300,8,{1,1.59f,2.14f,2.65f,3.16f,3.5f,4.15f,4.9f},{1.4f,1.2f,1.f,.9f,.8f,.7f,.6f,.5f},{1,.8f,.7f,.6f,.5f,.4f,.35f,.3f}, .3f,.1f,8000, 70},
  {"Corrente",14,35,1.f,.45f, .5f, 2400,0,1,1,0, 2400,4,{1,1.4f,2.3f,3.1f},{.12f,.1f,.08f,.06f},{1,.7f,.5f,.4f}, .2f,.3f,9000, 40},
  {"Garrafa",1,0,1,0, 1.f, 820,0,1,1,0, 820,4,{1,2.32f,4.25f,6.63f},{1.2f,.8f,.5f,.3f},{1,.5f,.3f,.15f}, .05f,.01f,8000, 0},
  {"Caixote",1,0,1,0, 2.f, 90,.5f,1.5f,20,.12f, 140,5,{1,1.58f,2.4f,3.6f,4.9f},{.18f,.12f,.09f,.06f,.05f},{.6f,.45f,.35f,.25f,.2f}, .4f,.04f,2500, 20},
  {"Ossos",2,28,1.f,.1f, .4f, 1600,0,1,1,0, 1600,3,{1,2.6f,4.8f},{.05f,.03f,.02f},{1,.5f,.3f}, .3f,.01f,6000, 0},
  {"Engrenagem",12,45,1.12f,.15f, .3f, 1900,0,1,1,0, 1900,3,{1,2.2f,3.7f},{.04f,.03f,.02f},{1,.6f,.4f}, .2f,.4f,5000, 10},
  {"Maquina de Escrever",5,14,1.f,.6f, .3f, 2900,0,1,1,0, 2900,4,{1,1.5f,2.7f,3.3f},{.03f,.025f,.02f,.015f},{1,.6f,.4f,.3f}, .5f,.3f,7000, 15},
  {"Mola",1,0,1,0, 1.f, 110,.8f,7.f,120,.7f, 240,3,{1,1.21f,1.53f},{.8f,.7f,.6f},{.3f,.25f,.2f}, .05f,.05f,4000, 50},
  {"Porta Batendo",1,0,1,0, 4.f, 55,1.f,1.4f,30,.2f, 110,3,{1,1.7f,2.9f},{.25f,.2f,.15f},{.5f,.4f,.3f}, .9f,.12f,1800, 45},
  {"Chocalho de Tampinhas",22,110,1.03f,.5f, .3f, 3300,0,1,1,0, 3300,4,{1,1.3f,1.7f,2.2f},{.02f,.02f,.015f,.015f},{1,.8f,.6f,.5f}, .6f,.25f,11000, 30},
 }};
 return table;
}

// Level calibration per object, measured so a full-velocity hit peaks near
// -7 dBFS (sharp, short objects) or -9 dBFS (long-ringing metal and glass).
inline float objectGain(int object) {
 static const float gain[16] = {.48f, .39f, .5f, 1.8f, 4.4f, 2.3f, 1.67f, 1.07f, 4.07f, 1.08f, .65f, .58f, .92f, .53f, .35f, .37f};
 return gain[std::clamp(object, 0, 15)];
}

// Default object per pad, laid out like General MIDI drums so DAW drum maps
// make sense: C1 kick, C#1 side stick, D1 snare, D#1 clap, F#1 closed hat...
inline int defaultObject(int pad) {
 static const int map[16] = {0, 10, 1, 14, 6, 9, 7, 2, 15, 4, 11, 8, 3, 5, 13, 12};
 return map[std::clamp(pad, 0, 15)];
}

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
   {0,Tune,-3},{0,Decay,160},{2,Decay,140},{3,Decay,180},{8,Rattle,40},{12,Decay,200},{13,Decay,220},{-1,Output,-2},{-1,-1,0}}},
  {"Fanfarra Bebada", {{-1,Stumble,65},{-1,Gramophone,35},{-1,RoomMix,25},{0,Object,2},{2,Rattle,80},{2,Tune,2},
   {4,Object,1},{4,Tune,-5},{5,Object,1},{5,Tune,-9},{7,Object,1},{7,Tune,-12},{1,Object,10},{-1,Output,0},{-1,-1,0}}},
  {"Cozinha Assombrada", {{0,Object,9},{0,Tune,-7},{2,Object,6},{2,Tune,5},{4,Object,8},{4,Tune,-2},{5,Object,8},{5,Tune,3},
   {7,Object,4},{7,Tune,7},{9,Object,6},{9,Tune,-5},{11,Object,8},{11,Tune,7},{-1,RoomMix,30},{-1,RoomSize,25},
   {-1,Megaphone,0},{-1,Stumble,15},{-1,Output,-7},{-1,-1,0}}},
  {"Oficina Quebrada", {{-1,StutterDiv,3},{-1,StutterDepth,40},{-1,Chaos,55},{-1,Crush,20},{-1,Stumble,30},
   {6,Object,11},{8,Object,12},{10,Object,7},{12,Object,5},{12,Tune,-5},{-1,RoomMix,15},{-1,Output,-4},{-1,-1,0}}},
  {"Radio de Valvula", {{-1,Megaphone,70},{-1,Gramophone,60},{-1,RoomMix,12},{-1,RoomSize,20},{-1,Stumble,25},
   {-1,SawVibrato,55},{-1,Output,-5},{-1,-1,0}}},
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
