#pragma once
// Portable parameter table and factory presets. No JUCE dependency: the plugin
// builds its AudioProcessorValueTreeState from this table and the Linux tests
// drive the engine with the same values, so both always agree on IDs/ranges.
#include <array>
#include <cstddef>
#include <cstring>

namespace inverso_abismo {

enum class Kind { Float, Choice, Bool, Int };

struct ParamSpec {
 const char* id;
 const char* name;
 float min, max, def;
 float centre;   // >0: skew the host range so this value sits at the knob centre
 float interval;
 Kind kind;
 const char* choices; // '|' separated, Choice only
 const char* unit;
};

constexpr int numMaterials = 24;
constexpr const char* materialChoices = "Corda|Madeira|Metal|Vidro|Sino|Abismo|Marimba|Cano|Membrana|Gongo|Tigela tibetana|Mola|Corda de piano|Gamelao|Prato|Osso|Acorde menor|Cluster|Tritono|Ferrugem|Corpo de madeira|Chapa de trovao|Subterraneo|Sino rachado";

namespace P {
enum : int {
 OscShape, OscLevel, Octave, Unison, Detune, Spread, SubLevel,
 ClusterMode, ClusterLevel, NoiseLevel, Tension,
 ResMaterial, ResLevel, ResDecay, Bow, Scrape, Strike, NoiseTone,
 FilterMode, Cutoff, Resonance, FilterEnv, Keytrack, Drive,
 Attack, Decay, Sustain, Release, VelSens,
 FAttack, FDecay, FSustain, FRelease,
 PitchEnv, PitchTime, Drift, Glide, BendRange,
 LfoRate, LfoPitch, LfoFilter, LfoAmp,
 RevMix, RevSize, RevDecay, RevDamp, Freeze, Width, Output,
 VoiceMix, Vowel, FormantShift,
 Crush, Downsample, StutterDiv, StutterDepth, Chaos,
 // 0.8: a deeper resonator and more ways to break the sound.
 ResMaterialB, ResMorph, ResTune, ResKeytrack, ResStretch, ResBright, ResStereo, ResInput, ResRattle,
 DriveMode, RingMix, RingPitch, Reverse, RevReverse, Warp,
 StutterSync, StutterMs,
 Count
};
}

// IDs are part of saved sessions: never rename or reorder meaning, only append.
inline const std::array<ParamSpec, P::Count>& specs() {
 static const std::array<ParamSpec, P::Count> table{{
  {"OSC_SHAPE","Osc shape",0,100,66,0,.01f,Kind::Float,nullptr,"%"},
  {"OSC_LEVEL","Osc level",0,100,70,0,.01f,Kind::Float,nullptr,"%"},
  {"OCTAVE","Octave",0,4,2,0,1,Kind::Choice,"-2|-1|0|+1|+2",""},
  {"UNISON","Unison voices",0,6,2,0,1,Kind::Choice,"1|2|3|4|5|6|7",""},
  {"DETUNE","Unison detune",0,100,18,0,.01f,Kind::Float,nullptr,"%"},
  {"SPREAD","Stereo spread",0,100,60,0,.01f,Kind::Float,nullptr,"%"},
  {"SUB_LEVEL","Sub level",0,100,0,0,.01f,Kind::Float,nullptr,"%"},
  {"CLUSTER_MODE","Cluster",0,6,0,0,1,Kind::Choice,
   "Off|Minor 2nd|Tritone|Minor 2nd + Tritone|Quarter tones|Minor 9th + Tritone|Major 7th + Minor 2nd",""},
  {"CLUSTER_LEVEL","Cluster level",0,100,50,0,.01f,Kind::Float,nullptr,"%"},
  {"NOISE_LEVEL","Breath noise",0,100,0,0,.01f,Kind::Float,nullptr,"%"},
  {"TENSION","Tension",0,100,0,0,.01f,Kind::Float,nullptr,"%"},
  {"RES_MATERIAL","Material",0,numMaterials - 1,2,0,1,Kind::Choice,materialChoices,""},
  {"RES_LEVEL","Resonator level",0,100,0,0,.01f,Kind::Float,nullptr,"%"},
  {"RES_DECAY","Resonator decay",.05f,15,2,1.5f,.001f,Kind::Float,nullptr,"s"},
  {"BOW","Bow pressure",0,100,40,0,.01f,Kind::Float,nullptr,"%"},
  {"SCRAPE","Scrape",0,100,0,0,.01f,Kind::Float,nullptr,"%"},
  {"STRIKE","Strike",0,100,30,0,.01f,Kind::Float,nullptr,"%"},
  {"NOISE_TONE","Exciter tone",0,100,50,0,.01f,Kind::Float,nullptr,"%"},
  {"FILTER_MODE","Filter type",0,2,0,0,1,Kind::Choice,"Low-pass|Band-pass|High-pass",""},
  {"CUTOFF","Cutoff",20,20000,3500,1000,.1f,Kind::Float,nullptr,"Hz"},
  {"RESONANCE","Resonance",0,100,15,0,.01f,Kind::Float,nullptr,"%"},
  {"FILTER_ENV","Filter env amount",-100,100,20,0,.01f,Kind::Float,nullptr,"%"},
  {"KEYTRACK","Key tracking",0,100,30,0,.01f,Kind::Float,nullptr,"%"},
  {"DRIVE","Drive",0,100,0,0,.01f,Kind::Float,nullptr,"%"},
  {"ATTACK","Attack",1,20000,15,500,.1f,Kind::Float,nullptr,"ms"},
  {"DECAY","Decay",1,20000,600,500,.1f,Kind::Float,nullptr,"ms"},
  {"SUSTAIN","Sustain",0,100,80,0,.01f,Kind::Float,nullptr,"%"},
  {"RELEASE","Release",5,30000,900,1500,.1f,Kind::Float,nullptr,"ms"},
  {"VEL_SENS","Velocity sensitivity",0,100,60,0,.01f,Kind::Float,nullptr,"%"},
  {"F_ATTACK","Filter attack",1,20000,10,500,.1f,Kind::Float,nullptr,"ms"},
  {"F_DECAY","Filter decay",1,20000,800,500,.1f,Kind::Float,nullptr,"ms"},
  {"F_SUSTAIN","Filter sustain",0,100,40,0,.01f,Kind::Float,nullptr,"%"},
  {"F_RELEASE","Filter release",5,30000,900,1500,.1f,Kind::Float,nullptr,"ms"},
  {"PITCH_ENV","Pitch glissando",-24,24,0,0,.01f,Kind::Float,nullptr,"st"},
  {"PITCH_TIME","Glissando time",10,20000,1500,1000,.1f,Kind::Float,nullptr,"ms"},
  {"DRIFT","Drift",0,100,10,0,.01f,Kind::Float,nullptr,"%"},
  {"GLIDE","Glide",0,5000,0,300,.1f,Kind::Float,nullptr,"ms"},
  {"BEND_RANGE","Pitch bend range",1,24,2,0,1,Kind::Int,nullptr,"st"},
  {"LFO_RATE","LFO rate",.01f,20,.3f,1,.001f,Kind::Float,nullptr,"Hz"},
  {"LFO_PITCH","LFO to pitch",0,100,0,0,.01f,Kind::Float,nullptr,"%"},
  {"LFO_FILTER","LFO to filter",0,100,0,0,.01f,Kind::Float,nullptr,"%"},
  {"LFO_AMP","LFO to amp",0,100,0,0,.01f,Kind::Float,nullptr,"%"},
  {"REV_MIX","Reverb mix",0,100,30,0,.01f,Kind::Float,nullptr,"%"},
  {"REV_SIZE","Reverb size",0,100,70,0,.01f,Kind::Float,nullptr,"%"},
  {"REV_DECAY","Reverb decay",.3f,30,6,4,.01f,Kind::Float,nullptr,"s"},
  {"REV_DAMP","Reverb darkness",0,100,45,0,.01f,Kind::Float,nullptr,"%"},
  {"FREEZE","Reverb freeze",0,1,0,0,1,Kind::Bool,nullptr,""},
  {"WIDTH","Width",0,200,100,0,.01f,Kind::Float,nullptr,"%"},
  {"OUTPUT","Output",-36,6,-6,0,.01f,Kind::Float,nullptr,"dB"},
  {"VOICE_MIX","Formant voice",0,100,0,0,.01f,Kind::Float,nullptr,"%"},
  {"VOWEL","Vowel A-E-I-O-U",0,100,0,0,.01f,Kind::Float,nullptr,"%"},
  {"FORMANT_SHIFT","Formant shift",-12,12,0,0,.01f,Kind::Float,nullptr,"st"},
  {"CRUSH","Bit crush",0,100,0,0,.01f,Kind::Float,nullptr,"%"},
  {"DOWNSAMPLE","Downsample",0,100,0,0,.01f,Kind::Float,nullptr,"%"},
  {"STUTTER_DIV","Stutter",0,6,0,0,1,Kind::Choice,"Off|1/4|1/8|1/16|1/32|1/8T|1/16T",""},
  {"STUTTER_DEPTH","Stutter depth",0,100,60,0,.01f,Kind::Float,nullptr,"%"},
  {"CHAOS","Stutter chaos",0,100,30,0,.01f,Kind::Float,nullptr,"%"},
  {"RES_MATERIAL_B","Material B",0,numMaterials - 1,5,0,1,Kind::Choice,materialChoices,""},
  {"RES_MORPH","Material morph",0,100,0,0,.01f,Kind::Float,nullptr,"%"},
  {"RES_TUNE","Resonator tune",-24,24,0,0,.01f,Kind::Float,nullptr,"st"},
  {"RES_KEYTRACK","Resonator keytrack",0,100,100,0,.01f,Kind::Float,nullptr,"%"},
  {"RES_STRETCH","Partial stretch",-100,100,0,0,.01f,Kind::Float,nullptr,"%"},
  {"RES_BRIGHT","Resonator brightness",0,100,50,0,.01f,Kind::Float,nullptr,"%"},
  {"RES_STEREO","Resonator stereo",0,100,40,0,.01f,Kind::Float,nullptr,"%"},
  {"RES_INPUT","Osc into resonator",0,100,0,0,.01f,Kind::Float,nullptr,"%"},
  {"RES_RATTLE","Rattle",0,100,0,0,.01f,Kind::Float,nullptr,"%"},
  {"DRIVE_MODE","Drive type",0,4,0,0,1,Kind::Choice,"Fita|Fuzz|Dobra|Retificador|Estilhaco",""},
  {"RING_MIX","Ring mod",0,100,0,0,.01f,Kind::Float,nullptr,"%"},
  {"RING_PITCH","Ring pitch",-24,48,7,0,.01f,Kind::Float,nullptr,"st"},
  {"REVERSE","Reverse slices",0,100,0,0,.01f,Kind::Float,nullptr,"%"},
  {"REV_REVERSE","Reverse reverb",0,100,0,0,.01f,Kind::Float,nullptr,"%"},
  {"WARP","Tape warp",0,100,0,0,.01f,Kind::Float,nullptr,"%"},
  {"STUTTER_SYNC","Stutter timing",0,1,0,0,1,Kind::Choice,"BPM|Livre",""},
  {"STUTTER_MS","Stutter time",10,2000,125,250,.1f,Kind::Float,nullptr,"ms"},
 }};
 return table;
}

using Values = std::array<float, P::Count>;

inline Values defaults() {
 Values v{};
 for (std::size_t i = 0; i < v.size(); ++i) v[i] = specs()[i].def;
 return v;
}

inline int indexOf(const char* id) {
 for (int i = 0; i < P::Count; ++i)
  if (std::strcmp(specs()[static_cast<std::size_t>(i)].id, id) == 0) return i;
 return -1;
}

struct PresetValue { int param; float value; };
struct Preset { const char* name; PresetValue values[48]; };

// Each preset starts from the defaults and overrides only what it lists.
// Value lists end at the first entry with param == -1 (zero-initialised tail
// entries have param 0, so every list is explicitly terminated).
inline const std::array<Preset, 40>& presets() {
 using namespace P;
 static const std::array<Preset, 40> table{{
  {"Init", {{-1, 0}}},
  {"Cordas Mortas", {{OscShape,62},{Unison,4},{Detune,30},{Spread,85},{ClusterMode,1},{ClusterLevel,45},
   {Cutoff,1400},{Resonance,10},{FilterEnv,15},{Attack,2200},{Release,4500},{Sustain,90},
   {Drift,35},{LfoRate,.17f},{LfoPitch,8},{RevMix,45},{RevDecay,9},{RevDamp,60},{Output,-5.5},{-1,0}}},
  {"Arco no Metal", {{OscLevel,0},{ResLevel,90},{ResMaterial,2},{ResDecay,4},{Bow,85},{Scrape,15},{Strike,0},
   {NoiseTone,70},{Cutoff,9000},{Resonance,5},{FilterEnv,0},{Attack,900},{Release,5000},{Sustain,100},
   {Drift,20},{RevMix,50},{RevDecay,12},{RevDamp,35},{Output,-15},{-1,0}}},
  {"Vidro Rachado", {{OscShape,0},{OscLevel,20},{ResLevel,85},{ResMaterial,3},{ResDecay,6},{Bow,0},{Strike,95},
   {Scrape,6},{Cutoff,12000},{FilterEnv,0},{Attack,1},{Decay,6000},{Sustain,0},{Release,6000},
   {Drift,25},{RevMix,40},{RevDecay,8},{RevDamp,20},{Output,0},{-1,0}}},
  {"Queda Livre", {{OscShape,70},{Unison,5},{Detune,40},{ClusterMode,2},{ClusterLevel,60},{PitchEnv,-12},
   {PitchTime,6000},{Cutoff,2500},{Resonance,30},{FilterEnv,-30},{FDecay,6000},{FSustain,0},{Attack,40},
   {Sustain,100},{Release,3000},{Drift,15},{RevMix,40},{RevDecay,10},{Output,-9},{-1,0}}},
  {"Drone do Abismo", {{OscShape,55},{Octave,1},{Unison,3},{Detune,22},{SubLevel,70},{ClusterMode,3},
   {ClusterLevel,35},{NoiseLevel,8},{Cutoff,450},{Resonance,35},{FilterEnv,0},{LfoRate,.07f},{LfoFilter,35},
   {Attack,4000},{Sustain,100},{Release,9000},{Drift,45},{RevMix,55},{RevSize,95},{RevDecay,18},
   {RevDamp,75},{Output,-5},{-1,0}}},
  {"Sussurro no Porao", {{OscLevel,0},{NoiseLevel,80},{FilterMode,1},{Cutoff,900},{Resonance,55},
   {Keytrack,100},{FilterEnv,25},{FAttack,1500},{FDecay,3000},{ResLevel,25},{ResMaterial,5},{Bow,60},
   {Attack,1200},{Sustain,85},{Release,3500},{LfoRate,.4f},{LfoFilter,30},{RevMix,55},{RevDecay,7},
   {Output,6},{-1,0}}},
  {"Caixa de Musica Quebrada", {{OscShape,8},{OscLevel,55},{Octave,3},{ResLevel,50},{ResMaterial,4},
   {Strike,70},{Bow,0},{ResDecay,3},{Attack,1},{Decay,2500},{Sustain,0},{Release,2500},{Drift,70},
   {LfoRate,4.5f},{LfoPitch,18},{Cutoff,7000},{FilterEnv,0},{RevMix,45},{RevDecay,6},{Output,0},{-1,0}}},
  {"Batimento", {{OscShape,30},{Octave,1},{SubLevel,60},{Cutoff,300},{Resonance,40},{FilterEnv,40},
   {FAttack,5},{FDecay,250},{FSustain,0},{LfoRate,1.1f},{LfoAmp,95},{Attack,20},{Sustain,100},{Release,1200},
   {RevMix,25},{RevDecay,4},{RevDamp,70},{Output,-10},{-1,0}}},
  {"Tensao Infinita", {{OscShape,66},{Unison,6},{Detune,35},{Spread,100},{ClusterMode,6},{ClusterLevel,55},
   {PitchEnv,7},{PitchTime,15000},{Tension,40},{Cutoff,800},{FilterEnv,70},{FAttack,14000},{FSustain,100},
   {Attack,6000},{Sustain,100},{Release,5000},{Drift,30},{RevMix,50},{RevDecay,14},{Output,-6},{-1,0}}},
  {"Coro Afogado", {{OscShape,40},{Unison,6},{Detune,26},{Spread,90},{ClusterMode,4},{ClusterLevel,30},
   {NoiseLevel,20},{FilterMode,1},{Cutoff,700},{Resonance,55},{Keytrack,70},{FilterEnv,0},{LfoRate,.25f},
   {LfoFilter,25},{LfoPitch,6},{Attack,2500},{Sustain,100},{Release,6000},{Drift,40},{RevMix,65},
   {RevSize,90},{RevDecay,15},{RevDamp,65},{Output,-3},{-1,0}}},
  {"Corrente Arrastada", {{OscLevel,10},{OscShape,90},{ResLevel,80},{ResMaterial,5},{ResDecay,1.2f},{Bow,20},
   {Scrape,70},{Strike,40},{NoiseTone,85},{Drive,45},{Cutoff,5000},{Resonance,20},{FilterEnv,0},{Attack,5},
   {Sustain,100},{Release,1800},{Drift,50},{RevMix,40},{RevDecay,5},{RevDamp,40},{Output,-12},{-1,0}}},
  // Processed-voice presets: formant mouths, crushed and chopped in host tempo.
  {"Sorriso Torto", {{OscShape,70},{Unison,2},{Detune,25},{NoiseLevel,25},{VoiceMix,100},{Vowel,20},
   {FormantShift,-5},{Cutoff,6000},{FilterEnv,0},{Drive,55},{Crush,35},{StutterDiv,3},{StutterDepth,70},
   {Chaos,45},{PitchEnv,-3},{PitchTime,900},{Attack,5},{Sustain,100},{Release,400},{Drift,40},{RevMix,20},
   {RevDecay,2.5f},{Output,-6},{-1,0}}},
  {"Grunhido", {{OscShape,85},{Octave,1},{SubLevel,40},{NoiseLevel,45},{VoiceMix,100},{Vowel,75},
   {FormantShift,-10},{Drive,80},{Cutoff,4000},{FilterEnv,0},{LfoRate,6.5f},{LfoFilter,20},{LfoPitch,10},
   {Attack,30},{Sustain,100},{Release,600},{Drift,60},{RevMix,25},{RevDecay,3},{RevDamp,60},{Output,-6},{-1,0}}},
  {"Respiracao Picotada", {{OscLevel,0},{NoiseLevel,100},{VoiceMix,100},{Vowel,45},{FormantShift,2},
   {Cutoff,9000},{FilterEnv,0},{Keytrack,0},{StutterDiv,4},{StutterDepth,80},{Chaos,70},{Downsample,25},
   {LfoRate,.6f},{LfoFilter,40},{Attack,10},{Sustain,100},{Release,300},{RevMix,30},{RevDecay,3},
   {Output,6},{-1,0}}},
  {"Lamento Mastigado", {{OscShape,45},{Unison,3},{Detune,20},{ClusterMode,1},{ClusterLevel,40},
   {VoiceMix,90},{Vowel,90},{FormantShift,4},{Cutoff,5000},{FilterEnv,0},{PitchEnv,-5},{PitchTime,4000},
   {Crush,55},{Downsample,40},{StutterDiv,6},{StutterDepth,50},{Chaos,35},{LfoRate,5},{LfoPitch,15},
   {Attack,400},{Sustain,100},{Release,2500},{Drift,45},{RevMix,40},{RevDecay,7},{Output,0},{-1,0}}},
  // 0.8: the deeper resonator, more destruction, things played backwards.
  {"Tigela do Exorcismo", {{OscLevel,0},{ResLevel,90},{ResMaterial,10},{ResDecay,9},{Bow,70},{Strike,30},{NoiseTone,30},
   {ResStereo,85},{ResBright,40},{Cutoff,12000},{FilterEnv,0},{Attack,600},{Sustain,100},{Release,6000},{Drift,15},
   {RevMix,50},{RevDecay,12},{RevDamp,40},{Output,-12.3f},{-1,0}}},
  {"Gongo ao Contrario", {{OscLevel,0},{ResLevel,95},{ResMaterial,9},{ResDecay,8},{Bow,0},{Strike,100},{NoiseTone,60},
   {ResStereo,60},{Cutoff,14000},{FilterEnv,0},{Attack,1},{Decay,8000},{Sustain,0},{Release,8000},{RevMix,70},
   {RevReverse,100},{RevSize,85},{RevDecay,10},{RevDamp,30},{Output,2},{-1,0}}},
  {"Piano Enterrado", {{OscShape,10},{OscLevel,25},{ResLevel,85},{ResMaterial,12},{ResMaterialB,22},{ResMorph,35},
   {ResInput,40},{Strike,70},{Bow,0},{ResDecay,6},{Cutoff,2500},{Resonance,10},{FilterEnv,0},{Attack,2},{Decay,5000},
   {Sustain,0},{Release,4000},{Warp,35},{Drift,20},{RevMix,45},{RevDecay,8},{RevDamp,70},{Output,4},{-1,0}}},
  {"Corda Enferrujada", {{OscLevel,0},{ResLevel,90},{ResMaterial,19},{ResDecay,3},{Bow,80},{Scrape,30},{Strike,10},
   {NoiseTone,40},{ResRattle,30},{Drive,25},{DriveMode,1},{Cutoff,3500},{FilterEnv,0},{Attack,400},{Sustain,100},
   {Release,2500},{Drift,30},{RevMix,35},{RevDecay,5},{Output,-16},{-1,0}}},
  {"Mola do Porao", {{OscLevel,0},{ResLevel,90},{ResMaterial,11},{ResDecay,4},{ResStretch,40},{ResRattle,60},{Strike,90},
   {Bow,0},{Scrape,20},{Cutoff,9000},{FilterEnv,0},{Attack,1},{Decay,3500},{Sustain,0},{Release,3500},{Reverse,30},
   {RevMix,45},{RevSize,80},{RevDecay,6},{RevDamp,55},{Output,-5},{-1,0}}},
  {"Coral de Ossos", {{OscShape,66},{OscLevel,50},{Unison,3},{Detune,25},{ResLevel,70},{ResMaterial,15},{ResMaterialB,16},
   {ResMorph,50},{ResInput,60},{Bow,30},{ResDecay,2.5f},{VoiceMix,60},{Vowel,80},{FormantShift,-3},{Cutoff,5000},
   {FilterEnv,0},{Attack,900},{Sustain,100},{Release,3500},{Drift,35},{RevMix,50},{RevDecay,9},{Output,-11},{-1,0}}},
  {"Trovao de Chapa", {{OscLevel,0},{ResLevel,95},{ResMaterial,21},{ResDecay,6},{ResKeytrack,30},{ResTune,-12},
   {Strike,100},{Scrape,40},{Bow,30},{NoiseTone,40},{Drive,40},{Cutoff,8000},{FilterEnv,0},{Attack,1},{Decay,7000},
   {Sustain,20},{Release,6000},{RevMix,50},{RevSize,90},{RevDecay,9},{RevDamp,50},{Output,-8},{-1,0}}},
  {"Sino Rachado do Campanario", {{OscLevel,0},{ResLevel,95},{ResMaterial,23},{ResDecay,10},{Strike,100},{Bow,0},
   {NoiseTone,50},{ResRattle,15},{ResStereo,70},{Cutoff,15000},{FilterEnv,0},{Attack,1},{Decay,10000},{Sustain,0},
   {Release,10000},{RevMix,55},{RevSize,85},{RevDecay,14},{RevDamp,70},{Output,-6},{-1,0}}},
  {"Cluster Respirando", {{OscShape,40},{OscLevel,40},{Unison,2},{Detune,15},{ResLevel,80},{ResMaterial,17},{ResInput,80},
   {Bow,30},{ResDecay,4},{NoiseLevel,15},{Cutoff,3000},{FilterEnv,0},{LfoRate,.2f},{LfoFilter,30},{Attack,3000},
   {Sustain,100},{Release,5000},{Drift,25},{RevMix,50},{RevDecay,10},{Output,-11.5f},{-1,0}}},
  {"Gamelao Fantasma", {{OscLevel,0},{ResLevel,90},{ResMaterial,13},{ResDecay,5},{Strike,90},{Bow,0},{NoiseTone,65},
   {ResStereo,60},{Cutoff,12000},{FilterEnv,0},{Attack,1},{Decay,4000},{Sustain,0},{Release,4000},{Warp,25},
   {RevReverse,60},{RevMix,55},{RevDecay,8},{Output,4},{-1,0}}},
  {"Radio Morto", {{OscShape,80},{OscLevel,60},{NoiseLevel,30},{VoiceMix,70},{Vowel,40},{RingMix,60},{RingPitch,31},
   {Drive,60},{DriveMode,4},{Crush,40},{Downsample,30},{Warp,50},{Reverse,30},{StutterDiv,3},{StutterDepth,40},
   {Chaos,40},{Cutoff,5000},{FilterEnv,0},{Attack,10},{Sustain,100},{Release,500},{RevMix,25},{RevDecay,3},{Output,-2},{-1,0}}},
  {"Fuzz Infernal", {{OscShape,85},{Unison,4},{Detune,30},{Octave,1},{SubLevel,40},{Drive,90},{DriveMode,1},
   {Cutoff,1800},{Resonance,40},{FilterEnv,30},{FDecay,1500},{FSustain,30},{Attack,10},{Sustain,100},{Release,700},
   {Drift,20},{RevMix,20},{RevDecay,3},{Output,-12.6f},{-1,0}}},
  {"Dobra Cerebral", {{OscShape,0},{OscLevel,90},{Unison,2},{Detune,10},{ClusterMode,1},{ClusterLevel,40},{Drive,80},
   {DriveMode,2},{Cutoff,7000},{FilterEnv,0},{LfoRate,.13f},{LfoPitch,10},{Tension,20},{Attack,800},{Sustain,100},
   {Release,2500},{RevMix,35},{RevDecay,6},{Output,-11.8f},{-1,0}}},
  {"Retificador de Almas", {{OscShape,40},{OscLevel,60},{Drive,70},{DriveMode,3},{ResLevel,40},{ResMaterial,5},
   {ResInput,50},{ResDecay,3},{Bow,20},{Cutoff,4500},{Resonance,25},{FilterEnv,0},{Attack,300},{Sustain,100},
   {Release,2000},{Drift,30},{RevMix,35},{RevDecay,6},{Output,-7},{-1,0}}},
  {"Lembranca Derretida", {{OscShape,66},{Unison,3},{Detune,20},{ClusterMode,4},{ClusterLevel,40},{Warp,80},
   {RevReverse,40},{Cutoff,2200},{Resonance,15},{FilterEnv,0},{Attack,1500},{Sustain,100},{Release,4000},{Drift,40},
   {RevMix,50},{RevDecay,9},{RevDamp,60},{Output,-6},{-1,0}}},
  {"Reverso Sussurrado", {{OscLevel,0},{NoiseLevel,70},{VoiceMix,100},{Vowel,30},{FormantShift,3},{Reverse,100},
   {RevReverse,70},{Cutoff,9000},{FilterEnv,0},{Keytrack,0},{Attack,200},{Sustain,100},{Release,1500},{RevMix,60},
   {RevDecay,6},{Output,6},{-1,0}}},
  {"Picotado ao Contrario", {{OscShape,66},{Unison,2},{Detune,20},{StutterDiv,3},{StutterDepth,50},{Chaos,50},
   {Reverse,50},{Crush,20},{Cutoff,6000},{FilterEnv,20},{Attack,5},{Sustain,100},{Release,300},{RevMix,25},
   {RevDecay,3},{Output,-6},{-1,0}}},
  {"Anel de Fogo", {{OscShape,50},{OscLevel,70},{RingMix,80},{RingPitch,13},{ResLevel,40},{ResMaterial,2},{Bow,50},
   {ResDecay,3},{Drive,30},{DriveMode,1},{Cutoff,6000},{FilterEnv,0},{Attack,50},{Sustain,100},{Release,1500},
   {RevMix,30},{RevDecay,5},{Output,-11.3f},{-1,0}}},
  {"Marimba do Sotao", {{OscLevel,0},{ResLevel,90},{ResMaterial,6},{ResDecay,2.5f},{Strike,85},{Bow,0},{NoiseTone,45},
   {ResStereo,70},{Cutoff,10000},{FilterEnv,0},{Attack,1},{Decay,2500},{Sustain,0},{Release,2500},{Warp,20},
   {RevMix,40},{RevDecay,5},{Output,-2},{-1,0}}},
  {"Corpo Oco", {{OscShape,66},{OscLevel,60},{ResLevel,80},{ResMaterial,20},{ResKeytrack,0},{ResTune,-5},{ResInput,70},
   {ResDecay,1.5f},{Bow,10},{Cutoff,6000},{FilterEnv,0},{Attack,30},{Sustain,100},{Release,1200},{RevMix,30},
   {RevDecay,4},{Output,-8},{-1,0}}},
  {"Subterraneo", {{OscShape,30},{OscLevel,50},{Octave,1},{SubLevel,50},{ResLevel,80},{ResMaterial,22},{ResInput,50},
   {ResDecay,6},{Bow,20},{Cutoff,900},{Resonance,20},{FilterEnv,0},{Attack,2500},{Sustain,100},{Release,7000},
   {Drift,30},{RevMix,55},{RevSize,95},{RevDecay,16},{RevDamp,75},{Output,-14.6f},{-1,0}}},
  {"Tritono Pendular", {{OscLevel,0},{ResLevel,90},{ResMaterial,18},{ResDecay,5},{Bow,60},{Strike,20},{NoiseTone,35},
   {Cutoff,9000},{FilterEnv,0},{LfoRate,.15f},{LfoPitch,10},{Attack,2000},{Sustain,100},{Release,5000},{RevMix,50},
   {RevDecay,10},{Output,-10},{-1,0}}},
  {"Membrana Viva", {{OscLevel,0},{ResLevel,90},{ResMaterial,8},{ResDecay,2},{ResStretch,-30},{ResRattle,30},
   {ResKeytrack,70},{Strike,80},{Bow,25},{Cutoff,8000},{FilterEnv,0},{Attack,1},{Decay,3000},{Sustain,30},
   {Release,2000},{RevMix,35},{RevDecay,4},{Output,-6},{-1,0}}},
  {"Colapso Total", {{OscShape,75},{Unison,6},{Detune,40},{ClusterMode,3},{ClusterLevel,60},{ResLevel,50},
   {ResMaterial,9},{ResInput,40},{ResRattle,50},{Drive,80},{DriveMode,4},{RingMix,40},{RingPitch,6},{Crush,50},
   {Downsample,40},{StutterDiv,6},{StutterDepth,60},{Chaos,80},{Reverse,40},{Warp,70},{RevReverse,50},{Cutoff,5000},
   {FilterEnv,0},{Attack,20},{Sustain,100},{Release,1500},{RevMix,40},{RevDecay,6},{Output,-3},{-1,0}}},
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

} // namespace inverso_abismo
