#pragma once
// INVERSO CAPTURE, the part with no JUCE in it: a ring that keeps the last seconds of what passes through
// the track, the clip taken from it, and one way of turning a clip into sound (region, reverse, varispeed
// pitch, fades, gain, loop) used both live (audition) and offline (bake, export), so what you hear is what
// you get in the file.
#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace inverso_abismo::capture {

constexpr double maxClipSeconds = 60.0;

struct Clip {
 std::vector<float> l, r;
 double sampleRate = 48000.0;
 std::size_t frames() const { return l.size(); }
 bool empty() const { return l.empty(); }
};

// Written by the audio thread, read by the message thread: the head is published after the samples, and the
// ring is longer than the longest clip plus the time a copy takes, so what is being read is not being overwritten.
class Ring {
public:
 void prepare(double sampleRate) {
  rate = sampleRate;
  size = static_cast<std::size_t>(sampleRate * (maxClipSeconds + 2.0));
  l.assign(size, 0.f); r.assign(size, 0.f);
  head.store(0, std::memory_order_relaxed);
 }
 void write(const float* inL, const float* inR, int n) {
  if (size == 0) return;
  std::uint64_t h = head.load(std::memory_order_relaxed);
  for (int i = 0; i < n; ++i) {
   const std::size_t at = static_cast<std::size_t>(h % size);
   l[at] = inL[i]; r[at] = inR ? inR[i] : inL[i];
   ++h;
  }
  head.store(h, std::memory_order_release);
 }
 std::uint64_t position() const { return head.load(std::memory_order_acquire); }
 // The last `frames` frames up to now (fewer if less was written), or from `from` (a position() taken earlier).
 Clip last(std::size_t frames) const { return between(position() - std::min<std::uint64_t>(position(), frames), position()); }
 Clip between(std::uint64_t from, std::uint64_t to) const {
  Clip c; c.sampleRate = rate;
  if (size == 0 || to <= from) return c;
  const auto maxFrames = static_cast<std::uint64_t>(rate * maxClipSeconds);
  if (to - from > maxFrames) from = to - maxFrames;
  const auto written = head.load(std::memory_order_acquire);
  if (written > size && from < written - size) from = written - size;   // what was overwritten is gone
  const auto n = static_cast<std::size_t>(to - from);
  c.l.resize(n); c.r.resize(n);
  for (std::size_t i = 0; i < n; ++i) { const std::size_t at = static_cast<std::size_t>((from + i) % size); c.l[i] = l[at]; c.r[i] = r[at]; }
  return c;
 }
 double sampleRate() const { return rate; }
private:
 std::vector<float> l, r;
 std::size_t size = 0;
 double rate = 48000.0;
 std::atomic<std::uint64_t> head{0};
};

struct Edit {
 double start = 0.0, end = 1.0;     // the region, 0..1 of the clip
 float gainDb = 0.f, pitchSt = 0.f;
 bool reverse = false, loop = false;
 float fadeInMs = 5.f, fadeOutMs = 20.f;
};

inline double cubic(const std::vector<float>& x, double pos) {
 const auto n = static_cast<long>(x.size());
 const long i = static_cast<long>(std::floor(pos));
 const double t = pos - static_cast<double>(i);
 auto at = [&](long k) { return static_cast<double>(x[static_cast<std::size_t>(std::clamp(k, 0L, n - 1))]); };
 const double p0 = at(i - 1), p1 = at(i), p2 = at(i + 1), p3 = at(i + 2);
 return p1 + .5 * t * (p2 - p0 + t * (2.0 * p0 - 5.0 * p1 + 4.0 * p2 - p3 + t * (3.0 * (p1 - p2) + p3 - p0)));
}

// One pass (or a loop) over the region of a clip at the host's rate. Adds into the output.
class Player {
public:
 void play(const Clip* c, const Edit& e, double hostRate) {
  clip = c; edit = e; host = hostRate; playing = c != nullptr && !c->empty(); frames = c ? static_cast<double>(c->frames()) : 0.0;
  played = 0; stopping = false; stopGain = 1.0;
  begin();
 }
 void stop() { if (playing) stopping = true; }
 void setEdit(const Edit& e) { edit = e; }   // live: knobs move while it plays
 void setClip(const Clip* c) { clip = c; frames = c ? static_cast<double>(c->frames()) : 0.0; if (!c || c->empty()) playing = false; }
 // Another clip under the same playhead (the same fraction of the way through it): the edit changed while it plays.
 void swapClip(const Clip* c) {
  if (!c || c->empty()) { clip = nullptr; playing = false; return; }
  const double frac = position();   // (no look at the old clip: it may be gone)
  clip = c; frames = static_cast<double>(c->frames()); pos = frac * frames;
 }
 bool active() const { return playing; }
 // 0..1 within the clip, for the playhead
 double position() const { return frames > 0.0 ? std::clamp(pos / frames, 0.0, 1.0) : 0.0; }
 // True when the pass reached its end (and was not looping).
 bool render(float* outL, float* outR, int n) {
  if (!playing || !clip) return false;
  const double total = static_cast<double>(clip->frames());
  const double s = std::clamp(edit.start, 0.0, 1.0) * total, e = std::clamp(edit.end, 0.0, 1.0) * total;
  const double len = std::max(2.0, e - s);
  const double ratio = clip->sampleRate / host * std::exp2(static_cast<double>(edit.pitchSt) / 12.0);
  const double outLen = len / ratio;   // output samples in a pass
  const double gain = std::pow(10.0, static_cast<double>(edit.gainDb) / 20.0);
  const double fin = std::max(1.0, edit.fadeInMs * .001 * host), fout = std::max(1.0, edit.fadeOutMs * .001 * host);
  for (int i = 0; i < n; ++i) {
   if (!playing) return true;
   const double rel = edit.reverse ? (e - pos) : (pos - s);   // source frames into the region, in play direction
   if (rel < 0.0 || rel >= len) {   // the end of the pass
    if (edit.loop && !stopping) { begin(); played = 0; --i; continue; }   // this slot gets the first sample of the next pass
    playing = false; return true;
   }
   const double t = rel / ratio;   // output samples into the pass
   double g = gain * std::min({1.0, (t + 1.0) / fin, (outLen - t) / fout});
   if (stopping) { stopGain -= 1.0 / std::max(1.0, .005 * host); if (stopGain <= 0.0) { playing = false; return false; } g *= stopGain; }
   const double sl = cubic(clip->l, pos), sr = cubic(clip->r, pos);
   outL[i] += static_cast<float>(sl * g); outR[i] += static_cast<float>(sr * g);
   pos += edit.reverse ? -ratio : ratio;
   ++played;
  }
  return false;
 }
private:
 void begin() {
  if (!clip) return;
  const double total = static_cast<double>(clip->frames());
  const double s = std::clamp(edit.start, 0.0, 1.0) * total, e = std::clamp(edit.end, 0.0, 1.0) * total;
  pos = edit.reverse ? std::max(s, e - 1.0) : s;
 }
 const Clip* clip = nullptr;
 Edit edit;
 double host = 48000.0, pos = 0.0, stopGain = 1.0, frames = 0.0;
 std::uint64_t played = 0;
 bool playing = false, stopping = false;
};

// The edit applied to the clip: what BAKE keeps and what the file holds. Same sound as the audition.
inline Clip renderEdit(const Clip& in, Edit e) {
 Clip out; out.sampleRate = in.sampleRate;
 if (in.empty()) return out;
 e.loop = false;
 const double ratio = std::exp2(static_cast<double>(e.pitchSt) / 12.0);
 const double len = std::max(2.0, (std::clamp(e.end, 0.0, 1.0) - std::clamp(e.start, 0.0, 1.0)) * static_cast<double>(in.frames()));
 const auto n = static_cast<std::size_t>(std::ceil(len / ratio));
 out.l.assign(n, 0.f); out.r.assign(n, 0.f);
 Player p; p.play(&in, e, in.sampleRate);
 int done = 0; const int total = static_cast<int>(n);
 while (done < total) {
  const int block = std::min(4096, total - done);
  if (p.render(out.l.data() + done, out.r.data() + done, block)) break;
  done += block;
 }
 return out;
}

inline double peakOf(const Clip& c) {
 double p = 0;
 for (float v : c.l) p = std::max(p, static_cast<double>(std::abs(v)));
 for (float v : c.r) p = std::max(p, static_cast<double>(std::abs(v)));
 return p;
}
// Raise or lower the whole clip so its highest sample sits at targetDb.
inline void normalize(Clip& c, double targetDb) {
 const double p = peakOf(c);
 if (p < 1e-9) return;
 const float g = static_cast<float>(std::pow(10.0, targetDb / 20.0) / p);
 for (auto& v : c.l) v *= g;
 for (auto& v : c.r) v *= g;
}

} // namespace inverso_abismo::capture
