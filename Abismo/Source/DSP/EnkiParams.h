#pragma once
// ENKI: a sound idea generator. Two sources (waves, FM, noise, recordings,
// grains, resonators, reeds) are shaped, moved and placed in a space; the
// generator (EnkiGenerator.h) invents whole patches from these parameters.
// Portable parameter table and factory sounds (no JUCE).
#include "AbismoParams.h"
#include "AbismoResampler.h"
#include <array>
#include <cstddef>

namespace inverso_abismo::enki {

using inverso_abismo::Kind;
using inverso_abismo::ParamSpec;

// Sources can play any of the drum library's 48 recordings or one of the
// user's sample slots (loaded or downloaded files).
constexpr int numLibrarySamples = 48;
constexpr int numUserSlots = 16;
constexpr int numSamples = numLibrarySamples + numUserSlots;
constexpr int numSourceTypes = 7;

namespace P {
enum : int {
 // Source A, then source B (same layout; B = A + SourceStride).
 AType, ALevel, APitch, AFine, AChar, AMotion, ASample,
 BType, BLevel, BPitch, BFine, BChar, BMotion, BSample,
 Ring, // A x B ring modulation
 // Shape.
 FilterMode, Cutoff, Resonance, FilterEnv, Keytrack, Drive, DriveType, Crush,
 // Envelopes.
 Attack, Decay, Sustain, Release, ModAttack, ModDecay, PitchEnv, Hold,
 // Motion.
 LfoRate, LfoShape, LfoSync, LfoPitch, LfoFilter, LfoAmp, LfoPan, LfoChar,
 Gate, GateDiv, GatePattern, Glitch, Reverse,
 // Space.
 EchoMix, EchoSync, EchoTime, EchoFeedback, RevMix, RevSize, RevDecay,
 // Play.
 Octave, Glide, VelSens, BendRange, PressureCC, Width, Output,
 // The generator's settings (saved with the session).
 Category, Mutate, LockA, LockB, LockShape, LockMotion, LockSpace,
 // Added later: the RESAMPLE stage (in place of the bit crush).
 Resample, ResampleSat,
 Count
};
constexpr int SourceStride = BType - AType;
}

inline const char* sourceTypeNames() { return "Wave|FM|Noise|Sample|Grains|Resonator|Reed"; }

// IDs are part of saved sessions: never rename or reorder meaning, only append.
inline const std::array<ParamSpec, P::Count>& specs() {
 static const std::array<ParamSpec, P::Count> table{{
  {"A_TYPE","Source A type",0,numSourceTypes - 1,0,0,1,Kind::Choice,sourceTypeNames(),""},
  {"A_LEVEL","Source A level",0,100,80,0,.01f,Kind::Float,nullptr,"%"},
  {"A_PITCH","Source A pitch",-24,24,0,0,1,Kind::Int,nullptr,"st"},
  {"A_FINE","Source A fine",-50,50,0,0,.01f,Kind::Float,nullptr,"ct"},
  {"A_CHAR","Source A character",0,100,50,0,.01f,Kind::Float,nullptr,"%"},
  {"A_MOTION","Source A motion",0,100,20,0,.01f,Kind::Float,nullptr,"%"},
  {"A_SAMPLE","Source A sample",0,numSamples - 1,0,0,1,Kind::Int,nullptr,"smp"},
  {"B_TYPE","Source B type",0,numSourceTypes - 1,2,0,1,Kind::Choice,sourceTypeNames(),""},
  {"B_LEVEL","Source B level",0,100,0,0,.01f,Kind::Float,nullptr,"%"},
  {"B_PITCH","Source B pitch",-24,24,0,0,1,Kind::Int,nullptr,"st"},
  {"B_FINE","Source B fine",-50,50,0,0,.01f,Kind::Float,nullptr,"ct"},
  {"B_CHAR","Source B character",0,100,50,0,.01f,Kind::Float,nullptr,"%"},
  {"B_MOTION","Source B motion",0,100,20,0,.01f,Kind::Float,nullptr,"%"},
  {"B_SAMPLE","Source B sample",0,numSamples - 1,12,0,1,Kind::Int,nullptr,"smp"},
  {"RING","Ring modulation A x B",0,100,0,0,.01f,Kind::Float,nullptr,"%"},
  {"FILTER_MODE","Filter type",0,3,0,0,1,Kind::Choice,"Low pass|Band pass|High pass|Comb",""},
  {"CUTOFF","Filter cutoff",20,20000,8000,1200,.1f,Kind::Float,nullptr,"Hz"},
  {"RESONANCE","Filter resonance",0,100,15,0,.01f,Kind::Float,nullptr,"%"},
  {"FILTER_ENV","Filter envelope",-100,100,0,0,.01f,Kind::Float,nullptr,"%"},
  {"KEYTRACK","Filter keytrack",0,100,30,0,.01f,Kind::Float,nullptr,"%"},
  {"DRIVE","Drive",0,100,0,0,.01f,Kind::Float,nullptr,"%"},
  {"DRIVE_TYPE","Drive type",0,4,0,0,1,Kind::Choice,"Tape|Fuzz|Fold|Rectify|Shatter",""},
  {"CRUSH","Bit crush",0,100,0,0,.01f,Kind::Float,nullptr,"%"},
  {"ATTACK","Attack",1,10000,10,400,.1f,Kind::Float,nullptr,"ms"},
  {"DECAY","Decay",1,10000,600,500,.1f,Kind::Float,nullptr,"ms"},
  {"SUSTAIN","Sustain",0,100,80,0,.01f,Kind::Float,nullptr,"%"},
  {"RELEASE","Release",5,15000,400,1000,.1f,Kind::Float,nullptr,"ms"},
  {"MOD_ATTACK","Mod envelope attack",1,10000,5,400,.1f,Kind::Float,nullptr,"ms"},
  {"MOD_DECAY","Mod envelope decay",5,15000,800,800,.1f,Kind::Float,nullptr,"ms"},
  {"PITCH_ENV","Pitch envelope",-48,48,0,0,.01f,Kind::Float,nullptr,"st"},
  {"HOLD","One-shot (ignore note off)",0,1,0,0,1,Kind::Bool,nullptr,""},
  {"LFO_RATE","LFO rate",.02f,20,2,2,.001f,Kind::Float,nullptr,"Hz"},
  {"LFO_SHAPE","LFO shape",0,4,0,0,1,Kind::Choice,"Sine|Triangle|Square|Random|Drift",""},
  {"LFO_SYNC","LFO timing",0,6,0,0,1,Kind::Choice,"Free|2 bars|1 bar|1/2|1/4|1/8|1/16",""},
  {"LFO_PITCH","LFO to pitch",0,1200,0,100,.1f,Kind::Float,nullptr,"ct"},
  {"LFO_FILTER","LFO to filter",0,100,0,0,.01f,Kind::Float,nullptr,"%"},
  {"LFO_AMP","LFO to level",0,100,0,0,.01f,Kind::Float,nullptr,"%"},
  {"LFO_PAN","LFO to pan",0,100,0,0,.01f,Kind::Float,nullptr,"%"},
  {"LFO_CHAR","LFO to character",0,100,0,0,.01f,Kind::Float,nullptr,"%"},
  {"GATE","Rhythmic gate",0,100,0,0,.01f,Kind::Float,nullptr,"%"},
  {"GATE_DIV","Gate steps",0,3,1,0,1,Kind::Choice,"1/8|1/16|1/8T|1/32",""},
  {"GATE_PATTERN","Gate pattern",0,255,93,0,1,Kind::Int,nullptr,""},
  {"GLITCH","Glitch",0,100,0,0,.01f,Kind::Float,nullptr,"%"},
  {"REVERSE","Reverse slices",0,100,0,0,.01f,Kind::Float,nullptr,"%"},
  {"ECHO_MIX","Echo",0,100,0,0,.01f,Kind::Float,nullptr,"%"},
  {"ECHO_SYNC","Echo timing",0,6,0,0,1,Kind::Choice,"Free|1/4|1/8|1/8 dotted|1/16|1/4T|1/8T",""},
  {"ECHO_TIME","Echo time",20,1200,380,300,.1f,Kind::Float,nullptr,"ms"},
  {"ECHO_FEEDBACK","Echo repeats",0,95,35,0,.01f,Kind::Float,nullptr,"%"},
  {"REV_MIX","Reverb mix",0,100,25,0,.01f,Kind::Float,nullptr,"%"},
  {"REV_SIZE","Reverb size",0,100,60,0,.01f,Kind::Float,nullptr,"%"},
  {"REV_DECAY","Reverb decay",.3f,20,3,3,.01f,Kind::Float,nullptr,"s"},
  {"OCTAVE","Octave",0,4,2,0,1,Kind::Choice,"-2|-1|0|+1|+2",""},
  {"GLIDE","Glide",0,2000,0,300,.1f,Kind::Float,nullptr,"ms"},
  {"VEL_SENS","Velocity sensitivity",0,100,50,0,.01f,Kind::Float,nullptr,"%"},
  {"BEND_RANGE","Pitch bend range",1,24,2,0,1,Kind::Int,nullptr,"st"},
  {"PRESSURE_CC","Aftertouch from knob (CC)",0,127,0,0,1,Kind::Int,nullptr,"cc"},
  {"WIDTH","Width",0,200,100,0,.01f,Kind::Float,nullptr,"%"},
  {"OUTPUT","Output",-36,6,-6,0,.01f,Kind::Float,nullptr,"dB"},
  {"CATEGORY","Generator category",0,4,0,0,1,Kind::Choice,"Musical|Textures|Hits|Rhythmic|Weird",""},
  {"MUTATE","Mutate amount",0,100,35,0,.01f,Kind::Float,nullptr,"%"},
  {"LOCK_A","Lock source A",0,1,0,0,1,Kind::Bool,nullptr,""},
  {"LOCK_B","Lock source B",0,1,0,0,1,Kind::Bool,nullptr,""},
  {"LOCK_SHAPE","Lock shape",0,1,0,0,1,Kind::Bool,nullptr,""},
  {"LOCK_MOTION","Lock motion",0,1,0,0,1,Kind::Bool,nullptr,""},
  {"LOCK_SPACE","Lock space",0,1,0,0,1,Kind::Bool,nullptr,""},
  {"RESAMPLE","Resample rate",0,7,0,0,1,Kind::Choice,inverso_abismo::Resampler::choices(),""},
  {"RESAMPLE_SAT","Resample saturation",0,100,0,0,.01f,Kind::Float,nullptr,"%"},
 }};
 return table;
}

using Values = std::array<float, P::Count>;

inline Values defaults() {
 Values v{};
 for (std::size_t i = 0; i < v.size(); ++i) v[i] = specs()[i].def;
 return v;
}

// The parameters the generator writes, by section (the lock buttons follow these).
enum class Section { SourceA, SourceB, Shape, Motion, Space, Play, Generator };
inline Section sectionOf(int p) {
 if (p >= P::AType && p <= P::ASample) return Section::SourceA;
 if (p >= P::BType && p <= P::BSample) return Section::SourceB;
 if ((p >= P::Ring && p <= P::Crush) || (p >= P::Attack && p <= P::Hold)) return Section::Shape;
 if (p >= P::LfoRate && p <= P::Reverse) return Section::Motion;
 if (p >= P::EchoMix && p <= P::RevDecay) return Section::Space;
 if (p == P::Resample || p == P::ResampleSat) return Section::Shape;
 if (p >= P::Category) return Section::Generator;
 return Section::Play;
}

} // namespace inverso_abismo::enki
