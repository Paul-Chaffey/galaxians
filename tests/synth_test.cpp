// Checks the procedurally generated sound effects.
#include "audio/Synth.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>

namespace {

int failures = 0;

void check(bool ok, const char* what)
{
    std::printf("%s: %s\n", ok ? "PASS" : "FAIL", what);
    if (!ok)
        ++failures;
}

float peak(const audio::Samples& s)
{
    float p = 0;
    for (float v : s)
        p = std::max(p, std::abs(v));
    return p;
}

// Largest jump between neighbouring samples; a click shows up as a big jump.
float largestStep(const audio::Samples& s)
{
    float step = 0;
    for (size_t i = 1; i < s.size(); ++i)
        step = std::max(step, std::abs(s[i] - s[i - 1]));
    return step;
}

void checkEffect(const char* name, const audio::Samples& s, float minSeconds, float maxSeconds)
{
    const float seconds = float(s.size()) / float(audio::kSampleRate);
    const float p = peak(s);
    std::printf("      %-18s %.2f s, peak %.2f\n", name, seconds, p);
    check(seconds >= minSeconds && seconds <= maxSeconds, "effect has the intended length");
    check(p > 0.05f && p <= 1.0f, "effect is audible and within [-1, 1]");
    check(std::abs(s.front()) < 0.01f && std::abs(s.back()) < 0.05f, "effect starts and ends near silence");
}

} // namespace

int main()
{
    checkEffect("player shot", audio::synthPlayerShot(), 0.1f, 0.5f);
    checkEffect("alien explosion", audio::synthAlienExplosion(), 0.2f, 1.0f);
    checkEffect("flagship explosion", audio::synthFlagshipExplosion(), 0.4f, 1.5f);
    checkEffect("player explosion", audio::synthPlayerExplosion(), 1.0f, 2.5f);
    checkEffect("dive", audio::synthDive(), 0.5f, 2.0f);

    const audio::Samples hum = audio::synthHumLoop();
    const float seam = std::abs(hum.front() - hum.back());
    check(peak(hum) > 0.02f && peak(hum) <= 1.0f, "hum is audible and within [-1, 1]");
    check(seam <= largestStep(hum) * 1.5f, "hum loops without a click at the seam");

    return failures == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
