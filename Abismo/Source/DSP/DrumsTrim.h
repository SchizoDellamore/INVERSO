#pragma once
// Level of each object (recording or physical model) so that all of them hit at about the same loudness
// at their default settings. Measured by Tools/measure_drum_levels (K-weighted momentary maximum of one
// hit at velocity .8, aimed at -20 LUFS, peak kept under -3 dBFS); regenerate it when the library changes.
#include <array>
// (64 objects, 16 pads, 11 kits: DrumsParams.h checks it.)
namespace inverso_abismo::drums {
inline float objectTrimDb(int object) {
 static const float trim[64] = {
  1.61f, -4.86f, 6.82f, 6.22f, 0.70f, 6.63f, 2.07f, 2.39f,
  1.23f, 2.86f, -4.56f, 8.24f, -4.89f, 3.19f, 7.91f, -2.68f,
  5.92f, -1.09f, 3.26f, -2.59f, -4.03f, 3.07f, 3.75f, 4.39f,
  5.46f, 2.97f, 5.18f, 4.38f, 11.26f, -4.11f, -2.80f, -2.55f,
  -2.63f, 4.37f, -1.71f, 4.32f, 0.85f, -1.46f, -3.95f, 4.19f,
  8.30f, 4.85f, 2.78f, 3.96f, 0.12f, 0.42f, -3.91f, -0.47f,
  4.50f, 14.21f, 6.19f, 9.75f, 8.64f, 10.06f, 12.75f, 10.21f,
  12.14f, 6.62f, 5.31f, 8.94f, 11.41f, 7.78f, 5.01f, 8.77f};
 return trim[object < 0 ? 0 : (object >= 64 ? 63 : object)];
}

// The same, for each pad of each kit (its decay, tone, rattle and character change the level): added to the pad's LEVEL.
inline const std::array<std::array<float, 16>, 11>& kitPadTrimDb() {
 static const std::array<std::array<float, 16>, 11> trim{{
  {2.39f, -1.24f, -0.59f, 1.77f, -1.01f, 0.01f, -0.99f, -0.95f, -1.37f, -0.91f, -1.17f, -0.75f, -1.27f, -0.97f, -0.81f, -1.33f},
  {1.45f, -0.34f, 0.44f, 3.12f, 0.41f, 1.25f, 0.62f, 0.50f, 1.13f, 1.00f, 0.54f, 0.33f, 0.84f, 0.68f, 0.91f, -0.19f},
  {6.00f, 0.00f, 1.74f, 3.56f, 0.41f, 2.74f, 1.57f, 2.03f, 1.76f, 2.71f, 1.27f, 2.27f, -0.46f, 0.24f, 1.45f, 2.00f},
  {-0.22f, 0.17f, -0.21f, 1.99f, -0.62f, 0.20f, -1.59f, -5.07f, -1.61f, -1.19f, -0.89f, 0.96f, -0.79f, -0.23f, -0.53f, -1.10f},
  {3.02f, -0.51f, 0.22f, 1.03f, -0.64f, -0.11f, -0.05f, -0.49f, 1.26f, -0.33f, -0.51f, -0.56f, 1.03f, 2.19f, -0.61f, 1.89f},
  {-3.65f, -1.87f, 1.02f, -2.14f, -1.60f, 2.74f, 2.35f, -4.12f, -1.81f, -1.25f, -1.78f, -1.40f, -1.48f, -1.62f, -1.45f, -1.94f},
  {6.00f, -13.25f, 2.02f, -5.91f, -10.69f, 6.00f, 0.38f, 0.30f, 6.00f, 6.00f, -0.87f, -7.61f, -9.87f, -11.83f, 3.60f, -8.69f},
  {2.85f, -0.85f, -0.97f, -0.60f, -1.30f, -2.31f, -1.09f, -1.97f, -1.01f, -0.90f, -1.15f, -0.46f, -1.22f, -1.11f, -1.22f, -0.96f},
  {-0.21f, 0.57f, -0.17f, 0.39f, 0.75f, -0.16f, 0.08f, -0.29f, 0.01f, 0.91f, -0.02f, 0.42f, 0.79f, 0.34f, -0.31f, -0.47f},
  {-0.54f, 0.94f, 0.44f, 0.39f, 3.77f, -0.39f, 0.89f, 0.17f, -1.13f, 0.96f, 0.51f, -0.32f, -0.16f, 0.38f, 0.45f, 0.03f},
  {-0.48f, 0.05f, -5.69f, -5.77f, -5.79f, 3.23f, -1.69f, -5.23f, -3.37f, -3.21f, -7.59f, -4.41f, 1.18f, -10.76f, -1.20f, -2.11f}}};
 return trim;
}
}
