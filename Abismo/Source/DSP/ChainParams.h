#pragma once
// INVERSO channel strips and the master: one parameter table, a subset of it for each plugin
// (MASTER, KICK, SNARE, TOMS, HATS, CYMBALS, OVERHEADS), and the presets of each. Portable (no JUCE).
// IDs are part of saved sessions: never rename or reorder meaning, only append.
#include "AbismoParams.h"
#include <array>
#include <cstddef>
#include <vector>

namespace inverso_abismo::chain {

using inverso_abismo::Kind;
using inverso_abismo::ParamSpec;

namespace P {
enum : int {
 InGain,
 EqOn, Hpf, Lpf, LsF, LsG, B1F, B1G, B1Q, B2F, B2G, B2Q, HsF, HsG, EqMs,
 GateOn, GateThresh, GateRange, GateHold, GateRelease, GateFocus,
 TrOn, TrAttack, TrSustain,
 TameOn, TameFreq, TameThresh, TameRange, TameQ,
 C1On, C1Type, C1Thresh, C1Ratio, C1Attack, C1Release, C1Knee, C1Time, C1Mix, C1Makeup, C1Sc,
 C2On, C2Type, C2Thresh, C2Ratio, C2Attack, C2Release, C2Mix, C2Makeup,
 SatOn, SatKind, SatDrive, SatTone, SatMix,
 StereoWidth, BassMono,
 LimOn, LimGain, LimCeil, LimAuto, LimRelease,
 OutGain, DitherBits,
 Count
};
}

enum class Strip { Master, Kick, Snare, Toms, Hats, Cymbals, Overheads };
constexpr int numStrips = 7;

inline const std::array<ParamSpec, P::Count>& allSpecs() {
 static const std::array<ParamSpec, P::Count> table{{
  {"IN","Input",-18,18,0,0,.01f,Kind::Float,nullptr,"dB"},
  {"EQ_ON","EQ",0,1,1,0,1,Kind::Bool,nullptr,""},
  {"HPF","High-pass",10,400,10,60,.1f,Kind::Float,nullptr,"hpf"},
  {"LPF","Low-pass",2000,22000,22000,9000,1,Kind::Float,nullptr,"lpf"},
  {"LS_F","Low shelf frequency",30,400,100,100,.1f,Kind::Float,nullptr,"Hz"},
  {"LS_G","Low shelf gain",-12,12,0,0,.01f,Kind::Float,nullptr,"dB"},
  {"B1_F","Bell 1 frequency",40,2000,300,300,.1f,Kind::Float,nullptr,"Hz"},
  {"B1_G","Bell 1 gain",-12,12,0,0,.01f,Kind::Float,nullptr,"dB"},
  {"B1_Q","Bell 1 width (Q)",.3f,8,1,0,.01f,Kind::Float,nullptr,"q"},
  {"B2_F","Bell 2 frequency",500,16000,3000,3000,.1f,Kind::Float,nullptr,"Hz"},
  {"B2_G","Bell 2 gain",-12,12,0,0,.01f,Kind::Float,nullptr,"dB"},
  {"B2_Q","Bell 2 width (Q)",.3f,8,1,0,.01f,Kind::Float,nullptr,"q"},
  {"HS_F","High shelf frequency",2000,20000,10000,8000,1,Kind::Float,nullptr,"Hz"},
  {"HS_G","High shelf gain",-12,12,0,0,.01f,Kind::Float,nullptr,"dB"},
  {"EQ_MS","EQ works on",0,2,0,0,1,Kind::Choice,"Stereo|Mid|Side",""},
  {"GATE_ON","Gate",0,1,0,0,1,Kind::Bool,nullptr,""},
  {"GATE_THRESH","Gate threshold",-70,0,-40,0,.1f,Kind::Float,nullptr,"thr"},
  {"GATE_RANGE","Gate range",6,80,30,0,.1f,Kind::Float,nullptr,"dB"},
  {"GATE_HOLD","Gate hold",0,500,40,0,.1f,Kind::Float,nullptr,"ms"},
  {"GATE_RELEASE","Gate release",5,1000,120,150,.1f,Kind::Float,nullptr,"ms"},
  {"GATE_FOCUS","Gate listens above",20,2000,20,200,.1f,Kind::Float,nullptr,"off20"},
  {"TR_ON","Transients",0,1,0,0,1,Kind::Bool,nullptr,""},
  {"TR_ATTACK","Attack",-100,100,0,0,.1f,Kind::Float,nullptr,"%"},
  {"TR_SUSTAIN","Sustain",-100,100,0,0,.1f,Kind::Float,nullptr,"%"},
  {"TAME_ON","Tame",0,1,0,0,1,Kind::Bool,nullptr,""},
  {"TAME_FREQ","Tame frequency",2000,16000,6500,6500,1,Kind::Float,nullptr,"Hz"},
  {"TAME_THRESH","Tame threshold",-60,0,-24,0,.1f,Kind::Float,nullptr,"thr"},
  {"TAME_RANGE","Tame range",0,18,6,0,.1f,Kind::Float,nullptr,"dB"},
  {"TAME_Q","Tame width (Q)",.5f,6,2,0,.01f,Kind::Float,nullptr,"q"},
  {"C1_ON","Compressor",0,1,1,0,1,Kind::Bool,nullptr,""},
  {"C1_TYPE","Compressor type",0,3,0,0,1,Kind::Choice,"Var-mu|VCA|FET|Opto",""},
  {"C1_THRESH","Threshold",-50,0,-18,0,.1f,Kind::Float,nullptr,"thr"},
  {"C1_RATIO","Ratio",1,20,4,4,.01f,Kind::Float,nullptr,":1"},
  {"C1_ATTACK","Attack",.1f,100,10,10,.01f,Kind::Float,nullptr,"ms"},
  {"C1_RELEASE","Release",10,2000,100,150,.1f,Kind::Float,nullptr,"ms"},
  {"C1_KNEE","Knee",0,18,4,0,.1f,Kind::Float,nullptr,"dB"},
  {"C1_TIME","Var-mu time constant",1,6,3,0,1,Kind::Int,nullptr,"n"},
  {"C1_MIX","Mix (parallel)",0,100,100,0,.1f,Kind::Float,nullptr,"%"},
  {"C1_MAKEUP","Make-up trim",-12,12,0,0,.01f,Kind::Float,nullptr,"dB"},
  {"C1_SC","Detector high-pass",20,500,60,100,.1f,Kind::Float,nullptr,"Hz"},
  {"C2_ON","Glue compressor",0,1,0,0,1,Kind::Bool,nullptr,""},
  {"C2_TYPE","Glue type",0,3,1,0,1,Kind::Choice,"Var-mu|VCA|FET|Opto",""},
  {"C2_THRESH","Glue threshold",-50,0,-16,0,.1f,Kind::Float,nullptr,"thr"},
  {"C2_RATIO","Glue ratio",1,20,2,4,.01f,Kind::Float,nullptr,":1"},
  {"C2_ATTACK","Glue attack",.1f,100,30,10,.01f,Kind::Float,nullptr,"ms"},
  {"C2_RELEASE","Glue release",10,2000,200,150,.1f,Kind::Float,nullptr,"ms"},
  {"C2_MIX","Glue mix",0,100,100,0,.1f,Kind::Float,nullptr,"%"},
  {"C2_MAKEUP","Glue make-up trim",-12,12,0,0,.01f,Kind::Float,nullptr,"dB"},
  {"SAT_ON","Saturation",0,1,0,0,1,Kind::Bool,nullptr,""},
  {"SAT_TYPE","Saturation type",0,2,0,0,1,Kind::Choice,"Tape|Tube|Transformer",""},
  {"SAT_DRIVE","Drive",0,100,0,0,.1f,Kind::Float,nullptr,"%"},
  {"SAT_TONE","Tone",-100,100,0,0,.1f,Kind::Float,nullptr,"%"},
  {"SAT_MIX","Saturation mix",0,100,100,0,.1f,Kind::Float,nullptr,"%"},
  {"WIDTH","Stereo width",0,200,100,0,.1f,Kind::Float,nullptr,"%"},
  {"BASS_MONO","Bass mono below",20,300,20,100,.1f,Kind::Float,nullptr,"off20"},
  {"LIM_ON","Limiter",0,1,1,0,1,Kind::Bool,nullptr,""},
  {"LIM_GAIN","Push into the limiter",0,18,0,0,.01f,Kind::Float,nullptr,"dB"},
  {"LIM_CEIL","Ceiling (true peak)",-6,0,-1,0,.1f,Kind::Float,nullptr,"dB"},
  {"LIM_AUTO","Automatic release",0,1,1,0,1,Kind::Bool,nullptr,""},
  {"LIM_RELEASE","Limiter release",20,1000,200,200,.1f,Kind::Float,nullptr,"ms"},
  {"OUT","Output",-18,18,0,0,.01f,Kind::Float,nullptr,"dB"},
  {"DITHER","Dither",0,3,0,0,1,Kind::Choice,"Off|16 bit|20 bit|24 bit",""},
 }};
 return table;
}

using Values = std::array<float, P::Count>;

struct PresetValue { int param; float value; };
struct Preset { const char* name; std::vector<PresetValue> values; };

inline const char* stripName(Strip s) {
 static const char* names[numStrips] = {"MASTER", "KICK", "SNARE", "TOMS", "HATS", "CYMBALS", "OVERHEADS"};
 return names[static_cast<int>(s)];
}

// Which parameters each plugin has (and so which modules it shows). Anything else stays at its default, off.
inline const std::vector<int>& paramsOf(Strip s) {
 using namespace P;
 static const std::array<std::vector<int>, numStrips> table = [] {
  auto range = [](std::vector<int>& v, int a, int b) { for (int i = a; i <= b; ++i) v.push_back(i); };
  std::array<std::vector<int>, numStrips> t;
  for (int k = 0; k < numStrips; ++k) {
   auto& v = t[static_cast<std::size_t>(k)];
   v.push_back(InGain);
   range(v, EqOn, EqMs);
   const bool master = k == 0, drum = k >= 1 && k <= 3, hats = k == 4, cym = k == 5, oh = k == 6;
   if (drum || hats) range(v, GateOn, GateFocus);
   if (drum || hats || cym) range(v, TrOn, TrSustain);
   if (!drum && !master) range(v, TameOn, TameQ);
   if (k == 2) range(v, TameOn, TameQ);
   range(v, C1On, C1Sc);
   if (master) range(v, C2On, C2Makeup);
   range(v, SatOn, SatMix);
   if (master || cym || oh) range(v, StereoWidth, BassMono);
   if (master) range(v, LimOn, LimRelease);
   v.push_back(OutGain);
   if (master) v.push_back(DitherBits);
  }
  return t;
 }();
 return table[static_cast<std::size_t>(s)];
}

// V(P::Hpf, 30): the preset tables below stay readable.
using V = PresetValue;
namespace T { enum : int { VarMu, Vca, Fet, Opto }; enum : int { Tape, Tube, Transformer }; }

inline const std::vector<Preset>& presets(Strip s) {
 using namespace P;
 static const std::array<std::vector<Preset>, numStrips> all = {{
  // ---------------------------------------------------------------- MASTER
  {
   {"Default/Clean Master", {V{EqOn,1}, V{Hpf,22}, V{C1On,1}, V{C1Type,T::VarMu}, V{C1Thresh,-20}, V{C1Time,3}, V{C1Knee,6}, V{LimOn,1}, V{LimGain,1.5f}, V{LimCeil,-1}}},
   {"Default/Gentle Glue", {V{EqOn,1}, V{Hpf,22}, V{C1On,0}, V{C2On,1}, V{C2Type,T::Vca}, V{C2Thresh,-16}, V{C2Ratio,2}, V{C2Attack,30}, V{C2Release,250}, V{LimOn,1}, V{LimGain,1}}},
   {"Abbey/Abbey Road", {V{Hpf,25}, V{LsF,80}, V{LsG,1.5f}, V{B1F,400}, V{B1G,-1}, V{B1Q,.8f}, V{HsF,12000}, V{HsG,2}, V{C1Type,T::VarMu}, V{C1Thresh,-22}, V{C1Time,4}, V{C1Knee,8},
                         V{SatOn,1}, V{SatKind,T::Transformer}, V{SatDrive,25}, V{SatTone,10}, V{StereoWidth,108}, V{BassMono,120}, V{LimGain,3}}},
   {"Abbey/Tape Warmth", {V{LsF,70}, V{LsG,1}, V{HsF,9000}, V{HsG,1}, V{C1Type,T::Opto}, V{C1Thresh,-20}, V{C1Ratio,3}, V{C1Attack,15}, V{C1Release,200},
                          V{SatOn,1}, V{SatKind,T::Tape}, V{SatDrive,40}, V{SatTone,-10}, V{LimGain,2}}},
   {"Loud/Pop Loud", {V{Hpf,28}, V{B1F,250}, V{B1G,-1.5f}, V{HsF,10000}, V{HsG,2.5f}, V{C1Type,T::Vca}, V{C1Thresh,-22}, V{C1Ratio,3}, V{C1Attack,20}, V{C1Release,120},
                      V{C2On,1}, V{C2Type,T::Fet}, V{C2Thresh,-14}, V{C2Ratio,4}, V{C2Attack,3}, V{C2Release,80},
                      V{SatOn,1}, V{SatKind,T::Tube}, V{SatDrive,20}, V{StereoWidth,115}, V{BassMono,110}, V{LimGain,6}}},
   {"Loud/Rock Punch", {V{Hpf,28}, V{LsF,90}, V{LsG,1.5f}, V{B2F,3500}, V{B2G,1.5f}, V{C1Type,T::Fet}, V{C1Thresh,-20}, V{C1Ratio,4}, V{C1Attack,8}, V{C1Release,90}, V{C1Mix,80},
                        V{C2On,1}, V{C2Type,T::Vca}, V{C2Thresh,-14}, V{C2Ratio,2}, V{C2Attack,30}, V{SatOn,1}, V{SatKind,T::Tape}, V{SatDrive,35}, V{BassMono,100}, V{LimGain,5}}},
   {"Loud/Electronic Wide", {V{Hpf,28}, V{LsF,60}, V{LsG,1}, V{HsF,12000}, V{HsG,2.5f}, V{C1Type,T::Vca}, V{C1Thresh,-20}, V{C1Ratio,2.5f}, V{C1Attack,30}, V{C1Release,150},
                             V{SatOn,1}, V{SatKind,T::Transformer}, V{SatDrive,15}, V{StereoWidth,135}, V{BassMono,140}, V{LimGain,5}}},
   {"Natural/Acoustic", {V{Hpf,20}, V{C1Type,T::Opto}, V{C1Thresh,-24}, V{C1Ratio,2.5f}, V{C1Attack,20}, V{C1Release,300}, V{LimGain,.5f}}},
   {"Natural/Jazz and Orchestral", {V{Hpf,20}, V{C1Type,T::VarMu}, V{C1Thresh,-26}, V{C1Time,2}, V{C1Knee,10}, V{StereoWidth,105}, V{LimGain,0}}},
   {"Character/Dark Tape", {V{Hpf,25}, V{Lpf,16000}, V{HsG,-1.5f}, V{C1Type,T::Opto}, V{C1Thresh,-22}, V{C1Ratio,3}, V{SatOn,1}, V{SatKind,T::Tape}, V{SatDrive,55}, V{SatTone,-30}, V{LimGain,2}}},
   {"Character/Cinematic", {V{Hpf,22}, V{LsF,60}, V{LsG,2}, V{HsF,14000}, V{HsG,1.5f}, V{C1Type,T::VarMu}, V{C1Thresh,-24}, V{C1Time,2}, V{StereoWidth,120}, V{BassMono,90}, V{LimGain,2}}},
   {"Character/Synthwave", {V{Hpf,26}, V{HsF,11000}, V{HsG,2}, V{C1Type,T::Vca}, V{C1Thresh,-20}, V{C1Ratio,3}, V{C1Attack,25},
                            V{SatOn,1}, V{SatKind,T::Tube}, V{SatDrive,30}, V{SatTone,15}, V{StereoWidth,130}, V{BassMono,120}, V{LimGain,4}}},
   {"Utility/Limiter Only", {V{EqOn,0}, V{C1On,0}, V{LimOn,1}, V{LimGain,3}, V{LimCeil,-1}}},
   {"Utility/Metering Only", {V{EqOn,0}, V{C1On,0}, V{LimOn,0}}},
  },
  // ---------------------------------------------------------------- KICK
  {
   {"Default/Starting Point", {V{Hpf,30}, V{B1F,350}, V{B1G,-3}, V{B1Q,1.2f}, V{B2F,4500}, V{B2G,2}, V{GateOn,1}, V{GateThresh,-40}, V{GateHold,60}, V{GateRelease,120},
                               V{C1Type,T::Fet}, V{C1Thresh,-16}, V{C1Ratio,4}, V{C1Attack,15}, V{C1Release,100}, V{SatOn,1}, V{SatKind,T::Tape}, V{SatDrive,10}}},
   {"Rock/Punchy Kick", {V{Hpf,30}, V{LsF,70}, V{LsG,2}, V{B1F,380}, V{B1G,-4}, V{B2F,4000}, V{B2G,3}, V{GateOn,1}, V{GateThresh,-38}, V{GateHold,70}, V{GateRelease,140},
                         V{TrOn,1}, V{TrAttack,30}, V{TrSustain,-10}, V{C1Type,T::Fet}, V{C1Thresh,-18}, V{C1Ratio,6}, V{C1Attack,20}, V{C1Release,90}, V{SatOn,1}, V{SatDrive,15}}},
   {"Rock/Big and Tight", {V{Hpf,28}, V{LsF,60}, V{LsG,3}, V{B1F,300}, V{B1G,-5}, V{B2F,3000}, V{B2G,2}, V{GateOn,1}, V{GateThresh,-36}, V{GateRange,40}, V{GateHold,50}, V{GateRelease,80},
                           V{TrOn,1}, V{TrAttack,15}, V{TrSustain,-30}, V{C1Type,T::Vca}, V{C1Thresh,-20}, V{C1Ratio,4}, V{C1Attack,25}, V{C1Release,80}}},
   {"Metal/Click Heavy", {V{Hpf,35}, V{LsF,55}, V{LsG,1.5f}, V{B1F,400}, V{B1G,-6}, V{B2F,3500}, V{B2G,5}, V{B2Q,1.5f}, V{HsF,8000}, V{HsG,2}, V{GateOn,1}, V{GateThresh,-34}, V{GateRange,60}, V{GateHold,35}, V{GateRelease,50},
                          V{TrOn,1}, V{TrAttack,50}, V{TrSustain,-40}, V{C1Type,T::Fet}, V{C1Thresh,-20}, V{C1Ratio,8}, V{C1Attack,8}, V{C1Release,60}}},
   {"Pop/Soft and Round", {V{Hpf,35}, V{LsF,80}, V{LsG,2}, V{B1F,250}, V{B1G,-2}, V{B2F,3000}, V{B2G,1}, V{GateOn,0}, V{C1Type,T::Opto}, V{C1Thresh,-18}, V{C1Ratio,3}, V{C1Attack,25}, V{C1Release,150},
                           V{SatOn,1}, V{SatKind,T::Tube}, V{SatDrive,15}}},
   {"Jazz/Natural Boom", {V{Hpf,28}, V{LsF,60}, V{LsG,1}, V{B1F,320}, V{B1G,-2}, V{B2F,5000}, V{B2G,1}, V{GateOn,0}, V{C1Type,T::VarMu}, V{C1Thresh,-24}, V{C1Time,2}, V{C1Knee,8}}},
   {"Electronic/Sub Weight", {V{Hpf,22}, V{LsF,55}, V{LsG,4}, V{B1F,200}, V{B1G,-2}, V{B2F,2500}, V{B2G,1}, V{GateOn,0}, V{TrOn,1}, V{TrAttack,10}, V{TrSustain,20},
                              V{C1Type,T::Vca}, V{C1Thresh,-16}, V{C1Ratio,3}, V{C1Attack,30}, V{C1Release,120}, V{SatOn,1}, V{SatKind,T::Transformer}, V{SatDrive,20}}},
   {"Lo-fi/Dirty Tape", {V{Hpf,40}, V{Lpf,9000}, V{B1F,300}, V{B1G,-3}, V{GateOn,0}, V{C1Type,T::Opto}, V{C1Thresh,-20}, V{C1Ratio,4}, V{SatOn,1}, V{SatKind,T::Tape}, V{SatDrive,60}, V{SatTone,-20}}},
  },
  // ---------------------------------------------------------------- SNARE
  {
   {"Default/Starting Point", {V{Hpf,80}, V{B1F,500}, V{B1G,-2}, V{B2F,5000}, V{B2G,2}, V{GateOn,1}, V{GateThresh,-38}, V{GateHold,80}, V{GateRelease,150}, V{GateFocus,150},
                               V{TameOn,1}, V{TameFreq,7000}, V{TameThresh,-24}, V{TameRange,5}, V{C1Type,T::Fet}, V{C1Thresh,-18}, V{C1Ratio,4}, V{C1Attack,12}, V{C1Release,100}, V{SatOn,1}, V{SatDrive,10}}},
   {"Rock/Crack", {V{Hpf,90}, V{LsF,180}, V{LsG,1.5f}, V{B1F,450}, V{B1G,-3}, V{B2F,4500}, V{B2G,3}, V{HsF,10000}, V{HsG,1.5f}, V{GateOn,1}, V{GateThresh,-34}, V{GateHold,90}, V{GateRelease,140}, V{GateFocus,200},
                   V{TrOn,1}, V{TrAttack,40}, V{TrSustain,-10}, V{TameOn,1}, V{TameFreq,6500}, V{TameThresh,-22}, V{C1Type,T::Fet}, V{C1Thresh,-20}, V{C1Ratio,6}, V{C1Attack,10}, V{C1Release,90}, V{SatOn,1}, V{SatDrive,15}}},
   {"Rock/Fat Backbeat", {V{Hpf,80}, V{LsF,200}, V{LsG,3}, V{B1F,400}, V{B1G,-3}, V{B2F,3500}, V{B2G,2}, V{GateOn,1}, V{GateThresh,-36}, V{GateHold,100}, V{GateRelease,200},
                          V{TrOn,1}, V{TrAttack,15}, V{TrSustain,25}, V{TameOn,1}, V{C1Type,T::Vca}, V{C1Thresh,-22}, V{C1Ratio,4}, V{C1Attack,20}, V{C1Release,100}, V{SatOn,1}, V{SatKind,T::Transformer}, V{SatDrive,20}}},
   {"Metal/Tight and Hard", {V{Hpf,100}, V{B1F,600}, V{B1G,-4}, V{B2F,5000}, V{B2G,4}, V{B2Q,1.3f}, V{GateOn,1}, V{GateThresh,-30}, V{GateRange,60}, V{GateHold,50}, V{GateRelease,60}, V{GateFocus,250},
                             V{TrOn,1}, V{TrAttack,50}, V{TrSustain,-35}, V{TameOn,1}, V{TameThresh,-20}, V{C1Type,T::Fet}, V{C1Thresh,-22}, V{C1Ratio,8}, V{C1Attack,6}, V{C1Release,60}}},
   {"Pop/Wide and Smooth", {V{Hpf,80}, V{LsF,200}, V{LsG,1}, V{B2F,6000}, V{B2G,2}, V{GateOn,0}, V{TameOn,1}, V{TameFreq,7500}, V{TameRange,6}, V{C1Type,T::Opto}, V{C1Thresh,-20}, V{C1Ratio,3}, V{C1Attack,20}, V{C1Release,160}, V{SatOn,1}, V{SatKind,T::Tube}, V{SatDrive,15}}},
   {"Jazz/Brushed and Natural", {V{Hpf,70}, V{B2F,6000}, V{B2G,1}, V{GateOn,0}, V{TameOn,0}, V{C1Type,T::VarMu}, V{C1Thresh,-26}, V{C1Time,2}, V{C1Knee,10}}},
   {"Electronic/Gated Snap", {V{Hpf,120}, V{B1F,700}, V{B1G,-3}, V{B2F,5500}, V{B2G,3}, V{GateOn,1}, V{GateThresh,-32}, V{GateRange,70}, V{GateHold,70}, V{GateRelease,40}, V{TrOn,1}, V{TrAttack,40}, V{TrSustain,-40}, V{C1Type,T::Vca}, V{C1Thresh,-20}, V{C1Ratio,5}, V{C1Attack,5}, V{C1Release,60}}},
   {"Lo-fi/Trash", {V{Hpf,150}, V{Lpf,8000}, V{GateOn,0}, V{C1Type,T::Opto}, V{C1Thresh,-24}, V{C1Ratio,5}, V{SatOn,1}, V{SatKind,T::Tape}, V{SatDrive,65}, V{SatTone,-15}}},
  },
  // ---------------------------------------------------------------- TOMS
  {
   {"Default/Starting Point", {V{Hpf,60}, V{B1F,400}, V{B1G,-3}, V{B2F,4500}, V{B2G,2}, V{GateOn,1}, V{GateThresh,-38}, V{GateHold,120}, V{GateRelease,200}, V{GateFocus,100},
                               V{C1Type,T::Fet}, V{C1Thresh,-18}, V{C1Ratio,4}, V{C1Attack,15}, V{C1Release,120}, V{SatOn,1}, V{SatDrive,10}}},
   {"Rock/Big Round", {V{Hpf,50}, V{LsF,100}, V{LsG,3}, V{B1F,350}, V{B1G,-4}, V{B2F,4000}, V{B2G,3}, V{GateOn,1}, V{GateThresh,-36}, V{GateHold,150}, V{GateRelease,250}, V{GateFocus,100},
                       V{TrOn,1}, V{TrAttack,25}, V{TrSustain,10}, V{C1Type,T::Vca}, V{C1Thresh,-20}, V{C1Ratio,4}, V{C1Attack,20}, V{C1Release,120}, V{SatOn,1}, V{SatKind,T::Tape}, V{SatDrive,20}}},
   {"Rock/Tight Attack", {V{Hpf,60}, V{B1F,450}, V{B1G,-5}, V{B2F,3500}, V{B2G,4}, V{GateOn,1}, V{GateThresh,-34}, V{GateRange,50}, V{GateHold,90}, V{GateRelease,120},
                          V{TrOn,1}, V{TrAttack,45}, V{TrSustain,-25}, V{C1Type,T::Fet}, V{C1Thresh,-20}, V{C1Ratio,6}, V{C1Attack,10}, V{C1Release,80}}},
   {"Metal/Punch", {V{Hpf,70}, V{B1F,400}, V{B1G,-5}, V{B2F,3000}, V{B2G,5}, V{B2Q,1.4f}, V{GateOn,1}, V{GateThresh,-32}, V{GateRange,60}, V{GateHold,80}, V{GateRelease,100},
                    V{TrOn,1}, V{TrAttack,50}, V{TrSustain,-30}, V{C1Type,T::Fet}, V{C1Thresh,-22}, V{C1Ratio,8}, V{C1Attack,8}, V{C1Release,70}}},
   {"Jazz/Open and Natural", {V{Hpf,50}, V{B1F,300}, V{B1G,-2}, V{B2F,5000}, V{B2G,1}, V{GateOn,0}, V{C1Type,T::VarMu}, V{C1Thresh,-26}, V{C1Time,2}, V{C1Knee,10}}},
   {"Pop/Smooth", {V{Hpf,60}, V{B1F,350}, V{B1G,-3}, V{GateOn,1}, V{GateThresh,-42}, V{GateRange,25}, V{GateHold,150}, V{GateRelease,300}, V{C1Type,T::Opto}, V{C1Thresh,-20}, V{C1Ratio,3}, V{C1Attack,25}, V{C1Release,180}}},
  },
  // ---------------------------------------------------------------- HATS
  {
   {"Default/Starting Point", {V{Hpf,250}, V{B2F,9000}, V{B2G,1}, V{GateOn,0}, V{TrOn,0}, V{TameOn,1}, V{TameFreq,7500}, V{TameThresh,-26}, V{TameRange,6}, V{C1Type,T::Opto}, V{C1Thresh,-22}, V{C1Ratio,3}, V{C1Attack,15}, V{C1Release,120}}},
   {"Rock/Controlled Sizzle", {V{Hpf,300}, V{B2F,8000}, V{B2G,1.5f}, V{TameOn,1}, V{TameFreq,8000}, V{TameThresh,-24}, V{TameRange,8}, V{TrOn,1}, V{TrAttack,-20}, V{C1Type,T::Vca}, V{C1Thresh,-22}, V{C1Ratio,3}, V{C1Attack,10}, V{C1Release,100}}},
   {"Rock/Tight Hats", {V{Hpf,350}, V{B2F,7000}, V{B2G,2}, V{GateOn,1}, V{GateThresh,-36}, V{GateRange,30}, V{GateHold,20}, V{GateRelease,50}, V{TrOn,1}, V{TrAttack,25}, V{TrSustain,-30}, V{TameOn,1}, V{TameFreq,8500}, V{TameRange,5}}},
   {"Pop/Smooth and Airy", {V{Hpf,300}, V{HsF,10000}, V{HsG,2}, V{TameOn,1}, V{TameFreq,7000}, V{TameThresh,-28}, V{TameRange,8}, V{C1Type,T::Opto}, V{C1Thresh,-24}, V{C1Ratio,3}, V{C1Attack,20}, V{C1Release,150}}},
   {"Electronic/Crisp", {V{Hpf,400}, V{B2F,9000}, V{B2G,2}, V{TrOn,1}, V{TrAttack,35}, V{TrSustain,-20}, V{TameOn,1}, V{TameFreq,9000}, V{TameRange,4}, V{C1Type,T::Vca}, V{C1Thresh,-22}, V{C1Ratio,3}}},
   {"Lo-fi/Dusty", {V{Hpf,400}, V{Lpf,9000}, V{TameOn,1}, V{TameFreq,6000}, V{TameRange,6}, V{C1Type,T::Opto}, V{C1Thresh,-22}, V{C1Ratio,4}, V{SatOn,1}, V{SatKind,T::Tape}, V{SatDrive,50}, V{SatTone,-25}}},
  },
  // ---------------------------------------------------------------- CYMBALS
  {
   {"Default/Starting Point", {V{Hpf,250}, V{B1F,500}, V{B1G,-2}, V{B2F,8000}, V{B2G,1}, V{TameOn,1}, V{TameFreq,6000}, V{TameThresh,-26}, V{TameRange,8}, V{TameQ,1.5f}, V{C1Type,T::Opto}, V{C1Thresh,-22}, V{C1Ratio,3}, V{C1Attack,20}, V{C1Release,200}, V{StereoWidth,110}}},
   {"Rock/Smooth Wash", {V{Hpf,300}, V{B1F,600}, V{B1G,-3}, V{TameOn,1}, V{TameFreq,5500}, V{TameThresh,-28}, V{TameRange,10}, V{TameQ,1.2f}, V{TrOn,1}, V{TrSustain,-20}, V{C1Type,T::Opto}, V{C1Thresh,-24}, V{C1Ratio,3}, V{StereoWidth,115}}},
   {"Rock/Bright and Present", {V{Hpf,250}, V{B2F,9000}, V{B2G,2}, V{HsF,11000}, V{HsG,1.5f}, V{TameOn,1}, V{TameFreq,6500}, V{TameThresh,-24}, V{TameRange,6}, V{C1Type,T::Vca}, V{C1Thresh,-22}, V{C1Ratio,2.5f}, V{C1Attack,20}}},
   {"Metal/Tamed Splash", {V{Hpf,350}, V{B1F,700}, V{B1G,-4}, V{TameOn,1}, V{TameFreq,5000}, V{TameThresh,-30}, V{TameRange,12}, V{TameQ,1}, V{TrOn,1}, V{TrAttack,-15}, V{TrSustain,-35}, V{C1Type,T::Fet}, V{C1Thresh,-24}, V{C1Ratio,4}}},
   {"Jazz/Shimmer", {V{Hpf,200}, V{HsF,12000}, V{HsG,2}, V{TameOn,1}, V{TameFreq,6000}, V{TameRange,4}, V{C1Type,T::VarMu}, V{C1Thresh,-28}, V{C1Time,2}, V{StereoWidth,120}}},
   {"Experimental/Dark Metal", {V{Hpf,200}, V{Lpf,10000}, V{HsG,-3}, V{TameOn,1}, V{TameFreq,4500}, V{TameThresh,-30}, V{TameRange,10}, V{SatOn,1}, V{SatKind,T::Tape}, V{SatDrive,40}, V{SatTone,-30}, V{StereoWidth,125}}},
  },
  // ---------------------------------------------------------------- OVERHEADS
  {
   {"Default/Starting Point", {V{Hpf,150}, V{B1F,400}, V{B1G,-2}, V{B2F,8000}, V{B2G,1}, V{TameOn,1}, V{TameFreq,6500}, V{TameThresh,-26}, V{TameRange,6}, V{C1Type,T::Opto}, V{C1Thresh,-24}, V{C1Ratio,3}, V{C1Attack,20}, V{C1Release,200}, V{StereoWidth,105}}},
   {"Rock/Big Kit", {V{Hpf,180}, V{LsF,200}, V{LsG,1.5f}, V{B1F,450}, V{B1G,-3}, V{HsF,10000}, V{HsG,2}, V{TameOn,1}, V{TameThresh,-24}, V{C1Type,T::Vca}, V{C1Thresh,-24}, V{C1Ratio,3}, V{C1Attack,25}, V{C1Release,150},
                      V{SatOn,1}, V{SatKind,T::Tape}, V{SatDrive,20}, V{StereoWidth,115}, V{BassMono,200}}},
   {"Rock/Punchy Room", {V{Hpf,150}, V{B1F,350}, V{B1G,-2}, V{B2F,3500}, V{B2G,2}, V{TameOn,1}, V{C1Type,T::Fet}, V{C1Thresh,-26}, V{C1Ratio,5}, V{C1Attack,15}, V{C1Release,120}, V{C1Mix,70}, V{SatOn,1}, V{SatDrive,20}, V{StereoWidth,110}}},
   {"Pop/Open and Airy", {V{Hpf,200}, V{HsF,12000}, V{HsG,3}, V{B1F,400}, V{B1G,-2}, V{TameOn,1}, V{TameFreq,7000}, V{TameRange,7}, V{C1Type,T::Opto}, V{C1Thresh,-26}, V{C1Ratio,2.5f}, V{StereoWidth,125}}},
   {"Jazz/Natural Stereo", {V{Hpf,120}, V{TameOn,0}, V{C1Type,T::VarMu}, V{C1Thresh,-30}, V{C1Time,2}, V{C1Knee,10}, V{StereoWidth,115}}},
   {"Metal/Controlled", {V{Hpf,220}, V{B1F,500}, V{B1G,-4}, V{TameOn,1}, V{TameFreq,6000}, V{TameThresh,-28}, V{TameRange,10}, V{C1Type,T::Fet}, V{C1Thresh,-28}, V{C1Ratio,6}, V{C1Attack,10}, V{C1Release,100}, V{StereoWidth,100}}},
  },
 }};
 return all[static_cast<std::size_t>(s)];
}

// The table of one plugin: its parameters in order, with the defaults of its first preset.
struct Layout {
 std::vector<ParamSpec> specs;        // only this plugin's parameters
 std::vector<int> global;             // specs[i] is allSpecs()[global[i]]
 std::array<int, P::Count> local{};   // global -> index in specs, -1 if the plugin does not have it
 Values defaults{};                   // every global parameter: the start state (first preset applied)
};

inline void applyPreset(Values& v, const Preset& p) {
 for (const auto& pv : p.values) v[static_cast<std::size_t>(pv.param)] = pv.value;
}

inline const Layout& layout(Strip s) {
 static const std::array<Layout, numStrips> all = [] {
  std::array<Layout, numStrips> t;
  for (int k = 0; k < numStrips; ++k) {
   auto& l = t[static_cast<std::size_t>(k)];
   for (std::size_t i = 0; i < allSpecs().size(); ++i) l.defaults[i] = allSpecs()[i].def;
   applyPreset(l.defaults, presets(static_cast<Strip>(k)).front());
   l.local.fill(-1);
   for (int g : paramsOf(static_cast<Strip>(k))) {
    ParamSpec sp = allSpecs()[static_cast<std::size_t>(g)];
    if (k == 0 && g == P::OutGain) sp.max = 0;   // the master's trim only lowers: the limiter's ceiling is the last word
    sp.def = l.defaults[static_cast<std::size_t>(g)];
    l.local[static_cast<std::size_t>(g)] = static_cast<int>(l.specs.size());
    l.specs.push_back(sp);
    l.global.push_back(g);
   }
  }
  return t;
 }();
 return all[static_cast<std::size_t>(s)];
}

// Every global parameter after preset `index`: the first preset is the base of all of them, and parameters
// the plugin does not have stay at their defaults.
inline Values presetValues(Strip s, std::size_t index) {
 Values v{};
 for (std::size_t i = 0; i < v.size(); ++i) v[i] = allSpecs()[i].def;
 const auto& list = presets(s);
 applyPreset(v, list.front());
 if (index < list.size() && index > 0) applyPreset(v, list[index]);
 const auto& l = layout(s);
 for (std::size_t i = 0; i < v.size(); ++i) if (l.local[i] < 0) v[i] = allSpecs()[i].def;
 return v;
}

} // namespace inverso_abismo::chain
