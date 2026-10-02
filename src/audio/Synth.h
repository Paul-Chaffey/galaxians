#pragma once

#include <array>
#include <cstdint>
#include <vector>

// Sound effects modelled on the original arcade board's sound circuits (as
// documented by MAME's discrete emulation), generated in code rather than
// sampled. Mono float samples in [-1, 1] at kSampleRate. No platform
// dependencies, so it can be tested and previewed alone.
//
// The board had four kinds of source:
//  - a monophonic tone generator whose pitch the game changed once per video
//    frame (alien hits, dive swoops)
//  - a 555 "fire" oscillator whose pitch falls and wobbles with noise
//  - an LFSR noise source through a low-pass filter (explosions)
//  - three 555 oscillators swept by a stepped LFO (the swarm drone)
namespace audio {

inline constexpr int kSampleRate = 48000;

using Samples = std::vector<float>;

Samples synthPlayerShot();
Samples synthAlienExplosion();
Samples synthFlagshipExplosion();
Samples synthPlayerExplosion();
Samples synthDive();

// The background swarm drone. Its three oscillators never line up into a
// seamless loop, so it is generated live, one sample at a time.
class SwarmDrone {
public:
    float next();

private:
    std::array<double, 3> phase_{};
    uint64_t sample_ = 0;
    float control_ = 0; // smoothed LFO control voltage
    float lowpass_ = 0;
};

} // namespace audio
