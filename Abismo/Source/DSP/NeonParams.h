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
  at(Unison, {"UNISON","Unison voices",1,8,1,0,1,Kind::Int,nullptr,""});
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
  at(ArpOctaves, {"ARP_OCTAVES","Arpeggio octaves",1,4,1,0,1,Kind::Int,nullptr,""});
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
inline const std::array<Preset, 40>& presets() {
 using namespace P;
 static const std::array<Preset, 40> table{{
  // ---- BASS
  {"Bass/Moroder Pluck", {{VoiceMode, 1}, {Osc1Wave, 0}, {SubLevel, 45}, {Cutoff, 380}, {Resonance, 35}, {FilterEnv, 55}, {FAttack, 1}, {FDecay, 170},
                          {FSustain, 8}, {Attack, 1}, {Decay, 260}, {Sustain, 55}, {Release, 60}, {Keytrack, 30}, {RevMix, 0}, {Output, 4.6f}, {-1, 0}}},
  {"Bass/Octave Runner", {{Arp, 1}, {ArpRate, 3}, {ArpMode, 0}, {ArpOctaves, 2}, {Gate, 50}, {VoiceMode, 1}, {Osc1Wave, 1}, {Osc1PW, 42},
                          {Osc2Level, 45}, {Osc2Octave, -1}, {Cutoff, 520}, {Resonance, 28}, {FilterEnv, 45}, {FDecay, 200}, {FSustain, 12},
                          {Attack, 1}, {Decay, 300}, {Sustain, 50}, {Release, 50}, {RevMix, 0}, {Output, 1.4f}, {-1, 0}}},
  {"Bass/Outrun Sub", {{VoiceMode, 2}, {Glide, 25}, {Osc1Wave, 2}, {SubLevel, 70}, {Osc1Level, 60}, {Cutoff, 900}, {FilterEnv, 15}, {FDecay, 300},
                       {Attack, 2}, {Release, 120}, {RevMix, 0}, {Output, -1.3f}, {-1, 0}}},
  {"Bass/Juno Bass", {{VoiceMode, 1}, {Osc1Wave, 1}, {Osc1PW, 35}, {SubLevel, 55}, {Cutoff, 650}, {Resonance, 20}, {FilterEnv, 35}, {FDecay, 350},
                      {FSustain, 20}, {Attack, 1}, {Decay, 500}, {Sustain, 70}, {Release, 90}, {ChorusMode, 1}, {ChorusMix, 40}, {RevMix, 0}, {Output, 0.0f}, {-1, 0}}},
  {"Bass/FM Slap", {{VoiceMode, 1}, {Osc1Level, 25}, {FmLevel, 85}, {FmRatio, 1}, {FmIndex, 50}, {FmDecay, 180}, {FmVelocity, 90}, {Cutoff, 2600},
                    {FilterEnv, 25}, {FDecay, 300}, {FSustain, 20}, {Attack, 1}, {Decay, 700}, {Sustain, 30}, {Release, 110}, {RevMix, 0}, {Output, 5.0f}, {-1, 0}}},
  {"Bass/Reese Night", {{VoiceMode, 1}, {Unison, 3}, {Detune, 45}, {Spread, 25}, {Osc1Wave, 0}, {Osc2Level, 70}, {Osc2Fine, -12}, {Cutoff, 850},
                        {Resonance, 15}, {FilterEnv, 10}, {LfoFilter, 12}, {LfoRate, .3f}, {LfoDelay, 0}, {Attack, 5}, {Release, 180}, {RevMix, 0}, {Output, -0.1f}, {-1, 0}}},
  // ---- POLYSYNTH
  {"Polysynth/Juno Poly", {{Osc1Wave, 0}, {SubLevel, 25}, {Cutoff, 2200}, {Resonance, 12}, {FilterEnv, 25}, {FDecay, 700}, {FSustain, 40},
                           {Attack, 8}, {Decay, 800}, {Sustain, 75}, {Release, 500}, {ChorusMode, 1}, {RevMix, 18}, {RevType, 1}, {Output, -4.2f}, {-1, 0}}},
  {"Polysynth/Prophet Poly", {{Osc1Wave, 0}, {Osc2Wave, 1}, {Osc2Level, 60}, {Osc2Fine, -7}, {Cutoff, 1600}, {Resonance, 20}, {FilterEnv, 40},
                              {FDecay, 600}, {FSustain, 35}, {Attack, 6}, {Decay, 900}, {Sustain, 70}, {Release, 450}, {RevMix, 15}, {RevType, 0}, {Output, -6.1f}, {-1, 0}}},
  {"Polysynth/OB Stab", {{FilterType, 1}, {Osc1Wave, 0}, {Osc2Level, 80}, {Osc2Fine, 10}, {Unison, 2}, {Detune, 25}, {Cutoff, 900}, {FilterEnv, 70},
                         {FAttack, 1}, {FDecay, 260}, {FSustain, 10}, {Attack, 1}, {Decay, 420}, {Sustain, 0}, {Release, 260},
                         {DelayMix, 18}, {DelaySync, 3}, {RevMix, 15}, {Output, 0.3f}, {-1, 0}}},
  {"Polysynth/Brass 84", {{FilterType, 1}, {Osc1Wave, 0}, {Osc2Wave, 0}, {Osc2Level, 80}, {Osc2Fine, 8}, {Unison, 2}, {Detune, 20}, {Cutoff, 700},
                          {Resonance, 15}, {FilterEnv, 65}, {FAttack, 60}, {FDecay, 600}, {FSustain, 45}, {Attack, 25}, {Release, 350},
                          {ChorusMode, 1}, {RevMix, 18}, {RevType, 0}, {Output, -8.4f}, {-1, 0}}},
  {"Polysynth/Wide Saws", {{Osc1Wave, 0}, {Osc2Wave, 0}, {Osc2Level, 60}, {Osc2Fine, 12}, {Unison, 3}, {Detune, 35}, {Spread, 80}, {Cutoff, 3200},
                           {FilterEnv, 15}, {Attack, 15}, {Release, 600}, {ChorusMode, 1}, {RevMix, 20}, {RevType, 1}, {Output, -3.6f}, {-1, 0}}},
  {"Polysynth/Chord Memory", {{Osc1Wave, 1}, {Osc1PW, 30}, {Osc2Level, 55}, {Osc2Fine, 8}, {Cutoff, 1300}, {Resonance, 25}, {FilterEnv, 45},
                              {FDecay, 220}, {FSustain, 12}, {Attack, 1}, {Decay, 380}, {Sustain, 25}, {Release, 200},
                              {ChorusMode, 2}, {DelayMix, 20}, {DelaySync, 2}, {RevMix, 15}, {Output, -4.5f}, {-1, 0}}},
  // ---- LEAD
  {"Lead/Sync Hero", {{VoiceMode, 2}, {Glide, 60}, {Osc1Wave, 0}, {Osc2Wave, 0}, {Osc2Sync, 1}, {Osc2Octave, 1}, {Osc2Semi, 5}, {Osc2Level, 80},
                      {Osc1Level, 30}, {Cutoff, 4500}, {Resonance, 10}, {FilterEnv, 25}, {LfoPitch, 12}, {LfoDelay, 500},
                      {DelayMix, 28}, {DelaySync, 3}, {RevMix, 18}, {Output, 2.6f}, {-1, 0}}},
  {"Lead/Saw Anthem", {{VoiceMode, 2}, {Glide, 40}, {Unison, 4}, {Detune, 30}, {Osc1Wave, 0}, {Osc2Wave, 0}, {Osc2Level, 70}, {Osc2Octave, -1},
                       {Cutoff, 3500}, {FilterEnv, 30}, {LfoPitch, 15}, {LfoDelay, 400}, {ChorusMode, 1}, {DelayMix, 28}, {DelaySync, 3},
                       {RevMix, 22}, {RevType, 1}, {Output, 2.2f}, {-1, 0}}},
  {"Lead/Square Night", {{VoiceMode, 1}, {Glide, 30}, {Osc1Wave, 1}, {Osc1PW, 50}, {PwmDepth, 20}, {LfoRate, 4}, {Cutoff, 2800}, {Resonance, 20},
                         {FilterEnv, 20}, {LfoPitch, 10}, {LfoDelay, 600}, {DelayMix, 25}, {DelaySync, 4}, {RevMix, 15}, {Output, -0.8f}, {-1, 0}}},
  {"Lead/Glide Whistle", {{VoiceMode, 2}, {Glide, 140}, {Osc1Wave, 3}, {Osc2Wave, 2}, {Osc2Level, 35}, {Osc2Octave, 1}, {FilterType, 1}, {Cutoff, 6000},
                          {LfoPitch, 14}, {LfoRate, 5.5f}, {LfoDelay, 350}, {Attack, 20}, {Release, 250}, {DelayMix, 30}, {DelaySync, 3},
                          {RevMix, 25}, {RevType, 1}, {Output, -0.5f}, {-1, 0}}},
  {"Lead/FM Solo", {{VoiceMode, 2}, {Glide, 35}, {Osc1Level, 20}, {FmLevel, 80}, {FmRatio, 2}, {FmIndex, 45}, {FmDecay, 2500}, {FilterType, 1},
                    {Cutoff, 7000}, {LfoPitch, 10}, {LfoDelay, 500}, {Attack, 5}, {Release, 200}, {DelayMix, 25}, {DelaySync, 3}, {RevMix, 18}, {Output, -2.4f}, {-1, 0}}},
  {"Lead/Resonant Bite", {{VoiceMode, 1}, {Osc1Wave, 0}, {Osc2Level, 50}, {Osc2Fine, 9}, {Cutoff, 900}, {Resonance, 60}, {FilterEnv, 55},
                          {FDecay, 400}, {FSustain, 30}, {FilterDrive, 35}, {Attack, 2}, {Release, 150}, {DelayMix, 20}, {DelaySync, 3}, {RevMix, 12}, {Output, 3.4f}, {-1, 0}}},
  {"Lead/Mono Saw", {{VoiceMode, 1}, {Glide, 20}, {Osc1Wave, 0}, {Osc2Wave, 0}, {Osc2Level, 60}, {Osc2Fine, 6}, {Cutoff, 2400}, {Resonance, 18},
                     {FilterEnv, 35}, {FDecay, 450}, {FSustain, 40}, {Attack, 3}, {Release, 160}, {LfoPitch, 10}, {LfoDelay, 450},
                     {DelayMix, 22}, {DelaySync, 3}, {RevMix, 15}, {Output, 1.9f}, {-1, 0}}},
  // ---- PAD
  {"Pad/Juno Pad", {{Osc1Wave, 0}, {Osc1Level, 70}, {SubLevel, 30}, {Cutoff, 1800}, {Resonance, 10}, {FilterEnv, 20}, {Attack, 600}, {Decay, 1500},
                    {Sustain, 85}, {Release, 2200}, {FAttack, 900}, {FDecay, 2000}, {FSustain, 50}, {ChorusMode, 2}, {RevMix, 30}, {RevType, 1},
                    {RevDecay, 5}, {Output, -4.6f}, {-1, 0}}},
  {"Pad/Night Drive", {{Osc1Wave, 1}, {Osc1PW, 40}, {PwmDepth, 55}, {LfoRate, .6f}, {LfoDelay, 0}, {Osc2Wave, 0}, {Osc2Level, 55}, {Osc2Fine, 9},
                       {Cutoff, 1400}, {Resonance, 20}, {FilterEnv, 15}, {Attack, 900}, {Release, 3000}, {Sustain, 90}, {ChorusMode, 3},
                       {DelayMix, 15}, {DelaySync, 3}, {RevMix, 35}, {RevType, 1}, {RevDecay, 7}, {Output, -14.4f}, {-1, 0}}},
  {"Pad/Neon Strings", {{Osc1Wave, 0}, {Osc2Wave, 0}, {Osc2Level, 70}, {Osc2Fine, 12}, {Unison, 2}, {Detune, 30}, {Cutoff, 3200}, {Resonance, 5},
                        {FilterEnv, 10}, {Attack, 450}, {Release, 1800}, {Sustain, 90}, {LfoPitch, 8}, {LfoRate, 5.5f}, {LfoDelay, 800},
                        {ChorusMode, 1}, {RevMix, 30}, {RevType, 1}, {RevDecay, 4}, {Output, -6.5f}, {-1, 0}}},
  {"Pad/Glass Choir", {{Osc1Wave, 3}, {Osc1Level, 40}, {FmLevel, 60}, {FmRatio, 3}, {FmIndex, 25}, {FmDecay, 4000}, {FilterType, 1}, {Cutoff, 6000},
                       {Attack, 1200}, {Release, 3500}, {Sustain, 100}, {ChorusMode, 4}, {RevMix, 45}, {RevType, 1}, {RevDecay, 9}, {Output, -12.3f}, {-1, 0}}},
  {"Pad/Sunset Sweep", {{Osc1Wave, 0}, {Osc2Level, 70}, {Osc2Fine, 14}, {Unison, 2}, {Detune, 30}, {Cutoff, 300}, {Resonance, 50}, {FilterEnv, 60},
                        {FAttack, 4000}, {FDecay, 4000}, {FSustain, 60}, {Attack, 800}, {Release, 3000}, {LfoFilter, 10}, {LfoRate, .2f},
                        {ChorusMode, 3}, {RevMix, 40}, {RevType, 1}, {RevDecay, 8}, {Output, -4.4f}, {-1, 0}}},
  {"Pad/Warm Analog", {{Osc1Wave, 0}, {Osc2Wave, 2}, {Osc2Level, 50}, {Osc2Octave, -1}, {Cutoff, 1100}, {Resonance, 8}, {FilterEnv, 10}, {Drift, 50},
                       {Attack, 700}, {Release, 2500}, {Sustain, 95}, {ChorusMode, 1}, {RevMix, 30}, {RevType, 1}, {RevDecay, 5}, {Output, -6.6f}, {-1, 0}}},
  // ---- KEYS
  {"Keys/FM Electric Piano", {{Osc1Level, 0}, {FmLevel, 90}, {FmRatio, 1}, {FmIndex, 45}, {FmDecay, 900}, {FmVelocity, 80}, {FilterType, 1},
                              {Cutoff, 9000}, {FilterEnv, 0}, {Attack, 2}, {Decay, 2600}, {Sustain, 25}, {Release, 600}, {ChorusMode, 1}, {ChorusMix, 60},
                              {RevMix, 20}, {RevType, 0}, {Output, -8.1f}, {-1, 0}}},
  {"Keys/Crystal Bells", {{Osc1Level, 0}, {FmLevel, 90}, {FmRatio, 8}, {FmIndex, 55}, {FmDecay, 1500}, {FilterType, 1}, {Cutoff, 12000},
                          {Attack, 1}, {Decay, 3500}, {Sustain, 0}, {Release, 2500}, {DelayMix, 22}, {DelaySync, 3}, {RevMix, 35}, {RevType, 1},
                          {RevDecay, 6}, {Output, -8.5f}, {-1, 0}}},
  {"Keys/DX Marimba", {{Osc1Level, 0}, {FmLevel, 90}, {FmRatio, 4}, {FmIndex, 35}, {FmDecay, 120}, {FmVelocity, 70}, {FilterType, 1}, {Cutoff, 9000},
                       {Attack, 1}, {Decay, 700}, {Sustain, 0}, {Release, 300}, {RevMix, 18}, {RevType, 0}, {Output, -4.0f}, {-1, 0}}},
  {"Keys/Poly Organ", {{Osc1Wave, 3}, {Osc2Wave, 3}, {Osc2Level, 60}, {Osc2Octave, 1}, {SubLevel, 40}, {FilterType, 1}, {Cutoff, 5000}, {Attack, 3},
                      {Release, 120}, {ChorusMode, 3}, {RevMix, 18}, {RevType, 1}, {Output, -11.0f}, {-1, 0}}},
  // ---- PLUCK
  {"Pluck/Poly Pluck", {{Osc1Wave, 0}, {Osc2Wave, 1}, {Osc2Level, 50}, {Osc2Fine, 6}, {Cutoff, 500}, {Resonance, 25}, {FilterEnv, 75}, {FAttack, 1},
                        {FDecay, 260}, {FSustain, 0}, {Attack, 1}, {Decay, 600}, {Sustain, 0}, {Release, 300}, {ChorusMode, 1}, {DelayMix, 28},
                        {DelaySync, 3}, {RevMix, 18}, {Output, 3.9f}, {-1, 0}}},
  {"Pluck/Glass Pluck", {{Osc1Level, 30}, {Osc1Wave, 2}, {FmLevel, 70}, {FmRatio, 3}, {FmIndex, 40}, {FmDecay, 250}, {FilterType, 1}, {Cutoff, 8000},
                         {Attack, 1}, {Decay, 500}, {Sustain, 0}, {Release, 250}, {DelayMix, 30}, {DelaySync, 6}, {RevMix, 22}, {Output, -1.6f}, {-1, 0}}},
  {"Pluck/Muted Pulse", {{Osc1Wave, 1}, {Osc1PW, 25}, {Cutoff, 700}, {Resonance, 15}, {FilterEnv, 50}, {FDecay, 140}, {FSustain, 0}, {Attack, 1},
                         {Decay, 220}, {Sustain, 0}, {Release, 120}, {ChorusMode, 1}, {DelayMix, 20}, {DelaySync, 4}, {RevMix, 12}, {Output, -0.7f}, {-1, 0}}},
  // ---- ARP
  {"Arp/Outrun Arp", {{Arp, 1}, {ArpRate, 3}, {ArpMode, 0}, {ArpOctaves, 2}, {Gate, 45}, {Osc1Wave, 0}, {Osc2Level, 50}, {Osc2Fine, 7}, {Cutoff, 1200},
                      {Resonance, 30}, {FilterEnv, 50}, {FDecay, 250}, {FSustain, 10}, {Attack, 1}, {Decay, 300}, {Sustain, 40}, {Release, 150},
                      {ChorusMode, 1}, {DelayMix, 28}, {DelaySync, 3}, {RevMix, 18}, {Output, 5.5f}, {-1, 0}}},
  {"Arp/Up-Down Glass", {{Arp, 1}, {ArpRate, 4}, {ArpMode, 2}, {ArpOctaves, 3}, {Gate, 40}, {Osc1Level, 20}, {FmLevel, 80}, {FmRatio, 3}, {FmIndex, 35},
                         {FmDecay, 300}, {FilterType, 1}, {Cutoff, 8000}, {Attack, 1}, {Decay, 400}, {Sustain, 0}, {Release, 300}, {DelayMix, 30},
                         {DelaySync, 6}, {RevMix, 30}, {RevType, 1}, {Output, -0.7f}, {-1, 0}}},
  {"Arp/Chord Pulse", {{Arp, 1}, {ArpRate, 3}, {ArpMode, 5}, {Gate, 30}, {Osc1Wave, 1}, {Osc1PW, 30}, {Osc2Level, 60}, {Osc2Fine, 8}, {Cutoff, 1500},
                       {Resonance, 20}, {FilterEnv, 40}, {FDecay, 200}, {FSustain, 10}, {Attack, 1}, {Decay, 250}, {Sustain, 30}, {Release, 120},
                       {ChorusMode, 2}, {DelayMix, 18}, {DelaySync, 2}, {RevMix, 18}, {Output, -6.8f}, {-1, 0}}},
  {"Arp/Triplet Rain", {{Arp, 1}, {ArpRate, 4}, {ArpMode, 3}, {ArpOctaves, 2}, {Gate, 35}, {Osc1Wave, 2}, {Osc2Wave, 3}, {Osc2Level, 50}, {Osc2Octave, 1},
                        {FilterType, 1}, {Cutoff, 4000}, {Attack, 1}, {Decay, 300}, {Sustain, 0}, {Release, 200}, {DelayMix, 30}, {DelaySync, 6},
                        {RevMix, 30}, {RevType, 1}, {Output, 1.2f}, {-1, 0}}},
  {"Arp/Sequence 1984", {{Arp, 1}, {ArpRate, 3}, {ArpMode, 4}, {ArpOctaves, 1}, {Gate, 55}, {VoiceMode, 1}, {Osc1Wave, 0}, {SubLevel, 30},
                         {Cutoff, 600}, {Resonance, 40}, {FilterEnv, 50}, {FDecay, 180}, {FSustain, 10}, {Attack, 1}, {Decay, 250}, {Sustain, 50},
                         {Release, 60}, {DelayMix, 15}, {DelaySync, 2}, {RevMix, 10}, {Output, 5.3f}, {-1, 0}}},
  // ---- FX
  {"FX/VHS Memory", {{Osc1Wave, 2}, {Osc2Wave, 3}, {Osc2Level, 50}, {Osc2Octave, 1}, {Drift, 70}, {LfoPitch, 6}, {LfoRate, .4f}, {LfoDelay, 0},
                     {Cutoff, 2200}, {FilterType, 1}, {Attack, 300}, {Release, 2000}, {ChorusMode, 2}, {DelayMix, 25}, {DelaySync, 3},
                     {RevMix, 35}, {RevType, 1}, {Output, -7.7f}, {-1, 0}}},
  {"FX/Night Wind", {{Osc1Level, 0}, {NoiseLevel, 80}, {FilterType, 2}, {Cutoff, 900}, {Resonance, 70}, {LfoFilter, 40}, {LfoRate, .15f}, {LfoDelay, 0},
                     {Attack, 1500}, {Release, 3000}, {ChorusMode, 4}, {RevMix, 45}, {RevType, 1}, {RevDecay, 8}, {Output, 6.0f}, {-1, 0}}},
  {"FX/Rise", {{Osc1Wave, 0}, {Osc2Level, 60}, {Osc2Fine, 15}, {NoiseLevel, 20}, {Unison, 2}, {Cutoff, 200}, {Resonance, 45}, {FilterEnv, 80},
               {FAttack, 6000}, {FDecay, 2000}, {FSustain, 100}, {Attack, 3000}, {Release, 1500}, {LfoPitch, 0}, {ChorusMode, 1}, {DelayMix, 20},
               {DelaySync, 3}, {RevMix, 35}, {RevType, 1}, {Output, 0.5f}, {-1, 0}}},
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
