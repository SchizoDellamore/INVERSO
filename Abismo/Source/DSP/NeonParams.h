#pragma once
// NEON: a light polysynth for synthwave, with a little of each machine the style
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
 // arpeggiator
 Arp, ArpRate, ArpMode, ArpOctaves, Gate, Swing, Latch,
 // effects
 ChorusMode, ChorusMix,
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
  at(Unison, {"UNISON","Unison voices",1,8,1,0,1,Kind::Int,nullptr,"n"});
  at(Detune, {"DETUNE","Unison detune",0,100,25,0,.01f,Kind::Float,nullptr,"%"});
  at(Spread, {"SPREAD","Unison stereo spread",0,100,60,0,.01f,Kind::Float,nullptr,"%"});
  at(Glide, {"GLIDE","Glide",0,2000,0,300,.1f,Kind::Float,nullptr,"ms"});
  at(VelSens, {"VEL_SENS","Velocity",0,100,40,0,.01f,Kind::Float,nullptr,"%"});
  at(BendRange, {"BEND_RANGE","Pitch bend range",1,24,2,0,1,Kind::Int,nullptr,"st"});
  at(Octave, {"OCTAVE","Octave",-2,2,0,0,1,Kind::Int,nullptr,"oct"});
  at(Drift, {"DRIFT","Analog drift",0,100,20,0,.01f,Kind::Float,nullptr,"%"});
  at(Arp, {"ARP","Arpeggiator",0,1,0,0,1,Kind::Bool,nullptr,""});
  at(ArpRate, {"ARP_RATE","Arpeggio rate",0,5,3,0,1,Kind::Choice,rateNames(),""});
  at(ArpMode, {"ARP_MODE","Arpeggio",0,5,0,0,1,Kind::Choice,"Up|Down|Up-down|Random|As played|Chord",""});
  at(ArpOctaves, {"ARP_OCTAVES","Arpeggio octaves",1,4,1,0,1,Kind::Int,nullptr,"n"});
  at(Gate, {"GATE","Gate length",5,100,60,0,.01f,Kind::Float,nullptr,"%"});
  at(Swing, {"SWING","Swing",0,60,0,0,.01f,Kind::Float,nullptr,"%"});
  at(Latch, {"LATCH","Latch (keep playing after release)",0,1,0,0,1,Kind::Bool,nullptr,""});
  at(ChorusMode, {"CHORUS","Chorus",0,4,0,0,1,Kind::Choice,"Off|I|II|I+II|Dimension",""});
  at(ChorusMix, {"CHORUS_MIX","Chorus mix",0,100,100,0,.01f,Kind::Float,nullptr,"%"});
  at(DelayMix, {"DELAY_MIX","Delay",0,100,0,0,.01f,Kind::Float,nullptr,"%"});
  at(DelaySync, {"DELAY_SYNC","Delay timing",0,6,3,0,1,Kind::Choice,"Free|1/4|1/8|1/8 dotted|1/16|1/4T|1/8T",""});
  at(DelayTime, {"DELAY_TIME","Delay time",20,1500,375,300,.1f,Kind::Float,nullptr,"ms"});
  at(DelayFeedback, {"DELAY_FEEDBACK","Delay repeats",0,95,35,0,.01f,Kind::Float,nullptr,"%"});
  at(DelayPingPong, {"DELAY_PINGPONG","Ping-pong",0,1,1,0,1,Kind::Bool,nullptr,""});
  at(DelayTone, {"DELAY_TONE","Delay tone",0,100,60,0,.01f,Kind::Float,nullptr,"%"});
  at(RevMix, {"REV_MIX","Reverb",0,100,20,0,.01f,Kind::Float,nullptr,"%"});
  at(RevType, {"REV_TYPE","Reverb type",0,1,1,0,1,Kind::Choice,"Plate|Hall",""});
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
struct Preset { const char* name; PresetValue values[40]; };

// Factory sounds, "Category/Name": the preset menu groups them. Each is meant to
// drop straight into a track. OUTPUT is set so every preset sits near the same
// loudness (the loudest 400 ms of a played part around -14 dBFS).
inline const std::array<Preset, 131>& presets() {
 using namespace P;
 static const std::array<Preset, 131> table{{
  // ---- BASS
  {"Bass/Moroder Pluck", {{Drift, 0}, {VoiceMode, 1}, {Osc1Wave, 0}, {SubLevel, 45}, {Cutoff, 380}, {Resonance, 35}, {FilterEnv, 55}, {FAttack, 1}, {FDecay, 170},
                          {FSustain, 8}, {Attack, 1}, {Decay, 260}, {Sustain, 55}, {Release, 60}, {Keytrack, 30}, {RevMix, 0}, {Output, 0.9f}, {-1, 0}}},
  {"Bass/Octave Runner", {{Drift, 0}, {Arp, 1}, {ArpRate, 3}, {ArpMode, 0}, {ArpOctaves, 2}, {Gate, 50}, {VoiceMode, 1}, {Osc1Wave, 1}, {Osc1PW, 42},
                          {Osc2Level, 45}, {Osc2Octave, -1}, {Cutoff, 520}, {Resonance, 28}, {FilterEnv, 45}, {FDecay, 200}, {FSustain, 12},
                          {Attack, 1}, {Decay, 300}, {Sustain, 50}, {Release, 50}, {RevMix, 0}, {Output, -2.0f}, {-1, 0}}},
  {"Bass/Outrun Sub", {{Drift, 0}, {VoiceMode, 2}, {Glide, 25}, {Osc1Wave, 2}, {SubLevel, 20}, {Osc1Level, 90}, {Osc2Wave, 3}, {Osc2Level, 35}, {Osc2Fine, 0}, {Cutoff, 900}, {FilterEnv, 15}, {FDecay, 300},
                       {Attack, 2}, {Release, 120}, {RevMix, 0}, {Output, -1.8f}, {-1, 0}}},
  {"Bass/Juno Bass", {{Drift, 0}, {VoiceMode, 1}, {Osc1Wave, 1}, {Osc1PW, 35}, {SubLevel, 55}, {Cutoff, 650}, {Resonance, 20}, {FilterEnv, 35}, {FDecay, 350},
                      {FSustain, 20}, {Attack, 1}, {Decay, 500}, {Sustain, 70}, {Release, 90}, {ChorusMode, 1}, {ChorusMix, 40}, {RevMix, 0}, {Output, -2.6f}, {-1, 0}}},
  {"Bass/FM Slap", {{Drift, 0}, {VoiceMode, 1}, {Osc1Level, 25}, {FmLevel, 85}, {FmRatio, 1}, {FmIndex, 50}, {FmDecay, 180}, {FmVelocity, 90}, {Cutoff, 2600},
                    {FilterEnv, 25}, {FDecay, 300}, {FSustain, 20}, {Attack, 1}, {Decay, 700}, {Sustain, 30}, {Release, 110}, {RevMix, 0}, {Output, 0.9f}, {-1, 0}}},
  {"Bass/Reese Night", {{Drift, 0}, {VoiceMode, 1}, {Unison, 3}, {Detune, 45}, {Spread, 25}, {Osc1Wave, 0}, {Osc2Level, 70}, {Osc2Fine, -12}, {Cutoff, 850},
                        {Resonance, 15}, {FilterEnv, 10}, {LfoFilter, 12}, {LfoRate, .3f}, {LfoDelay, 0}, {Attack, 5}, {Release, 180}, {RevMix, 0}, {Output, -2.9f}, {-1, 0}}},
  // ---- POLYSYNTH
  {"Polysynth/Juno Poly", {{Osc1Wave, 0}, {SubLevel, 25}, {Cutoff, 2200}, {Resonance, 12}, {FilterEnv, 25}, {FDecay, 700}, {FSustain, 40},
                           {Attack, 8}, {Decay, 800}, {Sustain, 75}, {Release, 500}, {ChorusMode, 1}, {RevMix, 18}, {RevType, 1}, {Output, -4.8f}, {-1, 0}}},
  {"Polysynth/Prophet Poly", {{Osc1Wave, 0}, {Osc2Wave, 1}, {Osc2Level, 60}, {Osc2Fine, -7}, {Cutoff, 1600}, {Resonance, 20}, {FilterEnv, 40},
                              {FDecay, 600}, {FSustain, 35}, {Attack, 6}, {Decay, 900}, {Sustain, 70}, {Release, 450}, {RevMix, 15}, {RevType, 0}, {Output, -8.4f}, {-1, 0}}},
  {"Polysynth/OB Stab", {{FilterType, 1}, {Osc1Wave, 0}, {Osc2Level, 80}, {Osc2Fine, 10}, {Unison, 2}, {Detune, 25}, {Cutoff, 900}, {FilterEnv, 70},
                         {FAttack, 1}, {FDecay, 260}, {FSustain, 10}, {Attack, 1}, {Decay, 420}, {Sustain, 0}, {Release, 260},
                         {DelayMix, 18}, {DelaySync, 3}, {RevMix, 15}, {Output, 0.3f}, {-1, 0}}},
  {"Polysynth/Brass 84", {{FilterType, 1}, {Osc1Wave, 0}, {Osc2Wave, 0}, {Osc2Level, 80}, {Osc2Fine, 8}, {Unison, 2}, {Detune, 20}, {Cutoff, 700},
                          {Resonance, 15}, {FilterEnv, 65}, {FAttack, 60}, {FDecay, 600}, {FSustain, 45}, {Attack, 25}, {Release, 350},
                          {ChorusMode, 1}, {RevMix, 18}, {RevType, 0}, {Output, -8.4f}, {-1, 0}}},
  {"Polysynth/Wide Saws", {{Osc1Wave, 0}, {Osc2Wave, 0}, {Osc2Level, 60}, {Osc2Fine, 12}, {Unison, 3}, {Detune, 35}, {Spread, 80}, {Cutoff, 3200},
                           {FilterEnv, 15}, {Attack, 15}, {Release, 600}, {ChorusMode, 1}, {RevMix, 20}, {RevType, 1}, {Output, -5.7f}, {-1, 0}}},
  {"Polysynth/Chord Memory", {{Osc1Wave, 1}, {Osc1PW, 30}, {Osc2Level, 55}, {Osc2Fine, 8}, {Cutoff, 1300}, {Resonance, 25}, {FilterEnv, 45},
                              {FDecay, 220}, {FSustain, 12}, {Attack, 1}, {Decay, 380}, {Sustain, 25}, {Release, 200},
                              {ChorusMode, 2}, {DelayMix, 20}, {DelaySync, 2}, {RevMix, 15}, {Output, -7.2f}, {-1, 0}}},
  // ---- LEAD
  {"Lead/Sync Hero", {{VoiceMode, 2}, {Glide, 60}, {Osc1Wave, 0}, {Osc2Wave, 0}, {Osc2Sync, 1}, {Osc2Octave, 1}, {Osc2Semi, 5}, {Osc2Level, 80},
                      {Osc1Level, 30}, {Cutoff, 4500}, {Resonance, 10}, {FilterEnv, 25}, {LfoPitch, 12}, {LfoDelay, 500},
                      {DelayMix, 28}, {DelaySync, 3}, {RevMix, 18}, {Output, 1.0f}, {-1, 0}}},
  {"Lead/Saw Anthem", {{VoiceMode, 2}, {Glide, 40}, {Unison, 4}, {Detune, 30}, {Osc1Wave, 0}, {Osc2Wave, 0}, {Osc2Level, 70}, {Osc2Octave, -1},
                       {Cutoff, 3500}, {FilterEnv, 30}, {LfoPitch, 15}, {LfoDelay, 400}, {ChorusMode, 1}, {DelayMix, 28}, {DelaySync, 3},
                       {RevMix, 22}, {RevType, 1}, {Output, 0.0f}, {-1, 0}}},
  {"Lead/Square Night", {{VoiceMode, 1}, {Glide, 30}, {Osc1Wave, 1}, {Osc1PW, 50}, {PwmDepth, 20}, {LfoRate, 4}, {Cutoff, 2800}, {Resonance, 20},
                         {FilterEnv, 20}, {LfoPitch, 10}, {LfoDelay, 600}, {DelayMix, 25}, {DelaySync, 4}, {RevMix, 15}, {Output, -3.5f}, {-1, 0}}},
  {"Lead/Glide Whistle", {{VoiceMode, 2}, {Glide, 140}, {Osc1Wave, 3}, {Osc2Wave, 2}, {Osc2Level, 35}, {Osc2Octave, 1}, {FilterType, 1}, {Cutoff, 6000},
                          {LfoPitch, 14}, {LfoRate, 5.5f}, {LfoDelay, 350}, {Attack, 20}, {Release, 250}, {DelayMix, 30}, {DelaySync, 3},
                          {RevMix, 25}, {RevType, 1}, {Output, -0.5f}, {-1, 0}}},
  {"Lead/FM Solo", {{VoiceMode, 2}, {Glide, 35}, {Osc1Level, 20}, {FmLevel, 80}, {FmRatio, 2}, {FmIndex, 45}, {FmDecay, 2500}, {FilterType, 1},
                    {Cutoff, 7000}, {LfoPitch, 10}, {LfoDelay, 500}, {Attack, 5}, {Release, 200}, {DelayMix, 25}, {DelaySync, 3}, {RevMix, 18}, {Output, -3.9f}, {-1, 0}}},
  {"Lead/Resonant Bite", {{VoiceMode, 1}, {Osc1Wave, 0}, {Osc2Level, 50}, {Osc2Fine, 9}, {Cutoff, 900}, {Resonance, 60}, {FilterEnv, 55},
                          {FDecay, 400}, {FSustain, 30}, {FilterDrive, 35}, {Attack, 2}, {Release, 150}, {DelayMix, 20}, {DelaySync, 3}, {RevMix, 12}, {Output, -1.6f}, {-1, 0}}},
  {"Lead/Mono Saw", {{VoiceMode, 1}, {Glide, 20}, {Osc1Wave, 0}, {Osc2Wave, 0}, {Osc2Level, 60}, {Osc2Fine, 6}, {Cutoff, 2400}, {Resonance, 18},
                     {FilterEnv, 35}, {FDecay, 450}, {FSustain, 40}, {Attack, 3}, {Release, 160}, {LfoPitch, 10}, {LfoDelay, 450},
                     {DelayMix, 22}, {DelaySync, 3}, {RevMix, 15}, {Output, -2.0f}, {-1, 0}}},
  // ---- PAD
  {"Pad/Juno Pad", {{Osc1Wave, 0}, {Osc1Level, 70}, {SubLevel, 30}, {Cutoff, 1800}, {Resonance, 10}, {FilterEnv, 20}, {Attack, 600}, {Decay, 1500},
                    {Sustain, 85}, {Release, 2200}, {FAttack, 900}, {FDecay, 2000}, {FSustain, 50}, {ChorusMode, 2}, {RevMix, 30}, {RevType, 1},
                    {RevDecay, 5}, {Output, -5.0f}, {-1, 0}}},
  {"Pad/Night Drive", {{Osc1Wave, 1}, {Osc1PW, 40}, {PwmDepth, 55}, {LfoRate, .6f}, {LfoDelay, 0}, {Osc2Wave, 0}, {Osc2Level, 55}, {Osc2Fine, 9},
                       {Cutoff, 1400}, {Resonance, 20}, {FilterEnv, 15}, {Attack, 900}, {Release, 3000}, {Sustain, 90}, {ChorusMode, 3},
                       {DelayMix, 15}, {DelaySync, 3}, {RevMix, 35}, {RevType, 1}, {RevDecay, 7}, {Output, -15.8f}, {-1, 0}}},
  {"Pad/Neon Strings", {{Osc1Wave, 0}, {Osc2Wave, 0}, {Osc2Level, 70}, {Osc2Fine, 12}, {Unison, 2}, {Detune, 30}, {Cutoff, 3200}, {Resonance, 5},
                        {FilterEnv, 10}, {Attack, 450}, {Release, 1800}, {Sustain, 90}, {LfoPitch, 8}, {LfoRate, 5.5f}, {LfoDelay, 800},
                        {ChorusMode, 1}, {RevMix, 30}, {RevType, 1}, {RevDecay, 4}, {Output, -7.4f}, {-1, 0}}},
  {"Pad/Glass Choir", {{Osc1Wave, 3}, {Osc1Level, 40}, {FmLevel, 60}, {FmRatio, 3}, {FmIndex, 25}, {FmDecay, 4000}, {FilterType, 1}, {Cutoff, 6000},
                       {Attack, 1200}, {Release, 3500}, {Sustain, 100}, {ChorusMode, 4}, {RevMix, 45}, {RevType, 1}, {RevDecay, 9}, {Output, -4.3f}, {-1, 0}}},
  {"Pad/Sunset Sweep", {{Osc1Wave, 0}, {Osc2Level, 70}, {Osc2Fine, 14}, {Unison, 2}, {Detune, 30}, {Cutoff, 300}, {Resonance, 50}, {FilterEnv, 60},
                        {FAttack, 4000}, {FDecay, 4000}, {FSustain, 60}, {Attack, 800}, {Release, 3000}, {LfoFilter, 10}, {LfoRate, .2f},
                        {ChorusMode, 3}, {RevMix, 40}, {RevType, 1}, {RevDecay, 8}, {Output, -8.8f}, {-1, 0}}},
  {"Pad/Warm Analog", {{Osc1Wave, 0}, {Osc2Wave, 2}, {Osc2Level, 50}, {Osc2Octave, -1}, {Cutoff, 1100}, {Resonance, 8}, {FilterEnv, 10}, {Drift, 50},
                       {Attack, 700}, {Release, 2500}, {Sustain, 95}, {ChorusMode, 1}, {RevMix, 30}, {RevType, 1}, {RevDecay, 5}, {Output, -6.3f}, {-1, 0}}},
  // ---- KEYS
  {"Keys/FM Electric Piano", {{Osc1Level, 0}, {FmLevel, 90}, {FmRatio, 1}, {FmIndex, 45}, {FmDecay, 900}, {FmVelocity, 80}, {FilterType, 1},
                              {Cutoff, 9000}, {FilterEnv, 0}, {Attack, 2}, {Decay, 2600}, {Sustain, 25}, {Release, 600}, {ChorusMode, 1}, {ChorusMix, 60},
                              {RevMix, 20}, {RevType, 0}, {Output, -8.2f}, {-1, 0}}},
  {"Keys/Crystal Bells", {{Osc1Level, 0}, {FmLevel, 90}, {FmRatio, 8}, {FmIndex, 55}, {FmDecay, 1500}, {FilterType, 1}, {Cutoff, 12000},
                          {Attack, 1}, {Decay, 3500}, {Sustain, 0}, {Release, 2500}, {DelayMix, 22}, {DelaySync, 3}, {RevMix, 35}, {RevType, 1},
                          {RevDecay, 6}, {Output, -8.3f}, {-1, 0}}},
  {"Keys/DX Marimba", {{Osc1Level, 0}, {FmLevel, 90}, {FmRatio, 4}, {FmIndex, 35}, {FmDecay, 120}, {FmVelocity, 70}, {FilterType, 1}, {Cutoff, 9000},
                       {Attack, 1}, {Decay, 700}, {Sustain, 0}, {Release, 300}, {RevMix, 18}, {RevType, 0}, {Output, -4.0f}, {-1, 0}}},
  {"Keys/Poly Organ", {{Osc1Wave, 3}, {Osc2Wave, 3}, {Osc2Level, 60}, {Osc2Octave, 1}, {SubLevel, 40}, {FilterType, 1}, {Cutoff, 5000}, {Attack, 3},
                      {Release, 120}, {ChorusMode, 3}, {RevMix, 18}, {RevType, 1}, {Output, -11.0f}, {-1, 0}}},
  // ---- PLUCK
  {"Pluck/Poly Pluck", {{Osc1Wave, 0}, {Osc2Wave, 1}, {Osc2Level, 50}, {Osc2Fine, 6}, {Cutoff, 500}, {Resonance, 25}, {FilterEnv, 75}, {FAttack, 1},
                        {FDecay, 260}, {FSustain, 0}, {Attack, 1}, {Decay, 600}, {Sustain, 0}, {Release, 300}, {ChorusMode, 1}, {DelayMix, 28},
                        {DelaySync, 3}, {RevMix, 18}, {Output, 3.6f}, {-1, 0}}},
  {"Pluck/Glass Pluck", {{Osc1Level, 30}, {Osc1Wave, 2}, {FmLevel, 70}, {FmRatio, 3}, {FmIndex, 40}, {FmDecay, 250}, {FilterType, 1}, {Cutoff, 8000},
                         {Attack, 1}, {Decay, 500}, {Sustain, 0}, {Release, 250}, {DelayMix, 30}, {DelaySync, 6}, {RevMix, 22}, {Output, -1.4f}, {-1, 0}}},
  {"Pluck/Muted Pulse", {{Osc1Wave, 1}, {Osc1PW, 25}, {Cutoff, 700}, {Resonance, 15}, {FilterEnv, 50}, {FDecay, 140}, {FSustain, 0}, {Attack, 1},
                         {Decay, 220}, {Sustain, 0}, {Release, 120}, {ChorusMode, 1}, {DelayMix, 20}, {DelaySync, 4}, {RevMix, 12}, {Output, -3.0f}, {-1, 0}}},
  // ---- ARP
  {"Arp/Outrun Arp", {{Arp, 1}, {ArpRate, 3}, {ArpMode, 0}, {ArpOctaves, 2}, {Gate, 45}, {Osc1Wave, 0}, {Osc2Level, 50}, {Osc2Fine, 7}, {Cutoff, 1200},
                      {Resonance, 30}, {FilterEnv, 50}, {FDecay, 250}, {FSustain, 10}, {Attack, 1}, {Decay, 300}, {Sustain, 40}, {Release, 150},
                      {ChorusMode, 1}, {DelayMix, 28}, {DelaySync, 3}, {RevMix, 18}, {Output, 0.6f}, {-1, 0}}},
  {"Arp/Up-Down Glass", {{Arp, 1}, {ArpRate, 4}, {ArpMode, 2}, {ArpOctaves, 3}, {Gate, 40}, {Osc1Level, 20}, {FmLevel, 80}, {FmRatio, 3}, {FmIndex, 35},
                         {FmDecay, 300}, {FilterType, 1}, {Cutoff, 8000}, {Attack, 1}, {Decay, 400}, {Sustain, 0}, {Release, 300}, {DelayMix, 30},
                         {DelaySync, 6}, {RevMix, 30}, {RevType, 1}, {Output, -1.5f}, {-1, 0}}},
  {"Arp/Chord Pulse", {{Arp, 1}, {ArpRate, 3}, {ArpMode, 5}, {Gate, 30}, {Osc1Wave, 1}, {Osc1PW, 30}, {Osc2Level, 60}, {Osc2Fine, 8}, {Cutoff, 1500},
                       {Resonance, 20}, {FilterEnv, 40}, {FDecay, 200}, {FSustain, 10}, {Attack, 1}, {Decay, 250}, {Sustain, 30}, {Release, 120},
                       {ChorusMode, 2}, {DelayMix, 18}, {DelaySync, 2}, {RevMix, 18}, {Output, -8.2f}, {-1, 0}}},
  {"Arp/Triplet Rain", {{Arp, 1}, {ArpRate, 4}, {ArpMode, 3}, {ArpOctaves, 2}, {Gate, 35}, {Osc1Wave, 2}, {Osc2Wave, 3}, {Osc2Level, 50}, {Osc2Octave, 1},
                        {FilterType, 1}, {Cutoff, 4000}, {Attack, 1}, {Decay, 300}, {Sustain, 0}, {Release, 200}, {DelayMix, 30}, {DelaySync, 6},
                        {RevMix, 30}, {RevType, 1}, {Output, 1.2f}, {-1, 0}}},
  {"Arp/Sequence 1984", {{Arp, 1}, {ArpRate, 3}, {ArpMode, 4}, {ArpOctaves, 1}, {Gate, 55}, {VoiceMode, 1}, {Osc1Wave, 0}, {SubLevel, 30},
                         {Cutoff, 600}, {Resonance, 40}, {FilterEnv, 50}, {FDecay, 180}, {FSustain, 10}, {Attack, 1}, {Decay, 250}, {Sustain, 50},
                         {Release, 60}, {DelayMix, 15}, {DelaySync, 2}, {RevMix, 10}, {Output, 0.5f}, {-1, 0}}},
  // ---- FX
  {"FX/VHS Memory", {{Osc1Wave, 2}, {Osc2Wave, 3}, {Osc2Level, 50}, {Osc2Octave, 1}, {Drift, 70}, {LfoPitch, 6}, {LfoRate, .4f}, {LfoDelay, 0},
                     {Cutoff, 2200}, {FilterType, 1}, {Attack, 300}, {Release, 2000}, {ChorusMode, 2}, {DelayMix, 25}, {DelaySync, 3},
                     {RevMix, 35}, {RevType, 1}, {Output, -8.0f}, {-1, 0}}},
  {"FX/Night Wind", {{Osc1Level, 0}, {NoiseLevel, 80}, {FilterType, 2}, {Cutoff, 900}, {Resonance, 70}, {LfoFilter, 40}, {LfoRate, .15f}, {LfoDelay, 0},
                     {Attack, 1500}, {Release, 3000}, {ChorusMode, 4}, {RevMix, 45}, {RevType, 1}, {RevDecay, 8}, {Output, 6.0f}, {-1, 0}}},
  {"FX/Rise", {{Osc1Wave, 0}, {Osc2Level, 60}, {Osc2Fine, 15}, {NoiseLevel, 20}, {Unison, 2}, {Cutoff, 200}, {Resonance, 45}, {FilterEnv, 80},
               {FAttack, 6000}, {FDecay, 2000}, {FSustain, 100}, {Attack, 3000}, {Release, 1500}, {LfoPitch, 0}, {ChorusMode, 1}, {DelayMix, 20},
               {DelaySync, 3}, {RevMix, 35}, {RevType, 1}, {Output, -3.7f}, {-1, 0}}},
  // ---- 0.22: fuller sounds. Layered oscillators, unison and stereo width, slow movement in the filter, the period's chorus,
  // delay and reverb. OUTPUT of each is set by measurement (see Tools/calibrate_neon_presets.py).
  {"Bass/Gated Drive", {{Drift, 0}, {VoiceMode, 1}, {Osc1Wave, 0}, {Osc2Wave, 0}, {Osc2Octave, -1}, {Osc2Level, 70}, {Osc2Fine, 6}, {SubLevel, 35}, {Unison, 2}, {Detune, 14}, {Spread, 30},
                        {Cutoff, 720}, {Resonance, 30}, {FilterEnv, 50}, {FDecay, 220}, {FSustain, 18}, {FilterDrive, 35}, {Attack, 1}, {Decay, 420}, {Sustain, 65}, {Release, 80},
                        {ChorusMode, 1}, {ChorusMix, 45}, {RevMix, 0}, {Output, -1.2f}, {-1, 0}}},
  {"Bass/Dark Rubber", {{Drift, 0}, {VoiceMode, 2}, {Glide, 18}, {Osc1Wave, 0}, {Osc2Wave, 1}, {Osc2Octave, -1}, {Osc2Level, 55}, {SubLevel, 40}, {Cutoff, 430}, {Resonance, 48}, {FilterEnv, 68},
                        {FDecay, 160}, {FSustain, 14}, {FilterDrive, 50}, {Attack, 1}, {Decay, 380}, {Sustain, 60}, {Release, 70}, {RevMix, 0}, {Output, 0.2f}, {-1, 0}}},
  {"Bass/Pulse Machine", {{Drift, 0}, {VoiceMode, 1}, {Osc1Wave, 1}, {Osc1PW, 32}, {PwmDepth, 35}, {LfoRate, 3.f}, {LfoDelay, 0}, {Osc2Wave, 1}, {Osc2Octave, -1}, {Osc2Level, 60}, {SubLevel, 30},
                          {Cutoff, 820}, {Resonance, 25}, {FilterEnv, 40}, {FDecay, 240}, {FSustain, 22}, {Attack, 1}, {Decay, 450}, {Sustain, 60}, {Release, 90}, {ChorusMode, 2}, {ChorusMix, 40}, {RevMix, 0}, {Output, -2.5f}, {-1, 0}}},
  {"Bass/Low Supersaw", {{Drift, 10}, {VoiceMode, 1}, {Unison, 5}, {Detune, 18}, {Spread, 40}, {Osc1Wave, 0}, {Osc2Wave, 0}, {Osc2Octave, -1}, {Osc2Level, 70}, {SubLevel, 30},
                         {Cutoff, 1000}, {Resonance, 14}, {FilterEnv, 30}, {FDecay, 350}, {FSustain, 30}, {Attack, 3}, {Decay, 600}, {Sustain, 75}, {Release, 130}, {ChorusMode, 2}, {ChorusMix, 50}, {RevMix, 0}, {Output, 0.8f}, {-1, 0}}},
  {"Bass/Hollow FM", {{Drift, 0}, {VoiceMode, 1}, {Osc1Wave, 3}, {Osc1Level, 45}, {SubLevel, 25}, {FmLevel, 70}, {FmRatio, 1}, {FmIndex, 58}, {FmDecay, 420}, {FmVelocity, 75},
                      {FilterType, 1}, {Cutoff, 3000}, {Attack, 1}, {Decay, 700}, {Sustain, 45}, {Release, 120}, {ChorusMode, 1}, {ChorusMix, 35}, {RevMix, 0}, {Output, 2.0f}, {-1, 0}}},
  {"Bass/Rolling Octaves", {{Drift, 0}, {Arp, 1}, {ArpRate, 3}, {ArpMode, 0}, {ArpOctaves, 2}, {Gate, 42}, {VoiceMode, 1}, {Osc1Wave, 0}, {Osc2Wave, 1}, {Osc2Level, 50}, {Osc2Octave, -1}, {SubLevel, 30},
                            {Cutoff, 900}, {Resonance, 35}, {FilterEnv, 55}, {FDecay, 170}, {FSustain, 10}, {Attack, 1}, {Decay, 260}, {Sustain, 45}, {Release, 60}, {ChorusMode, 1}, {RevMix, 6}, {Output, 0.7f}, {-1, 0}}},
  {"Polysynth/Super Juno", {{Drift, 30}, {Unison, 4}, {Detune, 28}, {Spread, 85}, {Osc1Wave, 0}, {Osc2Wave, 0}, {Osc2Level, 70}, {Osc2Fine, 9}, {SubLevel, 20}, {Cutoff, 2600}, {Resonance, 12}, {FilterEnv, 28},
                            {FDecay, 900}, {FSustain, 35}, {Attack, 10}, {Decay, 900}, {Sustain, 75}, {Release, 650}, {ChorusMode, 3}, {DelayMix, 12}, {DelaySync, 3}, {DelayFeedback, 30},
                            {RevMix, 22}, {RevType, 1}, {RevDecay, 3.5f}, {Width, 140}, {Output, -7.4f}, {-1, 0}}},
  {"Polysynth/Vangelis Brass", {{Drift, 20}, {Unison, 3}, {Detune, 22}, {Spread, 60}, {Osc1Wave, 0}, {Osc2Wave, 0}, {Osc2Level, 85}, {Osc2Fine, -8}, {Cutoff, 520}, {Resonance, 18}, {FilterEnv, 75},
                                {FAttack, 120}, {FDecay, 700}, {FSustain, 50}, {Attack, 70}, {Sustain, 85}, {Release, 450}, {LfoPitch, 8}, {LfoRate, 5.f}, {LfoDelay, 900}, {ChorusMode, 1},
                                {RevMix, 28}, {RevType, 1}, {RevDecay, 4}, {Width, 120}, {Output, -10.0f}, {-1, 0}}},
  {"Polysynth/Detroit Chord", {{Drift, 15}, {Osc1Wave, 1}, {Osc1PW, 40}, {PwmDepth, 30}, {LfoRate, .7f}, {LfoDelay, 0}, {Osc2Wave, 0}, {Osc2Semi, 7}, {Osc2Level, 45}, {Osc2Fine, 8}, {SubLevel, 20},
                               {Cutoff, 1900}, {Resonance, 18}, {FilterEnv, 35}, {FDecay, 400}, {FSustain, 30}, {Attack, 4}, {Decay, 700}, {Sustain, 55}, {Release, 380}, {ChorusMode, 3},
                               {DelayMix, 20}, {DelaySync, 3}, {RevMix, 20}, {RevType, 0}, {Width, 130}, {Output, -9.8f}, {-1, 0}}},
  {"Polysynth/Midnight Poly", {{Drift, 40}, {Unison, 2}, {Detune, 25}, {Spread, 70}, {Osc1Wave, 0}, {Osc2Wave, 2}, {Osc2Octave, -1}, {Osc2Level, 55}, {SubLevel, 28}, {Cutoff, 1500}, {Resonance, 18},
                               {FilterEnv, 35}, {FDecay, 800}, {FSustain, 35}, {LfoFilter, 10}, {LfoRate, .25f}, {LfoDelay, 0}, {Attack, 40}, {Decay, 1000}, {Sustain, 75}, {Release, 900},
                               {ChorusMode, 2}, {RevMix, 25}, {RevType, 0}, {RevDecay, 3.5f}, {Width, 130}, {Output, -7.8f}, {-1, 0}}},
  {"Polysynth/Bright Brass Stab", {{FilterType, 1}, {Unison, 3}, {Detune, 18}, {Spread, 55}, {Osc1Wave, 0}, {Osc2Wave, 0}, {Osc2Level, 85}, {Osc2Fine, -8}, {Cutoff, 1100}, {Resonance, 25}, {FilterEnv, 55},
                                   {FAttack, 30}, {FDecay, 500}, {FSustain, 30}, {Attack, 15}, {Decay, 500}, {Sustain, 60}, {Release, 280}, {ChorusMode, 1}, {DelayMix, 14}, {DelaySync, 3},
                                   {RevMix, 22}, {RevType, 0}, {Width, 120}, {Output, -6.6f}, {-1, 0}}},
  {"Polysynth/Pulse Choir", {{Drift, 30}, {Unison, 3}, {Detune, 24}, {Spread, 80}, {Osc1Wave, 1}, {Osc1PW, 30}, {PwmDepth, 45}, {LfoRate, .5f}, {LfoDelay, 0}, {Osc2Wave, 1}, {Osc2Level, 70}, {Osc2Fine, -9},
                              {Cutoff, 2200}, {Resonance, 12}, {FilterEnv, 20}, {Attack, 120}, {Decay, 1200}, {Sustain, 85}, {Release, 1100}, {ChorusMode, 3}, {RevMix, 35}, {RevType, 1},
                              {RevDecay, 5}, {Width, 140}, {Output, -22.9f}, {-1, 0}}},
  {"Polysynth/Laser Stab", {{Osc1Wave, 0}, {Osc1Level, 50}, {Osc2Wave, 0}, {Osc2Sync, 1}, {Osc2Octave, 1}, {Osc2Semi, 7}, {Osc2Level, 70}, {Unison, 2}, {Detune, 20}, {Cutoff, 1800}, {Resonance, 20},
                            {FilterEnv, 60}, {FDecay, 180}, {FSustain, 0}, {Attack, 1}, {Decay, 350}, {Sustain, 0}, {Release, 200}, {ChorusMode, 1}, {DelayMix, 30}, {DelaySync, 3}, {RevMix, 18}, {Output, 3.7f}, {-1, 0}}},
  {"Polysynth/Hold Me Tight", {{Drift, 25}, {Unison, 4}, {Detune, 26}, {Spread, 90}, {Osc1Wave, 0}, {Osc2Wave, 0}, {Osc2Level, 60}, {Osc2Fine, 11}, {Cutoff, 4200}, {Resonance, 8}, {FilterEnv, 10},
                             {Attack, 60}, {Decay, 900}, {Sustain, 80}, {Release, 800}, {ChorusMode, 1}, {DelayMix, 15}, {DelaySync, 3}, {RevMix, 25}, {RevType, 1}, {Width, 150}, {Output, -7.8f}, {-1, 0}}},
  {"Lead/Stadium Saws", {{VoiceMode, 1}, {Glide, 45}, {Unison, 6}, {Detune, 32}, {Spread, 70}, {Osc1Wave, 0}, {Osc2Wave, 0}, {Osc2Level, 60}, {Osc2Fine, 9}, {Cutoff, 3800}, {Resonance, 12}, {FilterEnv, 28},
                         {LfoPitch, 12}, {LfoRate, 5.5f}, {LfoDelay, 500}, {Attack, 4}, {Release, 260}, {ChorusMode, 1}, {DelayMix, 30}, {DelaySync, 3}, {DelayFeedback, 38},
                         {RevMix, 24}, {RevType, 1}, {RevDecay, 3.5f}, {Width, 120}, {Output, -1.2f}, {-1, 0}}},
  {"Lead/Pulse Chaser", {{VoiceMode, 1}, {Glide, 20}, {Unison, 2}, {Detune, 16}, {Osc1Wave, 1}, {Osc1PW, 45}, {PwmDepth, 30}, {LfoRate, 4.5f}, {LfoDelay, 300}, {Osc2Wave, 0}, {Osc2Level, 40}, {Osc2Octave, 1},
                         {Cutoff, 2600}, {Resonance, 22}, {FilterEnv, 35}, {FDecay, 500}, {FSustain, 40}, {Attack, 3}, {Release, 200}, {ChorusMode, 1}, {DelayMix, 28}, {DelaySync, 3}, {RevMix, 16}, {Output, -3.4f}, {-1, 0}}},
  {"Lead/FM Whistle", {{VoiceMode, 2}, {Glide, 60}, {Osc1Wave, 3}, {Osc1Level, 25}, {FmLevel, 75}, {FmRatio, 2}, {FmIndex, 38}, {FmDecay, 1200}, {FilterType, 1}, {Cutoff, 8000}, {LfoPitch, 14}, {LfoRate, 5.5f},
                       {LfoDelay, 450}, {Attack, 8}, {Release, 240}, {ChorusMode, 1}, {ChorusMix, 40}, {DelayMix, 26}, {DelaySync, 3}, {RevMix, 22}, {RevType, 1}, {Output, 1.8f}, {-1, 0}}},
  {"Lead/Ladder Scream", {{VoiceMode, 1}, {Osc1Wave, 0}, {Osc2Wave, 0}, {Osc2Sync, 1}, {Osc2Semi, 7}, {Osc2Level, 75}, {Osc1Level, 50}, {Cutoff, 1800}, {Resonance, 55}, {FilterDrive, 50}, {FilterEnv, 55},
                          {FDecay, 380}, {FSustain, 45}, {Attack, 2}, {Release, 180}, {LfoPitch, 8}, {LfoDelay, 600}, {DelayMix, 22}, {DelaySync, 3}, {RevMix, 14}, {Output, 0.2f}, {-1, 0}}},
  {"Lead/Arena Square", {{VoiceMode, 1}, {Glide, 30}, {Unison, 3}, {Detune, 20}, {Spread, 60}, {Osc1Wave, 1}, {Osc1PW, 50}, {Osc2Wave, 1}, {Osc2Level, 55}, {Osc2Fine, 6}, {Cutoff, 3000}, {Resonance, 15},
                         {FilterEnv, 25}, {LfoPitch, 10}, {LfoDelay, 500}, {Attack, 3}, {Release, 240}, {ChorusMode, 1}, {DelayMix, 30}, {DelaySync, 3}, {RevMix, 22}, {RevType, 1}, {Output, -3.5f}, {-1, 0}}},
  {"Lead/Rubber Mono", {{VoiceMode, 2}, {Glide, 90}, {Osc1Wave, 0}, {Osc2Wave, 0}, {Osc2Level, 55}, {Osc2Fine, 8}, {SubLevel, 20}, {Cutoff, 1600}, {Resonance, 35}, {FilterDrive, 30}, {FilterEnv, 40},
                        {FDecay, 450}, {FSustain, 40}, {Attack, 3}, {Release, 200}, {LfoPitch, 9}, {LfoDelay, 500}, {ChorusMode, 1}, {DelayMix, 24}, {DelaySync, 3}, {RevMix, 14}, {Output, -0.4f}, {-1, 0}}},
  {"Pad/Aurora", {{Drift, 40}, {Unison, 5}, {Detune, 35}, {Spread, 100}, {Osc1Wave, 0}, {Osc2Wave, 0}, {Osc2Level, 65}, {Osc2Fine, 10}, {Cutoff, 1500}, {Resonance, 12}, {FilterEnv, 18}, {LfoFilter, 18},
                  {LfoRate, .12f}, {LfoDelay, 0}, {Attack, 1400}, {Decay, 2000}, {Sustain, 90}, {Release, 4000}, {ChorusMode, 3}, {DelayMix, 14}, {DelaySync, 5}, {RevMix, 40}, {RevType, 1},
                  {RevDecay, 9}, {Width, 160}, {Output, -10.3f}, {-1, 0}}},
  {"Pad/Cassette Choir", {{Drift, 60}, {Unison, 3}, {Detune, 22}, {Spread, 80}, {Osc1Wave, 2}, {Osc2Wave, 1}, {Osc2Level, 50}, {Osc2Octave, 1}, {NoiseLevel, 4}, {Cutoff, 1800}, {Resonance, 10},
                          {FilterEnv, 15}, {LfoPitch, 5}, {LfoRate, .4f}, {LfoDelay, 0}, {Attack, 900}, {Sustain, 90}, {Release, 2800}, {ChorusMode, 2}, {RevMix, 38}, {RevType, 1}, {RevDecay, 6},
                          {Width, 140}, {Output, -10.0f}, {-1, 0}}},
  {"Pad/Dark Cathedral", {{Drift, 35}, {Unison, 4}, {Detune, 30}, {Spread, 90}, {Osc1Wave, 0}, {Osc2Wave, 0}, {Osc2Octave, -1}, {Osc2Level, 70}, {Cutoff, 700}, {Resonance, 22}, {FilterEnv, 25},
                          {LfoFilter, 15}, {LfoRate, .09f}, {LfoDelay, 0}, {Attack, 2000}, {Sustain, 95}, {Release, 5000}, {ChorusMode, 2}, {RevMix, 50}, {RevType, 1}, {RevDecay, 12}, {RevDamp, 60},
                          {Width, 150}, {Output, -8.6f}, {-1, 0}}},
  {"Pad/Glass Breath", {{Drift, 20}, {Osc1Wave, 3}, {Osc1Level, 45}, {FmLevel, 55}, {FmRatio, 4}, {FmIndex, 20}, {FmDecay, 6000}, {NoiseLevel, 6}, {Cutoff, 7000}, {FilterType, 1}, {LfoFilter, 12},
                        {LfoRate, .18f}, {LfoDelay, 0}, {Attack, 1500}, {Sustain, 100}, {Release, 3500}, {ChorusMode, 4}, {DelayMix, 18}, {DelaySync, 3}, {RevMix, 45}, {RevType, 1}, {RevDecay, 8},
                        {Width, 150}, {Output, -1.3f}, {-1, 0}}},
  {"Pad/Slow Strings 80", {{Drift, 25}, {Unison, 3}, {Detune, 24}, {Spread, 85}, {Osc1Wave, 0}, {Osc2Wave, 0}, {Osc2Level, 70}, {Osc2Fine, 12}, {Cutoff, 2600}, {Resonance, 6}, {FilterEnv, 10},
                           {LfoPitch, 10}, {LfoRate, 5.f}, {LfoDelay, 1000}, {Attack, 700}, {Sustain, 90}, {Release, 1800}, {ChorusMode, 3}, {RevMix, 32}, {RevType, 1}, {RevDecay, 5}, {Width, 140}, {Output, -8.1f}, {-1, 0}}},
  {"Pad/Polar Drift", {{Drift, 45}, {Unison, 3}, {Detune, 26}, {Spread, 90}, {Osc1Wave, 1}, {Osc1PW, 40}, {PwmDepth, 70}, {LfoRate, .2f}, {LfoDelay, 0}, {Osc2Wave, 0}, {Osc2Level, 55}, {Osc2Fine, 12},
                         {Cutoff, 1300}, {Resonance, 18}, {FilterEnv, 15}, {Attack, 1000}, {Sustain, 90}, {Release, 3000}, {ChorusMode, 3}, {DelayMix, 20}, {DelaySync, 3}, {RevMix, 35}, {RevType, 1}, {RevDecay, 7},
                         {Width, 150}, {Output, -15.8f}, {-1, 0}}},
  {"Pad/Warm Wash", {{Drift, 50}, {Unison, 2}, {Detune, 22}, {Spread, 80}, {Osc1Wave, 0}, {Osc2Wave, 2}, {Osc2Level, 60}, {SubLevel, 25}, {Cutoff, 900}, {Resonance, 10}, {FilterEnv, 20},
                       {FAttack, 1500}, {FDecay, 2500}, {FSustain, 55}, {Attack, 1200}, {Sustain, 95}, {Release, 3500}, {ChorusMode, 1}, {RevMix, 35}, {RevType, 1}, {RevDecay, 6}, {Width, 135}, {Output, -7.9f}, {-1, 0}}},
  {"Keys/Rhodes 84", {{Osc1Wave, 3}, {Osc1Level, 15}, {FmLevel, 85}, {FmRatio, 1}, {FmIndex, 38}, {FmDecay, 1400}, {FmVelocity, 85}, {FilterType, 1}, {Cutoff, 9000}, {Attack, 2}, {Decay, 2800},
                      {Sustain, 20}, {Release, 600}, {LfoAmp, 12}, {LfoRate, 4.f}, {LfoDelay, 300}, {ChorusMode, 1}, {ChorusMix, 70}, {RevMix, 22}, {RevType, 0}, {Width, 120}, {Output, -7.3f}, {-1, 0}}},
  {"Keys/Digital Bell Pad", {{Osc1Level, 0}, {FmLevel, 85}, {FmRatio, 7}, {FmIndex, 40}, {FmDecay, 3000}, {FmVelocity, 70}, {FilterType, 1}, {Cutoff, 12000}, {Attack, 8}, {Decay, 3500}, {Sustain, 35},
                             {Release, 2800}, {ChorusMode, 4}, {DelayMix, 22}, {DelaySync, 3}, {RevMix, 40}, {RevType, 1}, {RevDecay, 6}, {Width, 140}, {Output, -7.2f}, {-1, 0}}},
  {"Keys/Soft Clav", {{Osc1Wave, 1}, {Osc1PW, 20}, {Osc2Wave, 0}, {Osc2Level, 25}, {Cutoff, 1400}, {Resonance, 22}, {FilterEnv, 60}, {FDecay, 140}, {FSustain, 0}, {Attack, 1}, {Decay, 350},
                       {Sustain, 0}, {Release, 120}, {ChorusMode, 1}, {DelayMix, 15}, {DelaySync, 4}, {RevMix, 16}, {RevType, 0}, {Output, -6.8f}, {-1, 0}}},
  {"Keys/Glass Piano", {{Osc1Wave, 2}, {Osc1Level, 30}, {FmLevel, 70}, {FmRatio, 3}, {FmIndex, 30}, {FmDecay, 700}, {FmVelocity, 85}, {FilterType, 1}, {Cutoff, 10000}, {Attack, 1}, {Decay, 2400},
                        {Sustain, 15}, {Release, 500}, {ChorusMode, 1}, {ChorusMix, 50}, {RevMix, 26}, {RevType, 1}, {RevDecay, 4}, {Width, 120}, {Output, -6.7f}, {-1, 0}}},
  {"Pluck/Marimba Dream", {{Osc1Wave, 3}, {Osc1Level, 40}, {FmLevel, 75}, {FmRatio, 4}, {FmIndex, 28}, {FmDecay, 90}, {FmVelocity, 70}, {FilterType, 1}, {Cutoff, 9000}, {Attack, 1}, {Decay, 800},
                           {Sustain, 0}, {Release, 350}, {DelayMix, 28}, {DelaySync, 3}, {DelayFeedback, 40}, {RevMix, 28}, {RevType, 1}, {RevDecay, 4}, {Width, 130}, {Output, 1.4f}, {-1, 0}}},
  {"Pluck/Echo Stab", {{Osc1Wave, 0}, {Osc2Wave, 1}, {Osc2Level, 55}, {Osc2Fine, 7}, {Unison, 2}, {Detune, 18}, {Cutoff, 600}, {Resonance, 28}, {FilterEnv, 80}, {FAttack, 1}, {FDecay, 200},
                       {FSustain, 0}, {Attack, 1}, {Decay, 450}, {Sustain, 0}, {Release, 250}, {ChorusMode, 1}, {DelayMix, 35}, {DelaySync, 3}, {DelayFeedback, 48}, {RevMix, 20}, {Output, 0.4f}, {-1, 0}}},
  {"Pluck/Synth Harp", {{Osc1Wave, 2}, {Osc2Wave, 0}, {Osc2Level, 40}, {Osc2Octave, 1}, {Unison, 2}, {Detune, 14}, {Cutoff, 1500}, {Resonance, 15}, {FilterEnv, 60}, {FDecay, 400}, {FSustain, 0},
                        {Attack, 1}, {Decay, 1200}, {Sustain, 0}, {Release, 500}, {ChorusMode, 3}, {DelayMix, 30}, {DelaySync, 6}, {DelayPingPong, 1}, {RevMix, 30}, {RevType, 1}, {RevDecay, 5}, {Width, 140}, {Output, -3.8f}, {-1, 0}}},
  {"Pluck/Chirp Sequence", {{VoiceMode, 1}, {Arp, 1}, {ArpRate, 3}, {ArpMode, 0}, {ArpOctaves, 2}, {Gate, 45}, {Osc1Wave, 0}, {Osc1Level, 100}, {Osc2Wave, 1}, {Osc2Level, 60}, {Cutoff, 520}, {Resonance, 40}, {FilterEnv, 85},
                            {FAttack, 1}, {FDecay, 120}, {FSustain, 10}, {Attack, 1}, {Decay, 220}, {Sustain, 25}, {Release, 60}, {DelayMix, 24}, {DelaySync, 3}, {RevMix, 12}, {Output, 6.0f}, {-1, 0}}},
  {"Arp/Drive Time", {{Arp, 1}, {ArpRate, 3}, {ArpMode, 2}, {ArpOctaves, 3}, {Gate, 55}, {Osc1Wave, 0}, {Osc2Wave, 1}, {Osc2Level, 55}, {Osc2Fine, 7}, {Unison, 2}, {Detune, 16}, {Cutoff, 1800}, {Resonance, 30},
                      {FilterEnv, 55}, {FDecay, 220}, {FSustain, 12}, {Attack, 1}, {Decay, 320}, {Sustain, 40}, {Release, 140}, {ChorusMode, 1}, {DelayMix, 30}, {DelaySync, 3}, {RevMix, 18}, {Width, 130}, {Output, -2.3f}, {-1, 0}}},
  {"Arp/Pulse Cascade", {{Arp, 1}, {ArpRate, 4}, {ArpMode, 3}, {ArpOctaves, 3}, {Gate, 40}, {Osc1Wave, 1}, {Osc1PW, 35}, {PwmDepth, 40}, {LfoRate, .8f}, {LfoDelay, 0}, {Osc2Wave, 0}, {Osc2Level, 40}, {Osc2Octave, 1},
                         {Cutoff, 2400}, {Resonance, 20}, {FilterEnv, 45}, {FDecay, 250}, {FSustain, 8}, {Attack, 1}, {Decay, 300}, {Sustain, 25}, {Release, 160}, {ChorusMode, 2}, {DelayMix, 35}, {DelaySync, 6}, {RevMix, 25}, {Width, 140}, {Output, -4.2f}, {-1, 0}}},
  {"Arp/Dark Octaves", {{Arp, 1}, {ArpRate, 1}, {ArpMode, 0}, {ArpOctaves, 2}, {Gate, 70}, {VoiceMode, 1}, {Osc1Wave, 0}, {Osc2Wave, 0}, {Osc2Level, 60}, {Osc2Octave, -1}, {SubLevel, 40}, {Cutoff, 500},
                        {Resonance, 40}, {FilterEnv, 50}, {FDecay, 300}, {FSustain, 15}, {Attack, 2}, {Decay, 400}, {Sustain, 55}, {Release, 120}, {ChorusMode, 1}, {DelayMix, 18}, {DelaySync, 2}, {RevMix, 10}, {Output, -0.9f}, {-1, 0}}},
  {"Arp/Lush Chords", {{Arp, 1}, {ArpRate, 1}, {ArpMode, 5}, {Gate, 70}, {Unison, 3}, {Detune, 24}, {Spread, 85}, {Osc1Wave, 0}, {Osc2Wave, 0}, {Osc2Level, 60}, {Osc2Fine, 10}, {Cutoff, 2400}, {Resonance, 12},
                       {FilterEnv, 30}, {FDecay, 400}, {FSustain, 30}, {Attack, 8}, {Decay, 600}, {Sustain, 60}, {Release, 500}, {ChorusMode, 3}, {DelayMix, 25}, {DelaySync, 3}, {RevMix, 30}, {RevType, 1}, {Width, 150}, {Output, -7.2f}, {-1, 0}}},
  {"FX/Siren Patrol", {{VoiceMode, 1}, {Osc1Wave, 3}, {Osc2Wave, 2}, {Osc2Level, 40}, {LfoPitch, 100}, {LfoRate, 1.2f}, {LfoDelay, 0}, {Cutoff, 5000}, {FilterType, 1}, {Attack, 40}, {Release, 600},
                         {DelayMix, 28}, {DelaySync, 3}, {RevMix, 35}, {RevType, 1}, {RevDecay, 6}, {Width, 130}, {Output, -3.8f}, {-1, 0}}},
  {"FX/Cosmic Wash", {{Osc1Level, 0}, {NoiseLevel, 100}, {FilterType, 2}, {Cutoff, 1400}, {Resonance, 60}, {LfoFilter, 60}, {LfoRate, .08f}, {LfoDelay, 0}, {Osc2Level, 80}, {Osc2Wave, 0}, {Osc2Fine, 12},
                        {Unison, 3}, {Detune, 40}, {Attack, 2500}, {Sustain, 100}, {Release, 5000}, {ChorusMode, 4}, {DelayMix, 25}, {DelaySync, 3}, {RevMix, 60}, {RevType, 1}, {RevDecay, 14}, {Width, 180}, {Output, 6.0f}, {-1, 0}}},
  {"FX/Heartbeat Sub", {{VoiceMode, 1}, {Osc1Wave, 3}, {Osc1Level, 90}, {SubLevel, 40}, {LfoAmp, 85}, {LfoShape, 0}, {LfoRate, 1.1f}, {LfoDelay, 0}, {Cutoff, 400}, {Attack, 5}, {Release, 400},
                          {RevMix, 15}, {RevType, 0}, {Output, -1.8f}, {-1, 0}}},
  {"FX/Static Rain", {{Osc1Level, 0}, {NoiseLevel, 100}, {FilterType, 3}, {Cutoff, 5500}, {Resonance, 20}, {LfoShape, 3}, {LfoFilter, 45}, {LfoAmp, 40}, {LfoRate, 9.f}, {LfoDelay, 0}, {Attack, 800},
                        {Sustain, 100}, {Release, 2500}, {ChorusMode, 4}, {RevMix, 40}, {RevType, 1}, {RevDecay, 7}, {Width, 160}, {Output, 2.9f}, {-1, 0}}},
  // ---- 0.22, second batch
  {"Bass/Night Rider", {{Drift, 0}, {VoiceMode, 1}, {Osc1Wave, 0}, {Osc2Wave, 1}, {Osc2Octave, -1}, {Osc2Level, 60}, {SubLevel, 35}, {Cutoff, 600}, {Resonance, 40}, {FilterEnv, 60}, {FDecay, 200}, {FSustain, 15}, {FilterDrive, 30}, {Decay, 400}, {Sustain, 60}, {Release, 70}, {ChorusMode, 1}, {RevMix, 0}, {Output, -1.4f}, {-1, 0}}},
  {"Bass/Neon Pulse Sub", {{Drift, 0}, {VoiceMode, 1}, {Osc1Wave, 1}, {Osc1PW, 40}, {Osc2Wave, 3}, {Osc2Octave, -1}, {Osc2Level, 70}, {SubLevel, 20}, {Cutoff, 700}, {Resonance, 18}, {FilterEnv, 35}, {FDecay, 260}, {Decay, 500}, {Sustain, 65}, {Release, 100}, {ChorusMode, 2}, {ChorusMix, 35}, {RevMix, 0}, {Output, -1.8f}, {-1, 0}}},
  {"Bass/Acid Drive", {{Drift, 0}, {VoiceMode, 2}, {Glide, 40}, {Osc1Wave, 0}, {Osc2Level, 0}, {SubLevel, 15}, {Cutoff, 380}, {Resonance, 72}, {FilterEnv, 82}, {FDecay, 190}, {FSustain, 0}, {FilterDrive, 55}, {Decay, 300}, {Sustain, 55}, {Release, 60}, {DelayMix, 12}, {DelaySync, 4}, {RevMix, 0}, {Output, 1.0f}, {-1, 0}}},
  {"Bass/Fat Detune", {{Drift, 15}, {VoiceMode, 1}, {Unison, 3}, {Detune, 24}, {Spread, 30}, {Osc1Wave, 0}, {Osc2Wave, 0}, {Osc2Level, 60}, {Osc2Fine, -10}, {Cutoff, 780}, {Resonance, 20}, {FilterEnv, 40}, {FDecay, 300}, {FSustain, 25}, {Decay, 500}, {Sustain, 70}, {Release, 110}, {ChorusMode, 1}, {RevMix, 0}, {Output, -1.7f}, {-1, 0}}},
  {"Bass/Soft Sine Sub", {{Drift, 0}, {VoiceMode, 1}, {Osc1Wave, 3}, {Osc1Level, 90}, {SubLevel, 45}, {Cutoff, 500}, {FilterEnv, 10}, {Attack, 6}, {Decay, 600}, {Sustain, 85}, {Release, 140}, {RevMix, 0}, {Output, -3.5f}, {-1, 0}}},
  {"Bass/Triplet Walker", {{Drift, 0}, {VoiceMode, 1}, {Arp, 1}, {ArpRate, 4}, {ArpMode, 2}, {ArpOctaves, 2}, {Gate, 55}, {Osc1Wave, 0}, {Osc2Wave, 1}, {Osc2Level, 45}, {Osc2Octave, -1}, {SubLevel, 25}, {Cutoff, 850}, {Resonance, 30}, {FilterEnv, 50}, {FDecay, 180}, {FSustain, 12}, {Decay, 260}, {Sustain, 45}, {Release, 60}, {ChorusMode, 1}, {Output, -0.0f}, {-1, 0}}},
  {"Polysynth/Retro Brass 2", {{Drift, 20}, {Unison, 2}, {Detune, 18}, {Osc1Wave, 0}, {Osc2Wave, 0}, {Osc2Level, 80}, {Osc2Fine, 7}, {Cutoff, 650}, {Resonance, 20}, {FilterEnv, 70}, {FAttack, 50}, {FDecay, 500}, {FSustain, 45}, {Attack, 30}, {Sustain, 85}, {Release, 300}, {ChorusMode, 1}, {RevMix, 20}, {RevType, 0}, {Width, 120}, {Output, -9.2f}, {-1, 0}}},
  {"Polysynth/String Stack", {{Drift, 30}, {Unison, 4}, {Detune, 22}, {Spread, 90}, {Osc1Wave, 0}, {Osc2Wave, 0}, {Osc2Level, 75}, {Osc2Octave, 1}, {Osc2Fine, 6}, {Cutoff, 3000}, {FilterEnv, 8}, {LfoPitch, 7}, {LfoRate, 5.f}, {LfoDelay, 800}, {Attack, 320}, {Sustain, 90}, {Release, 1200}, {ChorusMode, 3}, {RevMix, 30}, {RevType, 1}, {RevDecay, 5}, {Width, 150}, {Output, -9.3f}, {-1, 0}}},
  {"Polysynth/Outrun Chords", {{Drift, 25}, {Unison, 3}, {Detune, 28}, {Spread, 80}, {Osc1Wave, 0}, {Osc2Wave, 1}, {Osc2Level, 55}, {Osc2Fine, 9}, {SubLevel, 15}, {Cutoff, 2000}, {Resonance, 18}, {FilterEnv, 40}, {FDecay, 600}, {FSustain, 25}, {Attack, 5}, {Decay, 700}, {Sustain, 55}, {Release, 450}, {ChorusMode, 3}, {DelayMix, 18}, {DelaySync, 3}, {RevMix, 22}, {RevType, 1}, {Width, 140}, {Output, -7.1f}, {-1, 0}}},
  {"Polysynth/Grid Runner", {{Drift, 10}, {Osc1Wave, 1}, {Osc1PW, 28}, {Osc2Wave, 0}, {Osc2Level, 50}, {Osc2Fine, 8}, {Unison, 2}, {Detune, 20}, {Cutoff, 1100}, {Resonance, 30}, {FilterEnv, 55}, {FDecay, 260}, {FSustain, 15}, {Attack, 2}, {Decay, 350}, {Sustain, 30}, {Release, 180}, {ChorusMode, 2}, {DelayMix, 22}, {DelaySync, 2}, {RevMix, 16}, {Width, 130}, {Output, -11.3f}, {-1, 0}}},
  {"Polysynth/Tape Poly", {{Drift, 65}, {Osc1Wave, 0}, {Osc2Wave, 2}, {Osc2Level, 50}, {Osc2Fine, 10}, {Unison, 2}, {Detune, 20}, {Cutoff, 1700}, {Resonance, 12}, {FilterEnv, 22}, {LfoPitch, 5}, {LfoRate, .5f}, {LfoDelay, 0}, {Attack, 25}, {Sustain, 75}, {Release, 600}, {ChorusMode, 2}, {DelayMix, 15}, {DelaySync, 3}, {RevMix, 25}, {RevType, 1}, {Width, 130}, {Output, -8.2f}, {-1, 0}}},
  {"Polysynth/Sunset Stab", {{FilterType, 1}, {Osc1Wave, 0}, {Osc2Wave, 0}, {Osc2Level, 80}, {Osc2Fine, 10}, {Unison, 3}, {Detune, 22}, {Spread, 70}, {Cutoff, 800}, {Resonance, 15}, {FilterEnv, 70}, {FDecay, 300}, {FSustain, 10}, {Attack, 1}, {Decay, 450}, {Sustain, 0}, {Release, 300}, {ChorusMode, 1}, {DelayMix, 22}, {DelaySync, 3}, {RevMix, 22}, {RevType, 1}, {Width, 140}, {Output, -0.4f}, {-1, 0}}},
  {"Polysynth/Crystal Poly", {{Osc1Wave, 0}, {Osc1Level, 60}, {FmLevel, 45}, {FmRatio, 3}, {FmIndex, 30}, {FmDecay, 900}, {Osc2Wave, 2}, {Osc2Level, 40}, {Osc2Octave, 1}, {Unison, 2}, {Detune, 18}, {Cutoff, 4500}, {FilterEnv, 20}, {FDecay, 600}, {Attack, 12}, {Sustain, 70}, {Release, 700}, {ChorusMode, 3}, {RevMix, 30}, {RevType, 1}, {RevDecay, 4.5f}, {Width, 140}, {Output, -8.2f}, {-1, 0}}},
  {"Polysynth/Wide Fifth", {{Drift, 20}, {Unison, 3}, {Detune, 26}, {Spread, 85}, {Osc1Wave, 0}, {Osc2Wave, 0}, {Osc2Semi, 7}, {Osc2Level, 55}, {Cutoff, 2400}, {Resonance, 10}, {FilterEnv, 25}, {FDecay, 700}, {FSustain, 40}, {Attack, 15}, {Sustain, 80}, {Release, 650}, {ChorusMode, 3}, {RevMix, 24}, {RevType, 1}, {Width, 150}, {Output, -6.3f}, {-1, 0}}},
  {"Lead/Neon Sunset", {{VoiceMode, 1}, {Glide, 40}, {Unison, 3}, {Detune, 24}, {Spread, 60}, {Osc1Wave, 0}, {Osc2Wave, 1}, {Osc2Level, 50}, {Osc2Fine, 8}, {Cutoff, 3200}, {Resonance, 15}, {FilterEnv, 30}, {LfoPitch, 11}, {LfoRate, 5.5f}, {LfoDelay, 450}, {Attack, 4}, {Release, 280}, {ChorusMode, 1}, {DelayMix, 30}, {DelaySync, 3}, {DelayFeedback, 40}, {RevMix, 24}, {RevType, 1}, {Output, -0.4f}, {-1, 0}}},
  {"Lead/Laser Whistle", {{VoiceMode, 2}, {Glide, 120}, {Osc1Wave, 3}, {Osc2Wave, 3}, {Osc2Octave, 1}, {Osc2Level, 35}, {FilterType, 1}, {Cutoff, 7000}, {LfoPitch, 12}, {LfoRate, 6.f}, {LfoDelay, 400}, {Attack, 15}, {Release, 260}, {DelayMix, 32}, {DelaySync, 3}, {RevMix, 26}, {RevType, 1}, {Output, -0.5f}, {-1, 0}}},
  {"Lead/Thin Square Solo", {{VoiceMode, 1}, {Glide, 25}, {Osc1Wave, 1}, {Osc1PW, 30}, {PwmDepth, 25}, {LfoRate, 4.f}, {LfoDelay, 300}, {LfoPitch, 8}, {Cutoff, 3500}, {Resonance, 20}, {FilterEnv, 25}, {Attack, 3}, {Release, 200}, {DelayMix, 26}, {DelaySync, 4}, {RevMix, 16}, {Output, -3.7f}, {-1, 0}}},
  {"Lead/Detuned Duo", {{VoiceMode, 1}, {Glide, 30}, {Osc1Wave, 0}, {Osc2Wave, 0}, {Osc2Level, 80}, {Osc2Fine, 14}, {Unison, 2}, {Detune, 18}, {Cutoff, 3000}, {Resonance, 14}, {FilterEnv, 30}, {LfoPitch, 10}, {LfoDelay, 500}, {Attack, 4}, {Release, 220}, {ChorusMode, 1}, {DelayMix, 24}, {DelaySync, 3}, {RevMix, 18}, {Output, -0.6f}, {-1, 0}}},
  {"Lead/Sync Sweep", {{VoiceMode, 1}, {Glide, 30}, {Osc1Level, 40}, {Osc2Wave, 0}, {Osc2Sync, 1}, {Osc2Octave, 1}, {Osc2Level, 85}, {FilterEnv, 20}, {Cutoff, 3800}, {LfoFilter, 20}, {LfoRate, .6f}, {LfoDelay, 0}, {Attack, 3}, {Release, 200}, {DelayMix, 24}, {DelaySync, 3}, {RevMix, 16}, {Output, -1.0f}, {-1, 0}}},
  {"Lead/Triangle Flute", {{VoiceMode, 2}, {Glide, 50}, {Osc1Wave, 2}, {Osc2Wave, 3}, {Osc2Octave, 1}, {Osc2Level, 30}, {NoiseLevel, 4}, {FilterType, 1}, {Cutoff, 5000}, {LfoPitch, 12}, {LfoRate, 5.f}, {LfoDelay, 500}, {Attack, 40}, {Release, 260}, {RevMix, 28}, {RevType, 1}, {Output, 0.2f}, {-1, 0}}},
  {"Lead/Chorus Lead", {{VoiceMode, 1}, {Glide, 35}, {Unison, 2}, {Detune, 20}, {Osc1Wave, 0}, {Osc2Wave, 0}, {Osc2Level, 55}, {Osc2Fine, 7}, {Cutoff, 2800}, {Resonance, 18}, {FilterEnv, 32}, {LfoPitch, 9}, {LfoDelay, 450}, {Attack, 3}, {Release, 220}, {ChorusMode, 3}, {DelayMix, 24}, {DelaySync, 3}, {RevMix, 20}, {Width, 130}, {Output, 1.6f}, {-1, 0}}},
  {"Pad/Midnight Glow", {{Drift, 40}, {Unison, 4}, {Detune, 30}, {Spread, 95}, {Osc1Wave, 0}, {Osc2Wave, 1}, {Osc2Level, 55}, {Osc2Fine, 8}, {Cutoff, 1300}, {Resonance, 14}, {FilterEnv, 15}, {LfoFilter, 14}, {LfoRate, .15f}, {LfoDelay, 0}, {Attack, 1100}, {Sustain, 90}, {Release, 3500}, {ChorusMode, 3}, {RevMix, 40}, {RevType, 1}, {RevDecay, 8}, {Width, 160}, {Output, -10.4f}, {-1, 0}}},
  {"Pad/Grid Mist", {{Drift, 50}, {Unison, 3}, {Detune, 20}, {Spread, 85}, {Osc1Wave, 2}, {Osc2Wave, 0}, {Osc2Level, 45}, {Osc2Octave, 1}, {NoiseLevel, 5}, {Cutoff, 1500}, {Resonance, 8}, {LfoFilter, 12}, {LfoRate, .2f}, {LfoDelay, 0}, {Attack, 1500}, {Sustain, 95}, {Release, 3500}, {ChorusMode, 4}, {RevMix, 45}, {RevType, 1}, {RevDecay, 9}, {Width, 160}, {Output, -8.5f}, {-1, 0}}},
  {"Pad/Chrome Choir", {{Drift, 25}, {Osc1Wave, 3}, {Osc1Level, 55}, {Osc2Wave, 2}, {Osc2Level, 45}, {Osc2Octave, 1}, {FmLevel, 40}, {FmRatio, 2}, {FmIndex, 15}, {FmDecay, 5000}, {Unison, 3}, {Detune, 18}, {Spread, 85}, {FilterType, 1}, {Cutoff, 5500}, {Attack, 900}, {Sustain, 100}, {Release, 3000}, {ChorusMode, 3}, {RevMix, 40}, {RevType, 1}, {RevDecay, 7}, {Width, 150}, {Output, -12.8f}, {-1, 0}}},
  {"Pad/Dusk Strings", {{Drift, 30}, {Unison, 3}, {Detune, 26}, {Spread, 90}, {Osc1Wave, 0}, {Osc2Wave, 0}, {Osc2Level, 70}, {Osc2Fine, 12}, {Cutoff, 1900}, {Resonance, 8}, {FilterEnv, 12}, {LfoPitch, 8}, {LfoRate, 5.f}, {LfoDelay, 1200}, {Attack, 800}, {Sustain, 90}, {Release, 2500}, {ChorusMode, 1}, {RevMix, 35}, {RevType, 1}, {RevDecay, 6}, {Width, 145}, {Output, -7.7f}, {-1, 0}}},
  {"Pad/Frozen Lake", {{Drift, 20}, {Osc1Wave, 3}, {Osc1Level, 50}, {FmLevel, 60}, {FmRatio, 5}, {FmIndex, 22}, {FmDecay, 7000}, {Unison, 2}, {Detune, 15}, {Spread, 90}, {FilterType, 1}, {Cutoff, 9000}, {Attack, 1800}, {Sustain, 100}, {Release, 4000}, {ChorusMode, 4}, {DelayMix, 20}, {DelaySync, 3}, {DelayFeedback, 45}, {RevMix, 50}, {RevType, 1}, {RevDecay, 11}, {Width, 170}, {Output, -12.5f}, {-1, 0}}},
  {"Pad/Electric Dream", {{Drift, 45}, {Unison, 3}, {Detune, 25}, {Spread, 85}, {Osc1Wave, 1}, {Osc1PW, 45}, {PwmDepth, 50}, {LfoRate, .35f}, {LfoDelay, 0}, {Osc2Wave, 0}, {Osc2Level, 60}, {Osc2Fine, 10}, {SubLevel, 15}, {Cutoff, 1600}, {Resonance, 16}, {FilterEnv, 18}, {Attack, 700}, {Sustain, 90}, {Release, 2800}, {ChorusMode, 3}, {DelayMix, 15}, {DelaySync, 3}, {RevMix, 35}, {RevType, 1}, {RevDecay, 7}, {Width, 150}, {Output, -15.4f}, {-1, 0}}},
  {"Pad/Analog Fog", {{Drift, 70}, {Unison, 2}, {Detune, 28}, {Spread, 85}, {Osc1Wave, 0}, {Osc2Wave, 2}, {Osc2Octave, -1}, {Osc2Level, 65}, {NoiseLevel, 6}, {Cutoff, 800}, {Resonance, 20}, {FilterEnv, 20}, {LfoFilter, 20}, {LfoRate, .1f}, {LfoDelay, 0}, {Attack, 1600}, {Sustain, 95}, {Release, 4000}, {ChorusMode, 2}, {RevMix, 45}, {RevType, 1}, {RevDecay, 10}, {RevDamp, 55}, {Width, 150}, {Output, -10.0f}, {-1, 0}}},
  {"Pad/Rising Sun", {{Drift, 30}, {Unison, 4}, {Detune, 32}, {Spread, 90}, {Osc1Wave, 0}, {Osc2Wave, 0}, {Osc2Level, 65}, {Osc2Fine, 11}, {Cutoff, 500}, {Resonance, 25}, {FilterEnv, 70}, {FAttack, 3500}, {FDecay, 3500}, {FSustain, 70}, {Attack, 1200}, {Sustain, 95}, {Release, 3500}, {ChorusMode, 3}, {RevMix, 38}, {RevType, 1}, {RevDecay, 8}, {Width, 155}, {Output, -10.4f}, {-1, 0}}},
  {"Keys/Wurli Neon", {{Osc1Wave, 2}, {Osc1Level, 40}, {FmLevel, 65}, {FmRatio, 1}, {FmIndex, 32}, {FmDecay, 700}, {FmVelocity, 85}, {FilterType, 1}, {Cutoff, 6000}, {Attack, 2}, {Decay, 1800}, {Sustain, 20}, {Release, 450}, {LfoAmp, 15}, {LfoRate, 5.f}, {LfoDelay, 200}, {ChorusMode, 1}, {ChorusMix, 60}, {RevMix, 20}, {RevType, 0}, {Output, -4.5f}, {-1, 0}}},
  {"Keys/Bell Tine", {{Osc1Wave, 3}, {Osc1Level, 25}, {FmLevel, 85}, {FmRatio, 6}, {FmIndex, 45}, {FmDecay, 1800}, {FmVelocity, 80}, {FilterType, 1}, {Cutoff, 12000}, {Attack, 1}, {Decay, 3000}, {Sustain, 0}, {Release, 2000}, {DelayMix, 18}, {DelaySync, 3}, {RevMix, 32}, {RevType, 1}, {RevDecay, 5}, {Width, 130}, {Output, -8.1f}, {-1, 0}}},
  {"Keys/Pluck Organ", {{Osc1Wave, 3}, {Osc2Wave, 3}, {Osc2Octave, 1}, {Osc2Level, 60}, {SubLevel, 35}, {FilterType, 1}, {Cutoff, 4500}, {Attack, 2}, {Decay, 300}, {Sustain, 40}, {Release, 90}, {ChorusMode, 3}, {RevMix, 15}, {RevType, 0}, {Width, 120}, {Output, -5.8f}, {-1, 0}}},
  {"Keys/Dream Piano", {{Osc1Wave, 2}, {Osc1Level, 35}, {FmLevel, 60}, {FmRatio, 2}, {FmIndex, 28}, {FmDecay, 900}, {FmVelocity, 85}, {FilterType, 1}, {Cutoff, 8000}, {Attack, 1}, {Decay, 2600}, {Sustain, 10}, {Release, 600}, {ChorusMode, 1}, {ChorusMix, 40}, {DelayMix, 15}, {DelaySync, 3}, {RevMix, 35}, {RevType, 1}, {RevDecay, 6}, {Width, 135}, {Output, -5.6f}, {-1, 0}}},
  {"Pluck/Kalimba Neon", {{Osc1Wave, 3}, {Osc1Level, 40}, {FmLevel, 70}, {FmRatio, 7}, {FmIndex, 25}, {FmDecay, 120}, {FilterType, 1}, {Cutoff, 7000}, {Attack, 1}, {Decay, 900}, {Sustain, 0}, {Release, 500}, {DelayMix, 30}, {DelaySync, 6}, {DelayFeedback, 40}, {RevMix, 28}, {RevType, 1}, {Width, 140}, {Output, 1.6f}, {-1, 0}}},
  {"Pluck/Pizzicato Poly", {{Osc1Wave, 0}, {Osc2Wave, 1}, {Osc2Level, 45}, {Unison, 2}, {Detune, 15}, {Cutoff, 900}, {Resonance, 15}, {FilterEnv, 65}, {FDecay, 150}, {FSustain, 0}, {Attack, 1}, {Decay, 260}, {Sustain, 0}, {Release, 120}, {ChorusMode, 1}, {DelayMix, 24}, {DelaySync, 4}, {RevMix, 18}, {Width, 130}, {Output, 3.1f}, {-1, 0}}},
  {"Pluck/Bright Pluck", {{Osc1Wave, 0}, {Osc2Wave, 0}, {Osc2Level, 50}, {Osc2Fine, 9}, {Cutoff, 1500}, {Resonance, 20}, {FilterEnv, 80}, {FDecay, 220}, {FSustain, 0}, {Attack, 1}, {Decay, 500}, {Sustain, 0}, {Release, 250}, {ChorusMode, 2}, {DelayMix, 30}, {DelaySync, 3}, {RevMix, 22}, {Width, 130}, {Output, 0.5f}, {-1, 0}}},
  {"Pluck/Dub Stab", {{FilterType, 1}, {Osc1Wave, 0}, {Osc2Level, 70}, {Osc2Fine, 8}, {Cutoff, 700}, {Resonance, 35}, {FilterEnv, 70}, {FDecay, 160}, {FSustain, 0}, {Attack, 1}, {Decay, 300}, {Sustain, 0}, {Release, 150}, {DelayMix, 45}, {DelaySync, 3}, {DelayFeedback, 60}, {DelayTone, 45}, {RevMix, 20}, {RevType, 0}, {Output, 1.6f}, {-1, 0}}},
  {"Pluck/Bell Pluck", {{Osc1Level, 0}, {FmLevel, 85}, {FmRatio, 7}, {FmIndex, 38}, {FmDecay, 400}, {FilterType, 1}, {Cutoff, 10000}, {Attack, 1}, {Decay, 1400}, {Sustain, 0}, {Release, 900}, {DelayMix, 28}, {DelaySync, 3}, {RevMix, 32}, {RevType, 1}, {Width, 140}, {Output, -5.4f}, {-1, 0}}},
  {"Arp/Highway Arp", {{Arp, 1}, {ArpRate, 3}, {ArpMode, 0}, {ArpOctaves, 3}, {Gate, 50}, {Osc1Wave, 0}, {Osc2Wave, 0}, {Osc2Level, 50}, {Osc2Fine, 8}, {Cutoff, 1700}, {Resonance, 25}, {FilterEnv, 55}, {FDecay, 230}, {FSustain, 10}, {Attack, 1}, {Decay, 300}, {Sustain, 35}, {Release, 150}, {ChorusMode, 1}, {DelayMix, 30}, {DelaySync, 3}, {RevMix, 20}, {Width, 130}, {Output, -1.4f}, {-1, 0}}},
  {"Arp/Triplet Run", {{Arp, 1}, {ArpRate, 4}, {ArpMode, 2}, {ArpOctaves, 2}, {Gate, 55}, {Osc1Wave, 1}, {Osc1PW, 38}, {Osc2Wave, 0}, {Osc2Level, 40}, {Cutoff, 1500}, {Resonance, 22}, {FilterEnv, 50}, {FDecay, 200}, {FSustain, 10}, {Attack, 1}, {Decay, 250}, {Sustain, 30}, {Release, 120}, {ChorusMode, 2}, {DelayMix, 32}, {DelaySync, 6}, {RevMix, 20}, {Width, 135}, {Output, -1.8f}, {-1, 0}}},
  {"Arp/Slow Cascade", {{Arp, 1}, {ArpRate, 1}, {ArpMode, 0}, {ArpOctaves, 3}, {Gate, 80}, {Osc1Level, 40}, {FmLevel, 70}, {FmRatio, 3}, {FmIndex, 30}, {FmDecay, 800}, {FilterType, 1}, {Cutoff, 9000}, {Attack, 2}, {Decay, 900}, {Sustain, 20}, {Release, 700}, {DelayMix, 35}, {DelaySync, 3}, {DelayFeedback, 50}, {RevMix, 40}, {RevType, 1}, {RevDecay, 6}, {Width, 150}, {Output, -4.0f}, {-1, 0}}},
  {"Arp/Pulse Walker", {{Arp, 1}, {ArpRate, 3}, {ArpMode, 3}, {ArpOctaves, 2}, {Gate, 45}, {Osc1Wave, 1}, {Osc1PW, 30}, {PwmDepth, 30}, {LfoRate, 1.f}, {LfoDelay, 0}, {Osc2Wave, 1}, {Osc2Level, 45}, {Osc2Fine, 8}, {Cutoff, 2000}, {Resonance, 20}, {FilterEnv, 45}, {FDecay, 220}, {FSustain, 8}, {Decay, 280}, {Sustain, 25}, {Release, 130}, {ChorusMode, 1}, {DelayMix, 26}, {DelaySync, 3}, {RevMix, 18}, {Width, 130}, {Output, -4.6f}, {-1, 0}}},
  {"Arp/FM Sparkle", {{Arp, 1}, {ArpRate, 4}, {ArpMode, 2}, {ArpOctaves, 3}, {Gate, 35}, {Osc1Level, 0}, {FmLevel, 85}, {FmRatio, 7}, {FmIndex, 32}, {FmDecay, 250}, {FilterType, 1}, {Cutoff, 11000}, {Attack, 1}, {Decay, 400}, {Sustain, 0}, {Release, 300}, {DelayMix, 32}, {DelaySync, 6}, {RevMix, 35}, {RevType, 1}, {Width, 150}, {Output, -1.6f}, {-1, 0}}},
  {"Arp/Minor Climb", {{Arp, 1}, {ArpRate, 3}, {ArpMode, 0}, {ArpOctaves, 4}, {Gate, 45}, {Osc1Wave, 0}, {Osc2Wave, 1}, {Osc2Level, 55}, {Unison, 2}, {Detune, 18}, {Cutoff, 1300}, {Resonance, 30}, {FilterEnv, 60}, {FDecay, 220}, {FSustain, 10}, {Decay, 300}, {Sustain, 30}, {Release, 140}, {ChorusMode, 1}, {DelayMix, 28}, {DelaySync, 3}, {RevMix, 22}, {Width, 135}, {Output, -1.8f}, {-1, 0}}},
  {"FX/Rewind Swell", {{Osc1Wave, 0}, {Osc2Level, 70}, {Osc2Fine, 18}, {NoiseLevel, 25}, {Unison, 3}, {Detune, 40}, {Cutoff, 250}, {Resonance, 40}, {FilterEnv, 85}, {FAttack, 5000}, {FDecay, 1500}, {FSustain, 100}, {Attack, 4000}, {Sustain, 100}, {Release, 1500}, {ChorusMode, 3}, {DelayMix, 25}, {DelaySync, 3}, {RevMix, 40}, {RevType, 1}, {Width, 170}, {Output, -2.2f}, {-1, 0}}},
  {"FX/Neon Rain", {{Osc1Level, 0}, {FmLevel, 80}, {FmRatio, 9}, {FmIndex, 50}, {FmDecay, 500}, {NoiseLevel, 15}, {FilterType, 1}, {Cutoff, 8000}, {LfoShape, 3}, {LfoRate, 7.f}, {LfoFilter, 35}, {LfoDelay, 0}, {Attack, 400}, {Sustain, 90}, {Release, 2500}, {ChorusMode, 4}, {DelayMix, 30}, {DelaySync, 6}, {RevMix, 50}, {RevType, 1}, {RevDecay, 8}, {Width, 180}, {Output, -12.4f}, {-1, 0}}},
  {"FX/Dark Drone", {{VoiceMode, 1}, {Unison, 5}, {Detune, 45}, {Spread, 100}, {Osc1Wave, 0}, {Osc2Wave, 0}, {Osc2Octave, -1}, {Osc2Level, 80}, {SubLevel, 50}, {Cutoff, 350}, {Resonance, 35}, {LfoFilter, 25}, {LfoRate, .07f}, {LfoDelay, 0}, {Attack, 2500}, {Sustain, 100}, {Release, 4500}, {ChorusMode, 2}, {RevMix, 55}, {RevType, 1}, {RevDecay, 14}, {RevDamp, 65}, {Width, 170}, {Output, -6.3f}, {-1, 0}}},
  {"FX/Laser Arc", {{VoiceMode, 1}, {Osc1Wave, 0}, {Osc2Wave, 0}, {Osc2Sync, 1}, {Osc2Octave, 1}, {Osc2Level, 70}, {Glide, 700}, {Cutoff, 3500}, {Resonance, 40}, {LfoPitch, 40}, {LfoRate, 2.f}, {LfoDelay, 0}, {Attack, 5}, {Release, 700}, {DelayMix, 40}, {DelaySync, 3}, {DelayFeedback, 55}, {RevMix, 30}, {RevType, 1}, {Output, -2.4f}, {-1, 0}}},
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
