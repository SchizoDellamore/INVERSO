#pragma once
// INVERSO CAPTURE: the edit of the clip and the mix. IDs are part of saved sessions: only append.
#include "AbismoParams.h"
#include <array>

namespace inverso_abismo::capture {
using inverso_abismo::Kind;
using inverso_abismo::ParamSpec;

namespace P {
enum : int {
 Start, End, Gain, Pitch, Reverse, Loop, FadeIn, FadeOut, Dry, Sample, CapLen,
 Stretch, Mode, Tape, Warp, WarpDir,
 FilterOn, FilterType, FilterFrom, FilterTo, FilterReso,
 DriveOn, DriveType, DriveAmount, DriveMix,
 CrushOn, CrushBits, CrushRate, CrushMix,
 RingOn, RingFreq, RingMix,
 ChopOn, ChopMode, ChopSlice, ChopDepth,
 EchoOn, EchoTime, EchoFeedback, EchoDark, EchoMix,
 ReverbOn, ReverbSize, ReverbDamp, ReverbMix,
 Original,
 Count
};
}

inline const std::array<ParamSpec, P::Count>& specs() {
 static const std::array<ParamSpec, P::Count> table{{
  {"START","Region start",0,100,0,0,.01f,Kind::Float,nullptr,"%"},
  {"END","Region end",0,100,100,0,.01f,Kind::Float,nullptr,"%"},
  {"GAIN","Gain",-24,24,0,0,.01f,Kind::Float,nullptr,"dB"},
  {"PITCH","Pitch",-24,24,0,0,.01f,Kind::Float,nullptr,"st"},
  {"REVERSE","Reverse",0,1,0,0,1,Kind::Bool,nullptr,""},
  {"LOOP","Loop",0,1,0,0,1,Kind::Bool,nullptr,""},
  {"FADE_IN","Fade in",0,2000,5,300,.1f,Kind::Float,nullptr,"ms"},
  {"FADE_OUT","Fade out",0,2000,20,300,.1f,Kind::Float,nullptr,"ms"},
  {"DRY","Track through",0,100,100,0,.1f,Kind::Float,nullptr,"%"},
  {"SAMPLE","Clip level",-60,6,0,-12,.1f,Kind::Float,nullptr,"gate"},
  {"CAPLEN","Capture the last",0,4,2,0,1,Kind::Choice,"5 s|10 s|20 s|30 s|60 s",""},
  {"STRETCH","Stretch (length)",10,1600,100,100,.1f,Kind::Float,nullptr,"%"},
  {"MODE","Stretch mode",0,2,0,0,1,Kind::Choice,"Beats|Tones|Texture",""},
  {"TAPE","Tape (pitch follows speed)",0,1,0,0,1,Kind::Bool,nullptr,""},
  {"WARP","Tape stop",0,100,0,0,.1f,Kind::Float,nullptr,"%"},
  {"WARP_DIR","Tape stop direction",0,1,0,0,1,Kind::Choice,"Stops|Starts",""},
  {"FILTER_ON","Filter",0,1,0,0,1,Kind::Bool,nullptr,""},
  {"FILTER_TYPE","Filter type",0,2,0,0,1,Kind::Choice,"Low-pass|High-pass|Band-pass",""},
  {"FILTER_FROM","Filter at the start",20,20000,20000,1500,1,Kind::Float,nullptr,"Hz"},
  {"FILTER_TO","Filter at the end",20,20000,20000,1500,1,Kind::Float,nullptr,"Hz"},
  {"FILTER_RESO","Resonance",0,100,20,0,.1f,Kind::Float,nullptr,"%"},
  {"DRIVE_ON","Drive",0,1,0,0,1,Kind::Bool,nullptr,""},
  {"DRIVE_TYPE","Drive type",0,2,0,0,1,Kind::Choice,"Tape|Tube|Transformer",""},
  {"DRIVE_AMOUNT","Drive amount",0,100,40,0,.1f,Kind::Float,nullptr,"%"},
  {"DRIVE_MIX","Drive mix",0,100,100,0,.1f,Kind::Float,nullptr,"%"},
  {"CRUSH_ON","Crush",0,1,0,0,1,Kind::Bool,nullptr,""},
  {"CRUSH_BITS","Bits",1,16,8,0,.1f,Kind::Float,nullptr,"bit"},
  {"CRUSH_RATE","Sample rate",500,48000,12000,6000,1,Kind::Float,nullptr,"Hz"},
  {"CRUSH_MIX","Crush mix",0,100,100,0,.1f,Kind::Float,nullptr,"%"},
  {"RING_ON","Ring",0,1,0,0,1,Kind::Bool,nullptr,""},
  {"RING_FREQ","Ring frequency",20,4000,200,300,.1f,Kind::Float,nullptr,"Hz"},
  {"RING_MIX","Ring mix",0,100,50,0,.1f,Kind::Float,nullptr,"%"},
  {"CHOP_ON","Chop",0,1,0,0,1,Kind::Bool,nullptr,""},
  {"CHOP_MODE","Chop mode",0,1,0,0,1,Kind::Choice,"Gate|Stutter",""},
  {"CHOP_SLICE","Slice length",20,1000,125,150,.1f,Kind::Float,nullptr,"ms"},
  {"CHOP_DEPTH","Chop depth",0,100,50,0,.1f,Kind::Float,nullptr,"%"},
  {"ECHO_ON","Echo",0,1,0,0,1,Kind::Bool,nullptr,""},
  {"ECHO_TIME","Echo time",20,1500,300,300,.1f,Kind::Float,nullptr,"ms"},
  {"ECHO_FEEDBACK","Echo feedback",0,95,40,0,.1f,Kind::Float,nullptr,"%"},
  {"ECHO_DARK","Echo darkness",0,100,30,0,.1f,Kind::Float,nullptr,"%"},
  {"ECHO_MIX","Echo mix",0,100,30,0,.1f,Kind::Float,nullptr,"%"},
  {"REVERB_ON","Reverb",0,1,0,0,1,Kind::Bool,nullptr,""},
  {"REVERB_SIZE","Reverb size",0,100,50,0,.1f,Kind::Float,nullptr,"%"},
  {"REVERB_DAMP","Reverb darkness",0,100,40,0,.1f,Kind::Float,nullptr,"%"},
  {"REVERB_MIX","Reverb mix",0,100,30,0,.1f,Kind::Float,nullptr,"%"},
  {"ORIGINAL","Hear the original",0,1,0,0,1,Kind::Bool,nullptr,""},
 }};
 return table;
}
inline double captureSeconds(int choice) { static const double s[5] = {5, 10, 20, 30, 60}; return s[choice < 0 ? 0 : (choice > 4 ? 4 : choice)]; }

// Does this parameter change the edited sound (so the clip has to be rendered again)?
inline bool affectsRender(int p) { return !(p == P::Loop || p == P::Dry || p == P::Sample || p == P::CapLen || p == P::Original); }
}
