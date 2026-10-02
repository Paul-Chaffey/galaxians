#include "audio/Synth.h"

#include <algorithm>
#include <cmath>
#include <numbers>

namespace audio {

namespace {

constexpr float kDt = 1.0f / float(kSampleRate);
constexpr int kSamplesPerFrame = kSampleRate / 60; // the game changed pitch once per video frame

// One-pole low-pass coefficient for a cutoff in Hz.
float lowpassCoefficient(float cutoff)
{
    return 1.0f - std::exp(-2.0f * std::numbers::pi_v<float> * cutoff * kDt);
}

// The board's 17-bit LFSR (taps 4 and 16), read at a much lower rate than it
// runs, which gives its noise a gritty, sample-and-hold character.
class LfsrNoise {
public:
    explicit LfsrNoise(float sampleRate)
        : holdSamples_(std::max(1, int(float(kSampleRate) / sampleRate)))
    {
    }

    float next()
    {
        if (counter_++ % holdSamples_ == 0) {
            for (int i = 0; i < 7; ++i) { // advance several steps between reads
                const uint32_t bit = ~((state_ >> 4) ^ (state_ >> 16)) & 1u;
                state_ = ((state_ << 1) | bit) & 0x1FFFFu;
            }
            value_ = (state_ & 1u) ? 1.0f : -1.0f;
        }
        return value_;
    }

private:
    int holdSamples_;
    int counter_ = 0;
    uint32_t state_ = 0;
    float value_ = 0;
};

// The tone generator: SOUND_CLOCK / (256 - pitch), through a 4-bit counter
// whose QA, QC and QD outputs are mixed through 33k, 22k and 5.1k resistors.
// QB is unused, which gives the wave its odd, buzzy staircase shape.
float toneWave(double phase)
{
    constexpr float kWeightA = 1.0f / 33.0f;
    constexpr float kWeightC = 1.0f / 22.0f;
    constexpr float kWeightD = 1.0f / 5.1f;
    constexpr float kTotal = kWeightA + kWeightC + kWeightD;

    const int step = int(phase * 16.0) & 15;
    const float level = (float(step & 1) * kWeightA + float((step >> 2) & 1) * kWeightC +
                         float((step >> 3) & 1) * kWeightD) / kTotal;
    return level * 2.0f - 1.0f;
}

double toneFrequency(int pitch)
{
    constexpr double kSoundClock = 1536000.0;
    return kSoundClock / double(256 - std::clamp(pitch, 0, 255)) / 16.0;
}

// Plays the tone generator with the pitch register stepped once per frame,
// from `startPitch` by `step` for `frames` frames. Volume drops to the board's
// quieter setting for the last `quietFrames`.
Samples toneSweep(int startPitch, int step, int frames, int quietFrames, float gain)
{
    Samples out(size_t(frames * kSamplesPerFrame));
    double phase = 0;
    for (size_t i = 0; i < out.size(); ++i) {
        const int frame = int(i) / kSamplesPerFrame;
        phase = std::fmod(phase + toneFrequency(startPitch + step * frame) / kSampleRate, 1.0);
        const float volume = frame >= frames - quietFrames ? 0.5f : 1.0f;
        // A few milliseconds of fade at each end, so the hard on/off of the
        // real circuit doesn't click through modern speakers.
        const float edge = std::min({1.0f, float(i) / 96.0f, float(out.size() - i) / 240.0f});
        out[i] = toneWave(phase) * volume * edge * gain;
    }
    return out;
}

// The explosion noise: LFSR noise through the board's ~700 Hz low-pass, held
// at full level for `holdSeconds`, then decaying with the circuit's RC.
Samples noiseExplosion(float holdSeconds, float decaySeconds, float gain)
{
    const float total = holdSeconds + decaySeconds * 5.0f;
    Samples out(size_t(total * float(kSampleRate)));
    LfsrNoise noise(3900.0f);
    const float a = lowpassCoefficient(700.0f);
    float filtered = 0;
    for (size_t i = 0; i < out.size(); ++i) {
        const float t = float(i) * kDt;
        filtered += a * (noise.next() - filtered);
        const float envelope = t < holdSeconds ? 1.0f : std::exp(-(t - holdSeconds) / decaySeconds);
        out[i] = filtered * envelope * std::min(1.0f, t / 0.002f) * gain;
    }
    return out;
}

void clip(Samples& samples)
{
    for (float& s : samples)
        s = std::clamp(s, -1.0f, 1.0f);
}

} // namespace

Samples synthPlayerShot()
{
    // The fire 555 (10k, 22k, 0.01uF) runs near 2.7 kHz. Firing pushes its
    // control voltage so the pitch falls, the noise source wobbles it, and the
    // output dies away with the 2.2k / 47uF RC (about 100 ms).
    constexpr float kBase = 2667.0f;
    constexpr float kDuty = 32.0f / 54.0f; // (Ra + Rb) / (Ra + 2 Rb)
    Samples out(size_t(0.4f * float(kSampleRate)));
    LfsrNoise noise(3900.0f);
    const float a = lowpassCoefficient(5000.0f);
    double phase = 0;
    float filtered = 0;
    for (size_t i = 0; i < out.size(); ++i) {
        const float t = float(i) * kDt;
        const float fall = 0.35f + 0.65f * std::exp(-t / 0.06f);
        const float wobble = 1.0f + 0.12f * noise.next();
        phase = std::fmod(phase + double(kBase * fall * wobble) / kSampleRate, 1.0);
        const float pulse = phase < kDuty ? 1.0f : -1.0f;
        filtered += a * (pulse - filtered);
        out[i] = filtered * std::exp(-t / 0.103f) * std::min(1.0f, t / 0.002f) * 0.35f;
    }
    clip(out);
    return out;
}

Samples synthAlienExplosion()
{
    // A quick stepped fall on the tone generator: about 2 kHz down to 500 Hz.
    Samples out = toneSweep(208, -9, 16, 6, 0.3f);
    clip(out);
    return out;
}

Samples synthFlagshipExplosion()
{
    // A longer, deeper fall, so a flagship kill stands out.
    Samples out = toneSweep(224, -6, 30, 10, 0.3f);
    clip(out);
    return out;
}

Samples synthPlayerExplosion()
{
    Samples out = noiseExplosion(0.6f, 0.22f, 0.9f);
    clip(out);
    return out;
}

Samples synthDive()
{
    // The swoop: the tone generator stepped slowly down, about 1.7 kHz to 450 Hz.
    Samples out = toneSweep(200, -2, 80, 20, 0.2f);
    clip(out);
    return out;
}

float SwarmDrone::next()
{
    // Three 555s: 100k with 470k, 330k and 220k on 0.01uF caps.
    constexpr double kBase[3] = {138.0, 189.0, 267.0};
    constexpr double kDuty[3] = {0.55, 0.57, 0.59};
    // The game steps the 4-bit LFO register through all 16 values over and
    // over; each step lowers the oscillators' control voltage, raising pitch.
    constexpr int kSamplesPerLfoStep = kSamplesPerFrame * 4;
    constexpr float kPitchSpan = 0.45f;

    const int lfo = int((sample_ / kSamplesPerLfoStep) % 16);
    ++sample_;
    // The DAC output is smoothed a little by the circuit's capacitance.
    control_ += 0.002f * (float(lfo) / 15.0f - control_);
    const double multiplier = 0.8 + kPitchSpan * double(control_);

    float sum = 0;
    for (int i = 0; i < 3; ++i) {
        phase_[i] = std::fmod(phase_[i] + kBase[i] * multiplier / kSampleRate, 1.0);
        sum += phase_[i] < kDuty[i] ? 1.0f : -1.0f;
    }
    // The output coupling capacitor and mixer take the edge off the pulses.
    static const float kSmoothing = lowpassCoefficient(1800.0f);
    lowpass_ += kSmoothing * (sum / 3.0f - lowpass_);
    return lowpass_ * 0.1f;
}

} // namespace audio
