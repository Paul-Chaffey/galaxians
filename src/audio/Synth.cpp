#include "audio/Synth.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <numbers>

namespace audio {

namespace {

constexpr float kTwoPi = 2.0f * std::numbers::pi_v<float>;
constexpr float kDt = 1.0f / float(kSampleRate);

// Deterministic white noise in [-1, 1].
class Noise {
public:
    float next()
    {
        state_ ^= state_ << 13;
        state_ ^= state_ >> 17;
        state_ ^= state_ << 5;
        return float(state_) / float(UINT32_MAX) * 2.0f - 1.0f;
    }

private:
    uint32_t state_ = 0x1234567u;
};

Samples makeBuffer(float seconds)
{
    return Samples(size_t(seconds * float(kSampleRate)));
}

// Ramps from 0 to 1 over `seconds` to avoid a click at the start.
float attack(float t, float seconds)
{
    return std::min(1.0f, t / seconds);
}

void clip(Samples& samples)
{
    for (float& s : samples)
        s = std::clamp(s, -1.0f, 1.0f);
}

// Noise through a one-pole low-pass whose cutoff and level fall over time.
// `brightness` and `decay` are rates per second.
Samples noiseBurst(float seconds, float baseCutoff, float extraCutoff, float brightness, float decay, float gain)
{
    Samples out = makeBuffer(seconds);
    Noise noise;
    float filtered = 0;
    for (size_t i = 0; i < out.size(); ++i) {
        const float t = float(i) * kDt;
        const float cutoff = baseCutoff + extraCutoff * std::exp(-t * brightness);
        filtered += cutoff * (noise.next() - filtered);
        out[i] = filtered * std::exp(-t * decay) * attack(t, 0.002f) * gain;
    }
    return out;
}

} // namespace

Samples synthPlayerShot()
{
    // A fast falling pulse sweep.
    Samples out = makeBuffer(0.25f);
    float phase = 0;
    for (size_t i = 0; i < out.size(); ++i) {
        const float t = float(i) * kDt;
        const float freq = 300.0f + 1500.0f * std::exp(-t * 18.0f);
        phase = std::fmod(phase + freq * kDt, 1.0f);
        const float pulse = phase < 0.25f ? 1.0f : -1.0f;
        out[i] = pulse * std::exp(-t * 10.0f) * attack(t, 0.002f) * 0.25f;
    }
    clip(out);
    return out;
}

Samples synthAlienExplosion()
{
    Samples out = noiseBurst(0.45f, 0.05f, 0.6f, 12.0f, 7.0f, 0.9f);
    clip(out);
    return out;
}

Samples synthFlagshipExplosion()
{
    // A longer burst with a falling tone on top, so a flagship kill stands out.
    Samples out = noiseBurst(0.8f, 0.05f, 0.6f, 8.0f, 4.0f, 0.8f);
    float phase = 0;
    for (size_t i = 0; i < out.size(); ++i) {
        const float t = float(i) * kDt;
        const float freq = 200.0f + 700.0f * std::exp(-t * 4.0f);
        phase += kTwoPi * freq * kDt;
        out[i] += std::sin(phase) * std::exp(-t * 5.0f) * attack(t, 0.005f) * 0.25f;
    }
    clip(out);
    return out;
}

Samples synthPlayerExplosion()
{
    // A long, dark burst over a low rumble.
    Samples out = noiseBurst(1.6f, 0.02f, 0.3f, 3.0f, 2.2f, 1.0f);
    for (size_t i = 0; i < out.size(); ++i) {
        const float t = float(i) * kDt;
        out[i] += std::sin(kTwoPi * 55.0f * t) * std::exp(-t * 2.0f) * attack(t, 0.01f) * 0.3f;
    }
    clip(out);
    return out;
}

Samples synthDive()
{
    // A falling whistle with a little vibrato.
    constexpr float kSeconds = 1.1f;
    constexpr float kRelease = 0.2f;
    Samples out = makeBuffer(kSeconds);
    float phase = 0;
    for (size_t i = 0; i < out.size(); ++i) {
        const float t = float(i) * kDt;
        const float fall = std::pow(1.0f - t / kSeconds, 1.5f);
        const float freq = (400.0f + 1000.0f * fall) * (1.0f + 0.02f * std::sin(kTwoPi * 7.0f * t));
        phase += kTwoPi * freq * kDt;
        const float release = std::min(1.0f, (kSeconds - t) / kRelease);
        out[i] = std::sin(phase) * attack(t, 0.03f) * release * 0.22f;
    }
    clip(out);
    return out;
}

Samples synthHumLoop()
{
    // A wavering low drone. Over 0.5 s the 140 Hz carrier completes 70 cycles,
    // its 2 Hz pitch wobble 1 and its 4 Hz tremolo 2, so the loop is seamless.
    // The phase is written in closed form for the same reason.
    constexpr float kSeconds = 0.5f;
    Samples out = makeBuffer(kSeconds);
    for (size_t i = 0; i < out.size(); ++i) {
        const float t = float(i) * kDt;
        const float phase = kTwoPi * 140.0f * t + 10.0f * (1.0f - std::cos(kTwoPi * 2.0f * t));
        const float tone = 0.6f * std::sin(phase) + 0.25f * std::sin(2.0f * phase);
        const float tremolo = 0.75f + 0.25f * std::sin(kTwoPi * 4.0f * t);
        out[i] = tone * tremolo * 0.12f;
    }
    clip(out);
    return out;
}

} // namespace audio
