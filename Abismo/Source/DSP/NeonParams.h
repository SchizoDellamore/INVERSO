#pragma once
// NEON: a polysynth for synthwave, with a little of each machine the style
// grew from: two analog oscillators (saw, PWM pulse, triangle, sine, sync and
// cross-mod) with sub and noise, a 2-operator FM layer for the 80s electric
// pianos, bells and basses, a 24 dB ladder or a bright 12 dB filter, unison,
// an arpeggiator and a 16-step bass sequencer, and the period's chorus, delay
// and reverb. Portable parameter table and factory presets (no JUCE).
#include "AbismoParams.h"
#include <array>
#include <cstddef>

namespace inverso_abismo::neon {

using inverso_abismo::Kind;
using inverso_abismo::ParamSpec;

constexpr int seqSteps = 16;

namespace P {
enum : int {
 // oscillators
 Osc1Wave, Osc1Octave, Osc1PW, Osc1Level,
 Osc2Wave, Osc2Octave, Osc2Semi, Osc2Fine, Osc2Sync, Osc2Level,
 SubLevel, NoiseLevel, CrossMod, PwmDepth,
 FmLevel, FmRatio, FmIndex, FmDecay, FmVelocity,
 // filter
 FilterType, Cutoff, Resonance, FilterEnv, Keytrack, FilterVel, FilterDrive,
 // envelopes
 Attack, Decay, Sustain, Release,
 FAttack, FDecay, FSustain, FRelease,
 // LFO
 LfoRate, LfoShape, LfoSync, LfoDelay, LfoPitch, LfoFilter, LfoAmp,
 // voice
 VoiceMode, Unison, Detune, Spread, Glide, VelSens, BendRange, Octave, Drift,
 // pattern: arpeggiator or bass sequencer
 Pattern, PatternRate, ArpMode, ArpOctaves, Gate, Swing, Latch, SeqLength,
 SeqNote0,
 SeqAccent0 = SeqNote0 + seqSteps,
 SeqSlide0 = SeqAccent0 + seqSteps,
 SeqOn0 = SeqSlide0 + seqSteps,
 // effects
 ChorusMode = SeqOn0 + seqSteps, ChorusMix,
 DelayMix, DelaySync, DelayTime, DelayFeedback, DelayPingPong, DelayTone,
 RevMix, RevType, RevSize, RevDecay, RevDamp,
 Width, Output,
 PressureCC,
 Count
};
}

inline const char* waveNames1() { return "Saw|Pulse|Triangle|Sine"; }
inline const char* rateNames() { return "1/4|1/8|1/8T|1/16|1/16T|1/32"; }

// IDs are part of saved sessions: never rename or reorder meaning, only append.
inline const std::array<ParamSpec, P::Count>& specs() {
 static const std::array<ParamSpec, P::Count> table = [] {
  std::array<ParamSpec, P::Count> t{};
  auto at = [&](int p, ParamSpec s) { t[static_cast<std::size_t>(p)] = s; };
  using namespace P;
  at(Osc1Wave, {"OSC1_WAVE","Oscillator 1 wave",0,3,0,0,1,Kind::Choice,waveNames1(),""});
  at(Osc1Octave, {"OSC1_OCTAVE","Oscillator 1 octave",-2,2,0,0,1,Kind::Int,nullptr,"oct"});
  at(Osc1PW, {"OSC1_PW","Oscillator 1 pulse width",5,95,50,0,.01f,Kind::Float,nullptr,"%"});
  at(Osc1Level, {"OSC1_LEVEL","Oscillator 1 level",0,100,80,0,.01f,Kind::Float,nullptr,"%"});
  at(Osc2Wave, {"OSC2_WAVE","Oscillator 2 wave",0,3,0,0,1,Kind::Choice,waveNames1(),""});
  at(Osc2Octave, {"OSC2_OCTAVE","Oscillator 2 octave",-2,2,0,0,1,Kind::Int,nullptr,"oct"});
  at(Osc2Semi, {"OSC2_SEMI","Oscillator 2 semitones",-12,12,0,0,1,Kind::Int,nullptr,"st"});
  at(Osc2Fine, {"OSC2_FINE","Oscillator 2 fine",-50,50,7,0,.01f,Kind::Float,nullptr,"ct"});
  at(Osc2Sync, {"OSC2_SYNC","Oscillator 2 sync to 1",0,1,0,0,1,Kind::Bool,nullptr,""});
  at(Osc2Level, {"OSC2_LEVEL","Oscillator 2 level",0,100,0,0,.01f,Kind::Float,nullptr,"%"});
  at(SubLevel, {"SUB_LEVEL","Sub oscillator",0,100,0,0,.01f,Kind::Float,nullptr,"%"});
  at(NoiseLevel, {"NOISE_LEVEL","Noise",0,100,0,0,.01f,Kind::Float,nullptr,"%"});
  at(CrossMod, {"CROSS_MOD","Cross modulation 2 > 1",0,100,0,0,.01f,Kind::Float,nullptr,"%"});
  at(PwmDepth, {"PWM","Pulse width modulation (LFO)",0,100,0,0,.01f,Kind::Float,nullptr,"%"});
  at(FmLevel, {"FM_LEVEL","FM layer level",0,100,0,0,.01f,Kind::Float,nullptr,"%"});
  at(FmRatio, {"FM_RATIO","FM modulator ratio",0,9,2,0,1,Kind::Choice,"0.5|1|2|3|4|5|7|9|11|14",""});
  at(FmIndex, {"FM_INDEX","FM brightness",0,100,40,0,.01f,Kind::Float,nullptr,"%"});
  at(FmDecay, {"FM_DECAY","FM brightness decay",5,8000,600,600,.1f,Kind::Float,nullptr,"ms"});
  at(FmVelocity, {"FM_VELOCITY","FM brightness from velocity",0,100,60,0,.01f,Kind::Float,nullptr,"%"});
  at(FilterType, {"FILTER_TYPE","Filter type",0,3,0,0,1,Kind::Choice,"24 dB ladder|12 dB bright|Band pass|High pass",""});
  at(Cutoff, {"CUTOFF","Cutoff",20,20000,2400,1200,.1f,Kind::Float,nullptr,"Hz"});
  at(Resonance, {"RESONANCE","Resonance",0,100,15,0,.01f,Kind::Float,nullptr,"%"});
  at(FilterEnv, {"FILTER_ENV","Filter envelope",-100,100,30,0,.01f,Kind::Float,nullptr,"%"});
  at(Keytrack, {"KEYTRACK","Keytrack",0,100,40,0,.01f,Kind::Float,nullptr,"%"});
  at(FilterVel, {"FILTER_VEL","Filter from velocity",0,100,30,0,.01f,Kind::Float,nullptr,"%"});
  at(FilterDrive, {"FILTER_DRIVE","Filter drive",0,100,10,0,.01f,Kind::Float,nullptr,"%"});
  at(Attack, {"ATTACK","Attack",1,10000,5,400,.1f,Kind::Float,nullptr,"ms"});
  at(Decay, {"DECAY","Decay",1,10000,600,500,.1f,Kind::Float,nullptr,"ms"});
  at(Sustain, {"SUSTAIN","Sustain",0,100,80,0,.01f,Kind::Float,nullptr,"%"});
  at(Release, {"RELEASE","Release",5,15000,400,1000,.1f,Kind::Float,nullptr,"ms"});
  at(FAttack, {"F_ATTACK","Filter attack",1,10000,5,400,.1f,Kind::Float,nullptr,"ms"});
  at(FDecay, {"F_DECAY","Filter decay",1,10000,500,500,.1f,Kind::Float,nullptr,"ms"});
  at(FSustain, {"F_SUSTAIN","Filter sustain",0,100,30,0,.01f,Kind::Float,nullptr,"%"});
  at(FRelease, {"F_RELEASE","Filter release",5,15000,400,1000,.1f,Kind::Float,nullptr,"ms"});
  at(LfoRate, {"LFO_RATE","LFO rate",.02f,20,5,2,.001f,Kind::Float,nullptr,"Hz"});
  at(LfoShape, {"LFO_SHAPE","LFO shape",0,3,0,0,1,Kind::Choice,"Sine|Triangle|Square|Random",""});
  at(LfoSync, {"LFO_SYNC","LFO timing",0,6,0,0,1,Kind::Choice,"Free|1 bar|1/2|1/4|1/8|1/16|1/8T",""});
  at(LfoDelay, {"LFO_DELAY","LFO fade in",0,4000,300,500,.1f,Kind::Float,nullptr,"ms"});
  at(LfoPitch, {"LFO_PITCH","Vibrato",0,100,0,0,.01f,Kind::Float,nullptr,"%"});
  at(LfoFilter, {"LFO_FILTER","LFO to filter",0,100,0,0,.01f,Kind::Float,nullptr,"%"});
  at(LfoAmp, {"LFO_AMP","LFO to level",0,100,0,0,.01f,Kind::Float,nullptr,"%"});
  at(VoiceMode, {"VOICE_MODE","Voice mode",0,2,0,0,1,Kind::Choice,"Poly|Mono|Legato",""});
  at(Unison, {"UNISON","Unison voices",1,8,1,0,1,Kind::Int,nullptr,""});
  at(Detune, {"DETUNE","Unison detune",0,100,25,0,.01f,Kind::Float,nullptr,"%"});
  at(Spread, {"SPREAD","Unison stereo spread",0,100,60,0,.01f,Kind::Float,nullptr,"%"});
  at(Glide, {"GLIDE","Glide",0,2000,0,300,.1f,Kind::Float,nullptr,"ms"});
  at(VelSens, {"VEL_SENS","Velocity",0,100,40,0,.01f,Kind::Float,nullptr,"%"});
  at(BendRange, {"BEND_RANGE","Pitch bend range",1,24,2,0,1,Kind::Int,nullptr,"st"});
  at(Octave, {"OCTAVE","Octave",-2,2,0,0,1,Kind::Int,nullptr,"oct"});
  at(Drift, {"DRIFT","Analog drift",0,100,20,0,.01f,Kind::Float,nullptr,"%"});
  at(Pattern, {"PATTERN","Pattern",0,2,0,0,1,Kind::Choice,"Off|Arpeggiator|Bass sequencer",""});
  at(PatternRate, {"PATTERN_RATE","Pattern rate",0,5,3,0,1,Kind::Choice,rateNames(),""});
  at(ArpMode, {"ARP_MODE","Arpeggio",0,5,0,0,1,Kind::Choice,"Up|Down|Up-down|Random|As played|Chord",""});
  at(ArpOctaves, {"ARP_OCTAVES","Arpeggio octaves",1,4,1,0,1,Kind::Int,nullptr,""});
  at(Gate, {"GATE","Gate length",5,100,60,0,.01f,Kind::Float,nullptr,"%"});
  at(Swing, {"SWING","Swing",0,60,0,0,.01f,Kind::Float,nullptr,"%"});
  at(Latch, {"LATCH","Latch (keep playing after release)",0,1,0,0,1,Kind::Bool,nullptr,""});
  at(SeqLength, {"SEQ_LENGTH","Sequence length",1,seqSteps,16,0,1,Kind::Int,nullptr,""});
  static const char* noteIds[seqSteps] = {"SEQ_NOTE_1","SEQ_NOTE_2","SEQ_NOTE_3","SEQ_NOTE_4","SEQ_NOTE_5","SEQ_NOTE_6","SEQ_NOTE_7","SEQ_NOTE_8",
                                          "SEQ_NOTE_9","SEQ_NOTE_10","SEQ_NOTE_11","SEQ_NOTE_12","SEQ_NOTE_13","SEQ_NOTE_14","SEQ_NOTE_15","SEQ_NOTE_16"};
  static const char* accIds[seqSteps] = {"SEQ_ACCENT_1","SEQ_ACCENT_2","SEQ_ACCENT_3","SEQ_ACCENT_4","SEQ_ACCENT_5","SEQ_ACCENT_6","SEQ_ACCENT_7","SEQ_ACCENT_8",
                                         "SEQ_ACCENT_9","SEQ_ACCENT_10","SEQ_ACCENT_11","SEQ_ACCENT_12","SEQ_ACCENT_13","SEQ_ACCENT_14","SEQ_ACCENT_15","SEQ_ACCENT_16"};
  static const char* slideIds[seqSteps] = {"SEQ_SLIDE_1","SEQ_SLIDE_2","SEQ_SLIDE_3","SEQ_SLIDE_4","SEQ_SLIDE_5","SEQ_SLIDE_6","SEQ_SLIDE_7","SEQ_SLIDE_8",
                                           "SEQ_SLIDE_9","SEQ_SLIDE_10","SEQ_SLIDE_11","SEQ_SLIDE_12","SEQ_SLIDE_13","SEQ_SLIDE_14","SEQ_SLIDE_15","SEQ_SLIDE_16"};
  static const char* onIds[seqSteps] = {"SEQ_ON_1","SEQ_ON_2","SEQ_ON_3","SEQ_ON_4","SEQ_ON_5","SEQ_ON_6","SEQ_ON_7","SEQ_ON_8",
                                        "SEQ_ON_9","SEQ_ON_10","SEQ_ON_11","SEQ_ON_12","SEQ_ON_13","SEQ_ON_14","SEQ_ON_15","SEQ_ON_16"};
  static const char* noteNames[seqSteps] = {"Step 1 note","Step 2 note","Step 3 note","Step 4 note","Step 5 note","Step 6 note","Step 7 note","Step 8 note",
                                            "Step 9 note","Step 10 note","Step 11 note","Step 12 note","Step 13 note","Step 14 note","Step 15 note","Step 16 note"};
  static const char* accNames[seqSteps] = {"Step 1 accent","Step 2 accent","Step 3 accent","Step 4 accent","Step 5 accent","Step 6 accent","Step 7 accent","Step 8 accent",
                                           "Step 9 accent","Step 10 accent","Step 11 accent","Step 12 accent","Step 13 accent","Step 14 accent","Step 15 accent","Step 16 accent"};
  static const char* slideNames[seqSteps] = {"Step 1 slide","Step 2 slide","Step 3 slide","Step 4 slide","Step 5 slide","Step 6 slide","Step 7 slide","Step 8 slide",
                                             "Step 9 slide","Step 10 slide","Step 11 slide","Step 12 slide","Step 13 slide","Step 14 slide","Step 15 slide","Step 16 slide"};
  static const char* onNames[seqSteps] = {"Step 1 on","Step 2 on","Step 3 on","Step 4 on","Step 5 on","Step 6 on","Step 7 on","Step 8 on",
                                          "Step 9 on","Step 10 on","Step 11 on","Step 12 on","Step 13 on","Step 14 on","Step 15 on","Step 16 on"};
  for (int k = 0; k < seqSteps; ++k) {
   at(SeqNote0 + k, {noteIds[k], noteNames[k], -24, 24, 0, 0, 1, Kind::Int, nullptr, "st"});
   at(SeqAccent0 + k, {accIds[k], accNames[k], 0, 1, 0, 0, 1, Kind::Bool, nullptr, ""});
   at(SeqSlide0 + k, {slideIds[k], slideNames[k], 0, 1, 0, 0, 1, Kind::Bool, nullptr, ""});
   at(SeqOn0 + k, {onIds[k], onNames[k], 0, 1, 1, 0, 1, Kind::Bool, nullptr, ""});
  }
  at(ChorusMode, {"CHORUS","Chorus",0,4,0,0,1,Kind::Choice,"Off|I|II|I+II|Dimension",""});
  at(ChorusMix, {"CHORUS_MIX","Chorus mix",0,100,100,0,.01f,Kind::Float,nullptr,"%"});
  at(DelayMix, {"DELAY_MIX","Delay",0,100,0,0,.01f,Kind::Float,nullptr,"%"});
  at(DelaySync, {"DELAY_SYNC","Delay timing",0,6,3,0,1,Kind::Choice,"Free|1/4|1/8|1/8 dotted|1/16|1/4T|1/8T",""});
  at(DelayTime, {"DELAY_TIME","Delay time",20,1500,375,300,.1f,Kind::Float,nullptr,"ms"});
  at(DelayFeedback, {"DELAY_FEEDBACK","Delay repeats",0,95,35,0,.01f,Kind::Float,nullptr,"%"});
  at(DelayPingPong, {"DELAY_PINGPONG","Ping-pong",0,1,1,0,1,Kind::Bool,nullptr,""});
  at(DelayTone, {"DELAY_TONE","Delay tone",0,100,60,0,.01f,Kind::Float,nullptr,"%"});
  at(RevMix, {"REV_MIX","Reverb",0,100,20,0,.01f,Kind::Float,nullptr,"%"});
  at(RevType, {"REV_TYPE","Reverb type",0,2,1,0,1,Kind::Choice,"Plate|Hall|Gated",""});
  at(RevSize, {"REV_SIZE","Reverb size",0,100,70,0,.01f,Kind::Float,nullptr,"%"});
  at(RevDecay, {"REV_DECAY","Reverb decay",.3f,20,3,3,.01f,Kind::Float,nullptr,"s"});
  at(RevDamp, {"REV_DAMP","Reverb darkness",0,100,40,0,.01f,Kind::Float,nullptr,"%"});
  at(Width, {"WIDTH","Width",0,200,100,0,.01f,Kind::Float,nullptr,"%"});
  at(Output, {"OUTPUT","Output",-36,6,-6,0,.01f,Kind::Float,nullptr,"dB"});
  at(PressureCC, {"PRESSURE_CC","Aftertouch from knob (CC)",0,127,0,0,1,Kind::Int,nullptr,"cc"});
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

struct PresetValue { int param; float value; };
struct Preset { const char* name; PresetValue values[48]; };

inline const std::array<Preset, 24>& presets() {
 using namespace P;
 static const std::array<Preset, 24> table{{
  {"Init", {{Output, -3.4f}, {-1, 0}}},
  // pads
  {"Juno Pad", {{Osc1Wave, 0}, {Osc1Level, 70}, {SubLevel, 30}, {PwmDepth, 0}, {Osc2Level, 0}, {Cutoff, 1800}, {Resonance, 10}, {FilterEnv, 20},
                {Attack, 600}, {Decay, 1500}, {Sustain, 85}, {Release, 2200}, {FAttack, 900}, {FDecay, 2000}, {FSustain, 50},
                {ChorusMode, 2}, {RevMix, 30}, {RevType, 1}, {RevDecay, 5}, {Output, -4.7f}, {-1, 0}}},
  {"Night Drive Pad", {{Osc1Wave, 1}, {Osc1PW, 40}, {PwmDepth, 55}, {LfoRate, .6f}, {LfoDelay, 0}, {Osc2Wave, 0}, {Osc2Level, 55}, {Osc2Fine, 9},
                       {Cutoff, 1400}, {Resonance, 20}, {FilterEnv, 15}, {Attack, 900}, {Release, 3000}, {Sustain, 90},
                       {ChorusMode, 3}, {DelayMix, 18}, {DelaySync, 3}, {RevMix, 40}, {RevType, 1}, {RevDecay, 7}, {Output, -14.5f}, {-1, 0}}},
  {"Neon Strings", {{Osc1Wave, 0}, {Osc2Wave, 0}, {Osc2Level, 70}, {Osc2Fine, 12}, {Unison, 3}, {Detune, 30}, {Cutoff, 3200}, {Resonance, 5},
                    {FilterEnv, 10}, {Attack, 450}, {Release, 1800}, {Sustain, 90}, {LfoPitch, 8}, {LfoRate, 5.5f}, {LfoDelay, 800},
                    {ChorusMode, 1}, {RevMix, 35}, {RevType, 1}, {RevDecay, 4}, {Output, -6.6f}, {-1, 0}}},
  {"Glass Choir", {{Osc1Wave, 3}, {FmLevel, 60}, {FmRatio, 3}, {FmIndex, 25}, {FmDecay, 4000}, {Osc1Level, 40}, {Cutoff, 6000}, {FilterType, 1},
                   {Attack, 1200}, {Release, 3500}, {Sustain, 100}, {ChorusMode, 4}, {RevMix, 50}, {RevType, 1}, {RevDecay, 9}, {Output, -11.8f}, {-1, 0}}},
  // brass and stabs
  {"OB Brass", {{FilterType, 1}, {Osc1Wave, 0}, {Osc2Wave, 0}, {Osc2Level, 80}, {Osc2Fine, 8}, {Unison, 2}, {Detune, 20}, {Cutoff, 700},
                {Resonance, 15}, {FilterEnv, 65}, {FAttack, 60}, {FDecay, 600}, {FSustain, 45}, {Attack, 25}, {Release, 350},
                {ChorusMode, 1}, {RevMix, 25}, {RevType, 0}, {Output, -8.1f}, {-1, 0}}},
  {"Prophet Brass", {{Osc1Wave, 0}, {Osc2Wave, 1}, {Osc2Level, 70}, {Osc2Fine, -6}, {Cutoff, 600}, {Resonance, 25}, {FilterEnv, 70},
                     {FAttack, 80}, {FDecay, 450}, {FSustain, 40}, {Attack, 30}, {Release, 300}, {RevMix, 22}, {RevType, 0}, {Output, -7.0f}, {-1, 0}}},
  {"Stab 84", {{FilterType, 1}, {Osc1Wave, 0}, {Osc2Level, 80}, {Osc2Fine, 10}, {Unison, 4}, {Detune, 35}, {Cutoff, 900}, {FilterEnv, 70},
               {FAttack, 1}, {FDecay, 250}, {FSustain, 10}, {Attack, 1}, {Decay, 400}, {Sustain, 0}, {Release, 250},
               {DelayMix, 25}, {DelaySync, 3}, {RevMix, 30}, {RevType, 2}, {Output, 2.1f}, {-1, 0}}},
  // leads
  {"Sync Lead", {{VoiceMode, 2}, {Glide, 60}, {Osc1Wave, 0}, {Osc2Wave, 0}, {Osc2Sync, 1}, {Osc2Octave, 1}, {Osc2Semi, 5}, {Osc2Level, 80},
                 {Osc1Level, 30}, {Cutoff, 4500}, {Resonance, 10}, {FilterEnv, 25}, {LfoPitch, 12}, {LfoDelay, 500},
                 {DelayMix, 30}, {DelaySync, 3}, {RevMix, 25}, {Output, 3.0f}, {-1, 0}}},
  {"Hero Lead", {{VoiceMode, 2}, {Glide, 40}, {Unison, 5}, {Detune, 30}, {Osc1Wave, 0}, {Osc2Wave, 0}, {Osc2Level, 70}, {Osc2Octave, -1},
                 {Cutoff, 3500}, {FilterEnv, 30}, {LfoPitch, 15}, {LfoDelay, 400}, {ChorusMode, 1}, {DelayMix, 30}, {DelaySync, 3},
                 {RevMix, 30}, {RevType, 1}, {Output, 2.6f}, {-1, 0}}},
  {"Square Lead", {{VoiceMode, 1}, {Glide, 30}, {Osc1Wave, 1}, {Osc1PW, 50}, {PwmDepth, 20}, {LfoRate, 4}, {Cutoff, 2800}, {Resonance, 20},
                   {FilterEnv, 20}, {LfoPitch, 10}, {LfoDelay, 600}, {DelayMix, 25}, {DelaySync, 4}, {RevMix, 20}, {Output, -0.4f}, {-1, 0}}},
  // keys and bells
  {"FM Electric Piano", {{Osc1Level, 0}, {FmLevel, 90}, {FmRatio, 1}, {FmIndex, 45}, {FmDecay, 900}, {FmVelocity, 80}, {FilterType, 1},
                         {Cutoff, 9000}, {FilterEnv, 0}, {Attack, 2}, {Decay, 2600}, {Sustain, 25}, {Release, 600},
                         {ChorusMode, 1}, {ChorusMix, 60}, {RevMix, 25}, {RevType, 0}, {Output, -7.7f}, {-1, 0}}},
  {"Crystal Bells", {{Osc1Level, 0}, {FmLevel, 90}, {FmRatio, 8}, {FmIndex, 55}, {FmDecay, 1500}, {FilterType, 1}, {Cutoff, 12000},
                     {Attack, 1}, {Decay, 3500}, {Sustain, 0}, {Release, 2500}, {DelayMix, 25}, {DelaySync, 3},
                     {RevMix, 45}, {RevType, 1}, {RevDecay, 6}, {Output, -7.5f}, {-1, 0}}},
  {"Poly Pluck", {{Osc1Wave, 0}, {Osc2Wave, 1}, {Osc2Level, 50}, {Osc2Fine, 6}, {Cutoff, 500}, {Resonance, 25}, {FilterEnv, 75},
                  {FAttack, 1}, {FDecay, 260}, {FSustain, 0}, {Attack, 1}, {Decay, 600}, {Sustain, 0}, {Release, 300},
                  {ChorusMode, 1}, {DelayMix, 30}, {DelaySync, 3}, {RevMix, 25}, {Output, 4.5f}, {-1, 0}}},
  // basses
  {"Moroder Bass", {{Pattern, 2}, {PatternRate, 3}, {VoiceMode, 1}, {Osc1Wave, 0}, {SubLevel, 40}, {Cutoff, 400}, {Resonance, 35},
                    {FilterEnv, 55}, {FAttack, 1}, {FDecay, 180}, {FSustain, 10}, {Attack, 1}, {Decay, 250}, {Sustain, 60}, {Release, 60},
                    {Gate, 55}, {SeqNote0 + 2, 12}, {SeqNote0 + 6, 12}, {SeqNote0 + 10, 12}, {SeqNote0 + 14, 12},
                    {SeqAccent0 + 0, 1}, {SeqAccent0 + 8, 1}, {RevMix, 8}, {Output, 3.2f}, {-1, 0}}},
  {"Octave Bass", {{Pattern, 2}, {PatternRate, 3}, {VoiceMode, 1}, {Osc1Wave, 1}, {Osc1PW, 40}, {Osc2Level, 50}, {Osc2Octave, -1},
                   {Cutoff, 550}, {Resonance, 25}, {FilterEnv, 45}, {FDecay, 220}, {FSustain, 15}, {Attack, 1}, {Decay, 300}, {Sustain, 50}, {Release, 50},
                   {Gate, 50}, {SeqNote0 + 1, 12}, {SeqNote0 + 3, 12}, {SeqNote0 + 5, 12}, {SeqNote0 + 7, 12}, {SeqNote0 + 9, 12},
                   {SeqNote0 + 11, 12}, {SeqNote0 + 13, 12}, {SeqNote0 + 15, 12}, {RevMix, 6}, {Output, 1.5f}, {-1, 0}}},
  {"Slap Bass FM", {{VoiceMode, 1}, {Osc1Level, 30}, {Osc1Wave, 0}, {FmLevel, 80}, {FmRatio, 1}, {FmIndex, 50}, {FmDecay, 200}, {FmVelocity, 90},
                    {Cutoff, 2500}, {FilterEnv, 30}, {FDecay, 300}, {FSustain, 20}, {Attack, 1}, {Decay, 700}, {Sustain, 30}, {Release, 120},
                    {RevMix, 6}, {Output, 6.f}, {-1, 0}}},
  {"Reese Night", {{VoiceMode, 1}, {Unison, 4}, {Detune, 40}, {Spread, 30}, {Osc1Wave, 0}, {Osc2Level, 70}, {Osc2Fine, -12},
                   {Cutoff, 900}, {Resonance, 15}, {FilterEnv, 10}, {LfoFilter, 15}, {LfoRate, .3f}, {Attack, 5}, {Release, 200},
                   {ChorusMode, 0}, {RevMix, 8}, {Output, 1.2f}, {-1, 0}}},
  // arps and sequences
  {"Outrun Arp", {{Pattern, 1}, {PatternRate, 3}, {ArpMode, 0}, {ArpOctaves, 2}, {Gate, 45}, {Osc1Wave, 0}, {Osc2Level, 50}, {Osc2Fine, 7},
                  {Cutoff, 1200}, {Resonance, 30}, {FilterEnv, 50}, {FDecay, 250}, {FSustain, 10}, {Attack, 1}, {Decay, 300}, {Sustain, 40},
                  {Release, 150}, {ChorusMode, 1}, {DelayMix, 30}, {DelaySync, 3}, {RevMix, 25}, {Output, 6.f}, {-1, 0}}},
  {"Up-Down Glass", {{Pattern, 1}, {PatternRate, 4}, {ArpMode, 2}, {ArpOctaves, 3}, {Gate, 40}, {Osc1Level, 20}, {FmLevel, 80}, {FmRatio, 3},
                     {FmIndex, 35}, {FmDecay, 300}, {FilterType, 1}, {Cutoff, 8000}, {Attack, 1}, {Decay, 400}, {Sustain, 0}, {Release, 300},
                     {DelayMix, 35}, {DelaySync, 6}, {RevMix, 35}, {RevType, 1}, {Output, -1.2f}, {-1, 0}}},
  {"Chord Pulse", {{Pattern, 1}, {PatternRate, 3}, {ArpMode, 5}, {Gate, 30}, {Osc1Wave, 1}, {Osc1PW, 30}, {Osc2Level, 60}, {Osc2Fine, 8},
                   {Cutoff, 1500}, {Resonance, 20}, {FilterEnv, 40}, {FDecay, 200}, {FSustain, 10}, {Attack, 1}, {Decay, 250}, {Sustain, 30},
                   {Release, 120}, {ChorusMode, 2}, {DelayMix, 20}, {DelaySync, 2}, {RevMix, 25}, {Output, -3.f}, {-1, 0}}},
  // sweeps and effects
  {"Sunset Sweep", {{Osc1Wave, 0}, {Osc2Level, 70}, {Osc2Fine, 14}, {Unison, 3}, {Detune, 30}, {Cutoff, 300}, {Resonance, 55},
                    {FilterEnv, 60}, {FAttack, 4000}, {FDecay, 4000}, {FSustain, 60}, {Attack, 800}, {Release, 3000}, {LfoFilter, 10}, {LfoRate, .2f},
                    {ChorusMode, 3}, {RevMix, 45}, {RevType, 1}, {RevDecay, 8}, {Output, -4.2f}, {-1, 0}}},
  {"Gated Hit", {{Osc1Wave, 0}, {Osc2Level, 70}, {Osc2Fine, 10}, {NoiseLevel, 25}, {Unison, 3}, {Cutoff, 2500}, {FilterEnv, 30}, {FDecay, 200},
                 {Attack, 1}, {Decay, 300}, {Sustain, 0}, {Release, 300}, {RevMix, 55}, {RevType, 2}, {RevSize, 90}, {Output, 6.f}, {-1, 0}}},
  {"VHS Memory", {{Osc1Wave, 2}, {Osc2Wave, 3}, {Osc2Level, 50}, {Osc2Octave, 1}, {Drift, 70}, {LfoPitch, 6}, {LfoRate, .4f}, {LfoDelay, 0},
                  {Cutoff, 2200}, {FilterType, 1}, {Attack, 300}, {Release, 2000}, {ChorusMode, 2}, {DelayMix, 25}, {DelaySync, 3},
                  {RevMix, 40}, {RevType, 1}, {Output, -7.7f}, {-1, 0}}},
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

} // namespace inverso_abismo::neon
