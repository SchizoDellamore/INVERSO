#pragma once
// RELICARIO: four forgotten machines in one instrument. A tape-loop synth, a
// reed chord organ, an electrically maintained tuning fork and a Soviet
// transistor organ, each with its own level so they play alone or together.
// Portable parameter table and factory presets (no JUCE).
#include "AbismoParams.h"
#include <array>
#include <cstddef>

namespace inverso_abismo::relicario {

using inverso_abismo::Kind;
using inverso_abismo::ParamSpec;

namespace P {
enum : int {
 // Machine levels.
 FitaLevel, HarmLevel, ForkLevel, OrganLevel,
 // FITA: two tape loops of recorded waveforms.
 Loop1Wave, Loop2Wave, LoopMix, Loop2Octave, Loop2Detune, LoopLength, Wear, Wow, TapeSat, FitaTone,
 // HARMONIO: reed organ with an electric fan bellows.
 Era, Breath, Motor, Sag, Speak,
 // DIAPASAO: electrically maintained tuning fork.
 Spark, Clang, Maintain, Beat,
 // ORGAO: Soviet transistor organ.
 Reg16, Reg8, Reg4, Upper, UpperInterval, OrganTone, TremDepth, TremRate, SpeakerBreak, Hum,
 // Keyboard and envelope.
 Attack, Decay, Sustain, Release, VelSens, Octave, Fine, Glide, BendRange,
 // Echoes and space.
 Ensemble, EchoMix, EchoTime, EchoFeedback, Dust, RevMix, RevSize, RevDecay, Width, Output,
 Count
};
}

// IDs are part of saved sessions: never rename or reorder meaning, only append.
inline const std::array<ParamSpec, P::Count>& specs() {
 static const std::array<ParamSpec, P::Count> table{{
  {"FITA_LEVEL","Tape level",0,100,70,0,.01f,Kind::Float,nullptr,"%"},
  {"HARM_LEVEL","Reed organ level",0,100,0,0,.01f,Kind::Float,nullptr,"%"},
  {"FORK_LEVEL","Tuning fork level",0,100,0,0,.01f,Kind::Float,nullptr,"%"},
  {"ORGAN_LEVEL","Soviet organ level",0,100,0,0,.01f,Kind::Float,nullptr,"%"},
  {"LOOP1_WAVE","Loop 1 wave",0,5,0,0,1,Kind::Choice,"Serra|Serra dupla|Triangulo|Quadrada|Seno|Estatica",""},
  {"LOOP2_WAVE","Loop 2 wave",0,5,2,0,1,Kind::Choice,"Serra|Serra dupla|Triangulo|Quadrada|Seno|Estatica",""},
  {"LOOP_MIX","Loop balance",0,100,50,0,.01f,Kind::Float,nullptr,"%"},
  {"LOOP2_OCTAVE","Loop 2 octave",0,2,1,0,1,Kind::Choice,"-1|0|+1",""},
  {"LOOP2_DETUNE","Loop 2 detune",-50,50,7,0,.01f,Kind::Float,nullptr,"ct"},
  {"LOOP_LENGTH","Loop length",.5f,12,4,3,.001f,Kind::Float,nullptr,"s"},
  {"WEAR","Tape wear",0,100,35,0,.01f,Kind::Float,nullptr,"%"},
  {"WOW","Wow and flutter",0,100,30,0,.01f,Kind::Float,nullptr,"%"},
  {"TAPE_SAT","Tape saturation",0,100,30,0,.01f,Kind::Float,nullptr,"%"},
  {"FITA_TONE","Tape tone",0,100,60,0,.01f,Kind::Float,nullptr,"%"},
  {"ERA","Bakelite to plastic",0,100,0,0,.01f,Kind::Float,nullptr,"%"},
  {"BREATH","Reed breath",0,100,30,0,.01f,Kind::Float,nullptr,"%"},
  {"MOTOR","Fan motor",0,100,25,0,.01f,Kind::Float,nullptr,"%"},
  {"SAG","Bellows sag",0,100,50,0,.01f,Kind::Float,nullptr,"%"},
  {"SPEAK","Reed speak time",5,400,60,60,.1f,Kind::Float,nullptr,"ms"},
  {"SPARK","Spark",0,100,30,0,.01f,Kind::Float,nullptr,"%"},
  {"CLANG","Strike clang",0,100,50,0,.01f,Kind::Float,nullptr,"%"},
  {"MAINTAIN","Electric drive",0,100,70,0,.01f,Kind::Float,nullptr,"%"},
  {"BEAT","Second fork beat",0,3,.4f,0,.001f,Kind::Float,nullptr,"Hz"},
  {"REG16","Register 16'",0,100,0,0,.01f,Kind::Float,nullptr,"%"},
  {"REG8","Register 8'",0,100,100,0,.01f,Kind::Float,nullptr,"%"},
  {"REG4","Register 4'",0,100,50,0,.01f,Kind::Float,nullptr,"%"},
  {"UPPER","Upper register",0,100,0,0,.01f,Kind::Float,nullptr,"%"},
  {"UPPER_INTERVAL","Upper interval",0,2,0,0,1,Kind::Choice,"Oitava|Quinta|Terca",""},
  {"ORGAN_TONE","Organ tone",0,100,50,0,.01f,Kind::Float,nullptr,"%"},
  {"TREM_DEPTH","Tremolo depth",0,100,30,0,.01f,Kind::Float,nullptr,"%"},
  {"TREM_RATE","Tremolo rate",1,10,6,0,.001f,Kind::Float,nullptr,"Hz"},
  {"SPEAKER_BREAK","Broken speaker",0,100,0,0,.01f,Kind::Float,nullptr,"%"},
  {"HUM","Mains hum",0,100,15,0,.01f,Kind::Float,nullptr,"%"},
  {"ATTACK","Attack",1,10000,8,400,.1f,Kind::Float,nullptr,"ms"},
  {"DECAY","Decay",1,10000,600,500,.1f,Kind::Float,nullptr,"ms"},
  {"SUSTAIN","Sustain",0,100,100,0,.01f,Kind::Float,nullptr,"%"},
  {"RELEASE","Release",5,15000,350,1000,.1f,Kind::Float,nullptr,"ms"},
  {"VEL_SENS","Velocity sensitivity",0,100,40,0,.01f,Kind::Float,nullptr,"%"},
  {"OCTAVE","Octave",0,4,2,0,1,Kind::Choice,"-2|-1|0|+1|+2",""},
  {"FINE","Fine tune",-50,50,0,0,.01f,Kind::Float,nullptr,"ct"},
  {"GLIDE","Glide",0,2000,0,300,.1f,Kind::Float,nullptr,"ms"},
  {"BEND_RANGE","Pitch bend range",1,24,2,0,1,Kind::Int,nullptr,"st"},
  {"ENSEMBLE","Tape ensemble",0,100,0,0,.01f,Kind::Float,nullptr,"%"},
  {"ECHO_MIX","Tape echo",0,100,0,0,.01f,Kind::Float,nullptr,"%"},
  {"ECHO_TIME","Echo time",20,1200,380,300,.1f,Kind::Float,nullptr,"ms"},
  {"ECHO_FEEDBACK","Echo repeats",0,95,35,0,.01f,Kind::Float,nullptr,"%"},
  {"DUST","Dust",0,100,10,0,.01f,Kind::Float,nullptr,"%"},
  {"REV_MIX","Reverb mix",0,100,20,0,.01f,Kind::Float,nullptr,"%"},
  {"REV_SIZE","Reverb size",0,100,60,0,.01f,Kind::Float,nullptr,"%"},
  {"REV_DECAY","Reverb decay",.3f,20,3,3,.01f,Kind::Float,nullptr,"s"},
  {"WIDTH","Width",0,200,100,0,.01f,Kind::Float,nullptr,"%"},
  {"OUTPUT","Output",-36,6,-6,0,.01f,Kind::Float,nullptr,"dB"},
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
struct Preset { const char* name; PresetValue values[40]; };

// Each preset starts from the defaults and overrides what it lists (ends at param -1).
inline const std::array<Preset, 23>& presets() {
 using namespace P;
 static const std::array<Preset, 23> table{{
  {"Init", {{-1, 0}}},
  // FITA
  {"Fita de Eno", {{FitaLevel,80},{Loop1Wave,1},{Loop2Wave,2},{Loop2Octave,2},{Loop2Detune,5},{LoopLength,6},{Wear,35},
   {Wow,25},{TapeSat,30},{FitaTone,55},{Attack,900},{Release,3000},{Ensemble,30},{EchoMix,25},{EchoTime,620},
   {EchoFeedback,45},{RevMix,35},{RevSize,80},{RevDecay,6},{Output,-6},{-1,0}}},
  {"Loop Gasto", {{FitaLevel,80},{Loop1Wave,3},{Loop2Wave,4},{LoopLength,2.5f},{Wear,85},{Wow,55},{FitaTone,40},
   {TapeSat,50},{Dust,40},{Attack,300},{Release,2000},{RevMix,30},{Output,-11},{-1,0}}},
  {"Estatica do Porao", {{FitaLevel,85},{Loop1Wave,5},{Loop2Wave,4},{LoopMix,40},{Loop2Octave,1},{Wear,60},{FitaTone,50},
   {Attack,1500},{Release,4000},{EchoMix,30},{EchoTime,450},{RevMix,45},{RevSize,85},{RevDecay,7},{Output,-0.5f},{-1,0}}},
  {"Fitas Desencontradas", {{FitaLevel,80},{Loop1Wave,4},{Loop2Wave,4},{Loop2Octave,1},{Loop2Detune,0},{LoopLength,7},
   {Wear,75},{Wow,40},{Attack,2000},{Release,5000},{Ensemble,40},{RevMix,50},{RevSize,90},{RevDecay,9},{Output,-7.5f},{-1,0}}},
  {"Baixo de Fita", {{FitaLevel,85},{Octave,1},{Loop1Wave,0},{Loop2Wave,3},{Loop2Octave,0},{LoopMix,40},{FitaTone,35},
   {TapeSat,70},{Wear,20},{Wow,10},{Attack,5},{Decay,800},{Sustain,70},{Release,300},{RevMix,10},{Output,-10.5f},{-1,0}}},
  // HARMONIO
  {"Baquelite", {{FitaLevel,0},{HarmLevel,90},{Era,0},{Breath,35},{Motor,35},{Sag,50},{Speak,70},{Attack,20},
   {Release,400},{RevMix,25},{Output,-10},{-1,0}}},
  {"Plastico 1974", {{FitaLevel,0},{HarmLevel,90},{Era,100},{Breath,20},{Motor,25},{Sag,40},{Speak,40},{Release,300},
   {RevMix,20},{Ensemble,10},{Output,-6.5f},{-1,0}}},
  {"Harmonio da Igreja Vazia", {{FitaLevel,0},{HarmLevel,90},{Era,30},{Breath,45},{Motor,50},{Sag,70},{Speak,120},
   {Attack,200},{Release,1500},{RevMix,60},{RevSize,90},{RevDecay,8},{Output,-6},{-1,0}}},
  {"Acordeao Fantasma", {{FitaLevel,0},{HarmLevel,85},{Era,60},{Breath,30},{Sag,35},{Ensemble,60},{EchoMix,20},
   {EchoTime,300},{Attack,60},{Release,800},{RevMix,35},{Output,-6},{-1,0}}},
  // DIAPASAO
  {"Diapasao de Laboratorio", {{FitaLevel,0},{ForkLevel,90},{Spark,25},{Clang,50},{Maintain,80},{Beat,.3f},{Attack,2},
   {Release,1200},{RevMix,35},{Output,-9},{-1,0}}},
  {"Faisca", {{FitaLevel,0},{ForkLevel,90},{Spark,100},{Maintain,100},{Clang,30},{Beat,0},{Attack,2},{Release,800},
   {EchoMix,30},{EchoTime,240},{Dust,30},{RevMix,25},{Output,-12.5f},{-1,0}}},
  {"Dois Garfos Batendo", {{FitaLevel,0},{ForkLevel,90},{Beat,2.2f},{Spark,10},{Clang,40},{Maintain,90},{Attack,2},
   {Release,2000},{RevMix,45},{RevSize,80},{Output,-8.5f},{-1,0}}},
  {"Boing", {{FitaLevel,0},{ForkLevel,95},{Clang,100},{Maintain,0},{Spark,0},{Beat,.6f},{Attack,1},{Release,2500},
   {EchoMix,25},{EchoTime,330},{RevMix,30},{Output,-7},{-1,0}}},
  // ORGAO
  {"Junost no Salao", {{FitaLevel,0},{OrganLevel,85},{Reg16,0},{Reg8,100},{Reg4,70},{OrganTone,55},{TremDepth,35},
   {TremRate,6},{Hum,20},{Attack,3},{Release,150},{RevMix,30},{Output,-8},{-1,0}}},
  {"Case Quebrado", {{FitaLevel,0},{OrganLevel,85},{Reg16,50},{Reg8,100},{Reg4,100},{OrganTone,70},{SpeakerBreak,90},
   {Hum,40},{TremDepth,20},{Attack,3},{Release,150},{RevMix,15},{Output,-9.5f},{-1,0}}},
  {"Quinta Sovietica", {{FitaLevel,0},{OrganLevel,85},{Reg8,100},{Reg4,40},{Upper,80},{UpperInterval,1},{OrganTone,60},
   {TremDepth,45},{TremRate,5},{Attack,3},{Release,200},{RevMix,35},{Output,-7},{-1,0}}},
  {"Terca Torta", {{FitaLevel,0},{OrganLevel,85},{Reg16,60},{Reg8,80},{Upper,90},{UpperInterval,2},{OrganTone,45},
   {TremDepth,60},{TremRate,7.5f},{SpeakerBreak,40},{Hum,30},{EchoMix,20},{Attack,3},{Release,200},{RevMix,30},{Output,-5.5f},{-1,0}}},
  {"Baixo do Clube", {{FitaLevel,0},{OrganLevel,90},{Octave,1},{Reg16,100},{Reg8,70},{Reg4,0},{OrganTone,30},
   {TremDepth,0},{SpeakerBreak,20},{Attack,3},{Release,150},{RevMix,10},{Output,-7.5f},{-1,0}}},
  // Together
  {"Orgao e Fita", {{FitaLevel,50},{OrganLevel,70},{Loop1Wave,4},{Loop2Wave,2},{Wear,40},{Wow,30},{TremDepth,30},
   {Reg8,100},{Reg4,50},{Ensemble,30},{EchoMix,25},{Attack,200},{Release,1500},{RevMix,40},{Output,-7},{-1,0}}},
  {"Garfo sobre Harmonio", {{FitaLevel,0},{ForkLevel,60},{HarmLevel,60},{Beat,.5f},{Era,30},{Sag,40},{Spark,15},
   {Attack,40},{Release,1500},{RevMix,40},{RevSize,75},{Output,-7.5f},{-1,0}}},
  {"Reliquia Inteira", {{FitaLevel,55},{HarmLevel,50},{ForkLevel,50},{OrganLevel,50},{Wear,50},{Wow,35},{TremDepth,30},
   {Spark,20},{Dust,40},{EchoMix,30},{Ensemble,30},{Attack,400},{Release,2500},{RevMix,45},{RevSize,85},{RevDecay,6},{Output,-8.5f},{-1,0}}},
  {"Sotao as 3 da Manha", {{FitaLevel,70},{ForkLevel,40},{Loop1Wave,5},{Loop2Wave,4},{Wear,90},{Wow,70},{Beat,1.5f},
   {Spark,20},{Dust,60},{EchoMix,40},{EchoFeedback,60},{EchoTime,520},{Attack,2500},{Release,6000},{RevMix,60},
   {RevSize,95},{RevDecay,12},{Output,-2.5f},{-1,0}}},
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

} // namespace inverso_abismo::relicario
