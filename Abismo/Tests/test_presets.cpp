// Renders every factory preset (chord + held note + release). Verifies each is
// audible, finite and below 0 dBFS. With a directory argument it also writes
// one demo WAV per preset there: test_presets <dir>
#include "TestUtil.h"
#include <string>
using namespace test;

int main(int argc, char** argv) {
 const int sr = 48000;
 const std::string dir = argc > 1 ? argv[1] : "";
 for (std::size_t i = 0; i < presets().size(); ++i) {
  Engine e; e.setValues(presetValues(i)); e.prepare(sr);
  Stereo all;
  auto append = [&](const Stereo& s) {
   all.l.insert(all.l.end(), s.l.begin(), s.l.end());
   all.r.insert(all.r.end(), s.r.begin(), s.r.end());
  };
  for (int n : {45, 52, 58}) e.noteOn(n, .85f);
  append(render(e, sr * 4));
  for (int n : {45, 52, 58}) e.noteOff(n);
  append(render(e, sr));
  e.noteOn(57, 1.f); append(render(e, sr * 3)); e.noteOff(57);
  e.noteOn(63, .7f); append(render(e, sr * 2)); e.noteOff(63);
  append(render(e, sr * 5));
  const float p = peak(all), level = rms(all.l, 0, static_cast<std::size_t>(sr * 4));
  std::printf("%-28s peak %.3f  rms %.4f\n", presets()[i].name, p, level);
  verify(finite(all), "preset output not finite");
  verify(p <= 1.f, "preset exceeded 0 dBFS");
  verify(level > 2e-3f, "preset too quiet / silent");
  if (!dir.empty()) {
   std::string name = std::to_string(i) + "_" + presets()[i].name + ".wav";
   for (auto& ch : name) if (ch == ' ') ch = '_';
   writeWav(dir + "/" + name, all, sr);
  }
 }
 std::puts("Abismo preset tests passed");
}
