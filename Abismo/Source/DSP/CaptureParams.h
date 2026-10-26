#pragma once
// INVERSO CAPTURE: the edit of the captured clip and the mix. IDs are part of saved sessions.
#include "AbismoParams.h"
#include <array>

namespace inverso_abismo::capture {
using inverso_abismo::Kind;
using inverso_abismo::ParamSpec;

namespace P { enum : int { Start, End, Gain, Pitch, Reverse, Loop, FadeIn, FadeOut, Dry, Sample, CapLen, Count }; }

inline const std::array<ParamSpec, P::Count>& specs() {
 static const std::array<ParamSpec, P::Count> table{{
  {"START","Region start",0,100,0,0,.01f,Kind::Float,nullptr,"%"},
  {"END","Region end",0,100,100,0,.01f,Kind::Float,nullptr,"%"},
  {"GAIN","Gain",-24,24,0,0,.01f,Kind::Float,nullptr,"dB"},
  {"PITCH","Pitch (speed follows)",-24,24,0,0,.01f,Kind::Float,nullptr,"st"},
  {"REVERSE","Reverse",0,1,0,0,1,Kind::Bool,nullptr,""},
  {"LOOP","Loop",0,1,0,0,1,Kind::Bool,nullptr,""},
  {"FADE_IN","Fade in",0,2000,5,300,.1f,Kind::Float,nullptr,"ms"},
  {"FADE_OUT","Fade out",0,2000,20,300,.1f,Kind::Float,nullptr,"ms"},
  {"DRY","Track through",0,100,100,0,.1f,Kind::Float,nullptr,"%"},
  {"SAMPLE","Clip level",-60,6,0,-12,.1f,Kind::Float,nullptr,"gate"},
  {"CAPLEN","Capture the last",0,4,2,0,1,Kind::Choice,"5 s|10 s|20 s|30 s|60 s",""},
 }};
 return table;
}
inline double captureSeconds(int choice) { static const double s[5] = {5, 10, 20, 30, 60}; return s[choice < 0 ? 0 : (choice > 4 ? 4 : choice)]; }
}
