#pragma once
// INVERSO BERIMBASS parameter table and presets. Shares ParamSpec / Preset
// with the ABISMO family. IDs are saved in sessions: never rename; only append.
#include "../../../Abismo/Source/DSP/AbismoParams.h"

namespace inverso_berimbass {

using inverso_abismo::Kind;
using inverso_abismo::ParamSpec;
using inverso_abismo::Preset;

namespace P {
enum : int {
 Technique, Decay, Brightness, Position, Stiffness, Buzz, Pressure, Attack, Release,
 Body, BodyMix, Gourd, GourdRate, GourdDepth, BodyTune,
 Octave, Tune, VoiceMode, Glide, BendRange, StoneBend, Vibrato, VibRate,
 Sub, Drive, Tone, VelSens, RevMix, RevSize, RevDecay, Output,
 Caxixi,
 Count
};
}

inline const std::array<ParamSpec, P::Count>& specs() {
 static const std::array<ParamSpec, P::Count> table{{
  {"TECHNIQUE","Technique",0,3,1,0,1,Kind::Choice,"Pizzicato|Bambu|Arco|Pedra",""},
  {"DECAY","String decay",.2f,12,3,2,.01f,Kind::Float,nullptr,"s"},
  {"BRIGHTNESS","Brightness",0,100,75,0,.01f,Kind::Float,nullptr,"%"},
  {"POSITION","Strike position",3,50,15,0,.01f,Kind::Float,nullptr,"%"},
  {"STIFFNESS","Stiffness",0,100,55,0,.01f,Kind::Float,nullptr,"%"},
  {"BUZZ","Buzz",0,100,20,0,.01f,Kind::Float,nullptr,"%"},
  {"PRESSURE","Pressure",0,100,60,0,.01f,Kind::Float,nullptr,"%"},
  {"ATTACK","Bow attack",1,2000,150,200,.1f,Kind::Float,nullptr,"ms"},
  {"RELEASE","Release",10,3000,250,300,.1f,Kind::Float,nullptr,"ms"},
  {"BODY","Body",0,3,0,0,1,Kind::Choice,"Cabaca|Banjo|Balde|Nenhum",""},
  {"BODY_MIX","Body mix",0,100,80,0,.01f,Kind::Float,nullptr,"%"},
  {"GOURD","Gourd open",0,100,50,0,.01f,Kind::Float,nullptr,"%"},
  {"GOURD_RATE","Gourd motion rate",0,8,0,1,.01f,Kind::Float,nullptr,"Hz"},
  {"GOURD_DEPTH","Gourd motion depth",0,100,40,0,.01f,Kind::Float,nullptr,"%"},
  {"BODY_TUNE","Body tune",-12,12,0,0,.01f,Kind::Float,nullptr,"st"},
  {"OCTAVE","Octave",0,3,2,0,1,Kind::Choice,"-2|-1|0|+1",""},
  {"TUNE","Fine tune",-100,100,0,0,.1f,Kind::Float,nullptr,"ct"},
  {"VOICE_MODE","Voice mode",0,1,1,0,1,Kind::Choice,"Poly|Mono",""},
  {"GLIDE","Glide",0,1000,60,150,.1f,Kind::Float,nullptr,"ms"},
  {"BEND_RANGE","Pitch bend range",1,12,2,0,1,Kind::Int,nullptr,"st"},
  {"STONE_BEND","Stone bend",0,4,1,0,.01f,Kind::Float,nullptr,"st"},
  {"VIBRATO","Vibrato",0,100,0,0,.01f,Kind::Float,nullptr,"%"},
  {"VIB_RATE","Vibrato rate",1,8,5,0,.01f,Kind::Float,nullptr,"Hz"},
  {"SUB","Sub",0,100,0,0,.01f,Kind::Float,nullptr,"%"},
  {"DRIVE","Drive",0,100,10,0,.01f,Kind::Float,nullptr,"%"},
  {"TONE","Tone",200,20000,12000,2000,.1f,Kind::Float,nullptr,"Hz"},
  {"VEL_SENS","Velocity sensitivity",0,100,70,0,.01f,Kind::Float,nullptr,"%"},
  {"REV_MIX","Reverb mix",0,100,12,0,.01f,Kind::Float,nullptr,"%"},
  {"REV_SIZE","Reverb size",0,100,40,0,.01f,Kind::Float,nullptr,"%"},
  {"REV_DECAY","Reverb decay",.3f,10,1.5f,2,.01f,Kind::Float,nullptr,"s"},
  {"OUTPUT","Output",-36,12,-5,0,.01f,Kind::Float,nullptr,"dB"},
  {"CAXIXI","Caxixi",0,100,45,0,.01f,Kind::Float,nullptr,"%"},
 }};
 return table;
}

using Values = std::array<float, P::Count>;

inline Values defaults() {
 Values v{};
 for (std::size_t i = 0; i < v.size(); ++i) v[i] = specs()[i].def;
 return v;
}

inline const std::array<Preset, 12>& presets() {
 using namespace P;
 static const std::array<Preset, 12> table{{
  {"Berimbau Gunga", {{-1, 0}}},
  {"Gunga Cabaca Viva", {{GourdRate,2.2f},{GourdDepth,60},{Buzz,25},{Decay,4},{Output,-5},{-1,0}}},
  {"Dobrao Zumbindo", {{Technique,3},{Buzz,75},{StoneBend,2},{Pressure,70},{Brightness,70},{Decay,3.5f},{Output,-4},{-1,0}}},
  {"Gunga no Arco", {{Technique,2},{Attack,300},{Release,600},{Pressure,55},{Buzz,10},{Gourd,65},{GourdRate,.6f},
   {GourdDepth,30},{Vibrato,15},{RevMix,20},{Output,-11.5},{-1,0}}},
  {"Berimbass Sub", {{Sub,55},{Drive,35},{Brightness,50},{Buzz,15},{Decay,5},{Tone,6000},{Output,-10.5},{-1,0}}},
  {"Banjo Bass", {{Body,1},{Technique,0},{Decay,1.2f},{Brightness,75},{Position,10},{Stiffness,20},{Buzz,0},
   {BodyMix,85},{Drive,15},{Output,-3.5},{-1,0}}},
  {"Banjo de Bambu", {{Body,1},{Technique,1},{Decay,1.5f},{Brightness,80},{Position,8},{Buzz,10},{Pressure,80},{Output,-4},{-1,0}}},
  {"Banjo Arqueado", {{Body,1},{Technique,2},{Attack,120},{Release,400},{Brightness,65},{Pressure,65},{Vibrato,20},{Output,-9.5},{-1,0}}},
  {"Balde do Quintal", {{Body,2},{Technique,0},{Decay,2.5f},{Brightness,35},{Position,25},{Stiffness,5},{Buzz,5},
   {Glide,120},{BodyMix,90},{Drive,20},{Tone,5000},{Output,-11},{-1,0}}},
  {"Tina Arqueada", {{Body,2},{Technique,2},{Attack,200},{Release,500},{Brightness,40},{Pressure,60},{Tone,6000},
   {Glide,200},{Output,-12},{-1,0}}},
  {"Balde de Pedra", {{Body,2},{Technique,3},{Buzz,55},{StoneBend,1.5f},{Brightness,45},{Decay,2.5f},{Output,2},{-1,0}}},
  {"Corda Seca", {{Body,3},{Technique,0},{Decay,2},{Brightness,55},{Stiffness,15},{Buzz,0},{Sub,30},{Output,1},{-1,0}}},
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

} // namespace inverso_berimbass
