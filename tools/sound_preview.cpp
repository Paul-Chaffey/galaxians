// Writes every synthesised sound to a WAV file for listening:
//   sound_preview <output directory>
#include "audio/Synth.h"

#include <cstdint>
#include <cstdio>
#include <fstream>
#include <string>

namespace {

void writeWav(const std::string& path, const audio::Samples& samples)
{
    std::ofstream file(path, std::ios::binary);
    auto u32 = [&](uint32_t v) { file.write(reinterpret_cast<const char*>(&v), 4); };
    auto u16 = [&](uint16_t v) { file.write(reinterpret_cast<const char*>(&v), 2); };

    const uint32_t dataBytes = uint32_t(samples.size() * 2);
    file.write("RIFF", 4);
    u32(36 + dataBytes);
    file.write("WAVEfmt ", 8);
    u32(16);
    u16(1); // PCM
    u16(1); // mono
    u32(audio::kSampleRate);
    u32(audio::kSampleRate * 2);
    u16(2);
    u16(16);
    file.write("data", 4);
    u32(dataBytes);
    for (float s : samples)
        u16(uint16_t(int16_t(s * 32767.0f)));
    std::printf("wrote %s\n", path.c_str());
}

} // namespace

int main(int argc, char** argv)
{
    const std::string dir = argc > 1 ? argv[1] : ".";
    writeWav(dir + "/1-player-shot.wav", audio::synthPlayerShot());
    writeWav(dir + "/2-alien-hit.wav", audio::synthAlienExplosion());
    writeWav(dir + "/3-flagship-hit.wav", audio::synthFlagshipExplosion());
    writeWav(dir + "/4-dive.wav", audio::synthDive());
    writeWav(dir + "/5-player-explosion.wav", audio::synthPlayerExplosion());

    audio::SwarmDrone drone;
    audio::Samples background(size_t(audio::kSampleRate) * 4);
    for (float& s : background)
        s = drone.next() * 3.0f; // louder than in game, to hear it clearly
    writeWav(dir + "/6-background.wav", background);
    return 0;
}
