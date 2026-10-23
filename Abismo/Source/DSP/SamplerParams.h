#pragma once
// ABISMO SAMPLER parameter table and factory presets. Same conventions as
// AbismoParams.h: IDs are saved in sessions, never rename; only append.
#include "AbismoParams.h"

namespace inverso_abismo::sampler {

namespace P {
enum : int {
 Mode, RootNote, Tune, Start, Reverse, LoopMode, LoopStart, LoopEnd, Xfade,
 Position, Scan, GrainSize, Density, Spray, GrainPitch, GrainReverse, GrainSpread,
 FilterMode, Cutoff, Resonance, FilterEnv, Keytrack, Drive,
 VoiceMix, Vowel, FormantShift,
 Attack, Decay, Sustain, Release, VelSens,
 FAttack, FDecay, FSustain, FRelease,
 PitchEnv, PitchTime, Drift, BendRange,
 LfoRate, LfoPitch, LfoFilter, LfoAmp, LfoPosition,
 Tension, Crush, Downsample, StutterDiv, StutterDepth, Chaos,
 RevMix, RevSize, RevDecay, RevDamp, Freeze, Width, Output,
 Count
};
}

inline const std::array<ParamSpec, P::Count>& specs() {
 static const std::array<ParamSpec, P::Count> table{{
  {"MODE","Playback mode",0,1,0,0,1,Kind::Choice,"Classic|Granular",""},
  {"ROOT_NOTE","Root note",0,127,60,0,1,Kind::Int,nullptr,""},
  {"TUNE","Tune",-24,24,0,0,.01f,Kind::Float,nullptr,"st"},
  {"START","Start",0,100,0,0,.01f,Kind::Float,nullptr,"%"},
  {"REVERSE","Reverse",0,1,0,0,1,Kind::Bool,nullptr,""},
  {"LOOP_MODE","Loop",0,2,0,0,1,Kind::Choice,"Off|Forward|Ping-pong",""},
  {"LOOP_START","Loop start",0,100,20,0,.01f,Kind::Float,nullptr,"%"},
  {"LOOP_END","Loop end",0,100,90,0,.01f,Kind::Float,nullptr,"%"},
  {"XFADE","Loop crossfade",0,100,20,0,.01f,Kind::Float,nullptr,"%"},
  {"POSITION","Grain position",0,100,30,0,.01f,Kind::Float,nullptr,"%"},
  {"SCAN","Scan speed",-200,200,0,0,.01f,Kind::Float,nullptr,"%"},
  {"GRAIN_SIZE","Grain size",5,1000,120,120,.1f,Kind::Float,nullptr,"ms"},
  {"DENSITY","Grain density",1,200,30,30,.01f,Kind::Float,nullptr,"/s"},
  {"SPRAY","Position spray",0,100,10,0,.01f,Kind::Float,nullptr,"%"},
  {"GRAIN_PITCH","Grain pitch random",0,100,0,0,.01f,Kind::Float,nullptr,"%"},
  {"GRAIN_REVERSE","Reverse grains",0,100,0,0,.01f,Kind::Float,nullptr,"%"},
  {"GRAIN_SPREAD","Grain stereo spread",0,100,50,0,.01f,Kind::Float,nullptr,"%"},
  {"FILTER_MODE","Filter type",0,2,0,0,1,Kind::Choice,"Low-pass|Band-pass|High-pass",""},
  {"CUTOFF","Cutoff",20,20000,20000,1000,.1f,Kind::Float,nullptr,"Hz"},
  {"RESONANCE","Resonance",0,100,0,0,.01f,Kind::Float,nullptr,"%"},
  {"FILTER_ENV","Filter env amount",-100,100,0,0,.01f,Kind::Float,nullptr,"%"},
  {"KEYTRACK","Key tracking",0,100,0,0,.01f,Kind::Float,nullptr,"%"},
  {"DRIVE","Drive",0,100,0,0,.01f,Kind::Float,nullptr,"%"},
  {"VOICE_MIX","Formant voice",0,100,0,0,.01f,Kind::Float,nullptr,"%"},
  {"VOWEL","Vowel A-E-I-O-U",0,100,0,0,.01f,Kind::Float,nullptr,"%"},
  {"FORMANT_SHIFT","Formant shift",-12,12,0,0,.01f,Kind::Float,nullptr,"st"},
  {"ATTACK","Attack",1,20000,5,500,.1f,Kind::Float,nullptr,"ms"},
  {"DECAY","Decay",1,20000,600,500,.1f,Kind::Float,nullptr,"ms"},
  {"SUSTAIN","Sustain",0,100,100,0,.01f,Kind::Float,nullptr,"%"},
  {"RELEASE","Release",5,30000,400,1500,.1f,Kind::Float,nullptr,"ms"},
  {"VEL_SENS","Velocity sensitivity",0,100,60,0,.01f,Kind::Float,nullptr,"%"},
  {"F_ATTACK","Filter attack",1,20000,10,500,.1f,Kind::Float,nullptr,"ms"},
  {"F_DECAY","Filter decay",1,20000,800,500,.1f,Kind::Float,nullptr,"ms"},
  {"F_SUSTAIN","Filter sustain",0,100,40,0,.01f,Kind::Float,nullptr,"%"},
  {"F_RELEASE","Filter release",5,30000,900,1500,.1f,Kind::Float,nullptr,"ms"},
  {"PITCH_ENV","Pitch glissando",-24,24,0,0,.01f,Kind::Float,nullptr,"st"},
  {"PITCH_TIME","Glissando time",10,20000,1500,1000,.1f,Kind::Float,nullptr,"ms"},
  {"DRIFT","Drift",0,100,0,0,.01f,Kind::Float,nullptr,"%"},
  {"BEND_RANGE","Pitch bend range",1,24,2,0,1,Kind::Int,nullptr,"st"},
  {"LFO_RATE","LFO rate",.01f,20,.3f,1,.001f,Kind::Float,nullptr,"Hz"},
  {"LFO_PITCH","LFO to pitch",0,100,0,0,.01f,Kind::Float,nullptr,"%"},
  {"LFO_FILTER","LFO to filter",0,100,0,0,.01f,Kind::Float,nullptr,"%"},
  {"LFO_AMP","LFO to amp",0,100,0,0,.01f,Kind::Float,nullptr,"%"},
  {"LFO_POSITION","LFO to grain position",0,100,0,0,.01f,Kind::Float,nullptr,"%"},
  {"TENSION","Tension",0,100,0,0,.01f,Kind::Float,nullptr,"%"},
  {"CRUSH","Bit crush",0,100,0,0,.01f,Kind::Float,nullptr,"%"},
  {"DOWNSAMPLE","Downsample",0,100,0,0,.01f,Kind::Float,nullptr,"%"},
  {"STUTTER_DIV","Stutter",0,6,0,0,1,Kind::Choice,"Off|1/4|1/8|1/16|1/32|1/8T|1/16T",""},
  {"STUTTER_DEPTH","Stutter depth",0,100,60,0,.01f,Kind::Float,nullptr,"%"},
  {"CHAOS","Stutter chaos",0,100,30,0,.01f,Kind::Float,nullptr,"%"},
  {"REV_MIX","Reverb mix",0,100,25,0,.01f,Kind::Float,nullptr,"%"},
  {"REV_SIZE","Reverb size",0,100,70,0,.01f,Kind::Float,nullptr,"%"},
  {"REV_DECAY","Reverb decay",.3f,30,5,4,.01f,Kind::Float,nullptr,"s"},
  {"REV_DAMP","Reverb darkness",0,100,45,0,.01f,Kind::Float,nullptr,"%"},
  {"FREEZE","Reverb freeze",0,1,0,0,1,Kind::Bool,nullptr,""},
  {"WIDTH","Width",0,200,100,0,.01f,Kind::Float,nullptr,"%"},
  {"OUTPUT","Output",-36,12,0,0,.01f,Kind::Float,nullptr,"dB"},
 }};
 return table;
}

using Values = std::array<float, P::Count>;

inline Values defaults() {
 Values v{};
 for (std::size_t i = 0; i < v.size(); ++i) v[i] = specs()[i].def;
 return v;
}

// Presets act on whatever sample is loaded; they are manipulation recipes.
inline const std::array<Preset, 10>& presets() {
 using namespace P;
 static const std::array<Preset, 10> table{{
  {"Init", {{-1, 0}}},
  {"Congelado", {{Mode,1},{Position,40},{Scan,0},{GrainSize,180},{Density,40},{Spray,4},{GrainSpread,80},
   {Attack,800},{Release,3000},{RevMix,45},{RevDecay,10},{Output,-3},{-1,0}}},
  {"Nuvem de Vozes", {{Mode,1},{Position,20},{Scan,8},{GrainSize,260},{Density,60},{Spray,35},{GrainPitch,18},
   {GrainSpread,100},{Drift,30},{Attack,1500},{Release,5000},{RevMix,55},{RevSize,90},{RevDecay,14},{Output,9},{-1,0}}},
  {"Rasgo Reverso", {{Reverse,1},{Start,10},{Cutoff,6000},{Resonance,25},{FilterEnv,-40},{FDecay,1500},{FSustain,0},
   {PitchEnv,-7},{PitchTime,2500},{Attack,10},{Release,1500},{RevMix,40},{RevDecay,6},{-1,0}}},
  {"Gagueira", {{Mode,1},{Position,30},{Scan,100},{GrainSize,60},{Density,25},{Spray,5},{StutterDiv,3},
   {StutterDepth,75},{Chaos,60},{Crush,25},{Attack,3},{Release,300},{RevMix,15},{RevDecay,2},{Output,4},{-1,0}}},
  {"Respiracao Esticada", {{Mode,1},{Position,0},{Scan,12},{GrainSize,400},{Density,25},{Spray,6},{GrainSpread,70},
   {FilterMode,1},{Cutoff,1500},{Resonance,8},{LfoRate,.15f},{LfoFilter,30},{Attack,1200},{Release,4000},
   {RevMix,50},{RevDecay,9},{Output,12},{-1,0}}},
  {"Monstro Grave", {{Tune,-12},{VoiceMix,80},{Vowel,80},{FormantShift,-9},{Drive,60},{Cutoff,3500},
   {LoopMode,1},{Attack,20},{Release,1200},{Drift,35},{RevMix,30},{RevDecay,4},{RevDamp,70},{Output,-1},{-1,0}}},
  {"Fita Derretida", {{LoopMode,2},{LoopStart,30},{LoopEnd,60},{Xfade,40},{Drift,60},{LfoRate,.35f},{LfoPitch,25},
   {Downsample,20},{Crush,30},{Cutoff,4500},{Attack,200},{Release,2500},{RevMix,35},{RevDecay,6},{-1,0}}},
  {"Enxame", {{Mode,1},{Position,50},{Scan,-30},{GrainSize,25},{Density,180},{Spray,60},{GrainPitch,45},
   {GrainReverse,50},{GrainSpread,100},{FilterMode,2},{Cutoff,400},{Attack,600},{Release,2500},{RevMix,40},
   {RevDecay,7},{Output,9},{-1,0}}},
  {"Boca Fechada", {{Mode,1},{Position,25},{Scan,20},{GrainSize,90},{Density,45},{VoiceMix,100},{Vowel,95},
   {LfoRate,.5f},{LfoFilter,60},{LfoPosition,25},{Attack,300},{Release,2000},{RevMix,35},{RevDecay,5},{Output,0},{-1,0}}},
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

} // namespace inverso_abismo::sampler
