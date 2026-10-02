#pragma once

#include <vector>

// Procedural sound effects, generated once at startup. Mono float samples in
// [-1, 1] at kSampleRate. No platform dependencies, so it can be tested alone.
namespace audio {

inline constexpr int kSampleRate = 48000;

using Samples = std::vector<float>;

Samples synthPlayerShot();
Samples synthAlienExplosion();
Samples synthFlagshipExplosion();
Samples synthPlayerExplosion();
Samples synthDive();
// Loops seamlessly: every component completes a whole number of cycles.
Samples synthHumLoop();

} // namespace audio
