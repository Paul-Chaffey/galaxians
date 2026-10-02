#pragma once

#include "audio/Synth.h"
#include "game/Sound.h"

#include <array>
#include <atomic>
#include <mutex>
#include <vector>

struct SDL_AudioStream;

namespace audio {

// Plays the game's sounds through an SDL3 audio stream, mixing on SDL's audio
// thread. If no audio device can be opened, every call is a silent no-op.
class Audio {
public:
    Audio();
    ~Audio();

    Audio(const Audio&) = delete;
    Audio& operator=(const Audio&) = delete;

    void play(game::Sound sound);
    void setHum(bool on) { humOn_ = on; }
    void setMuted(bool muted) { muted_ = muted; }
    bool muted() const { return muted_; }

private:
    struct Voice {
        game::Sound sound;
        size_t position = 0;
    };

    static void mixCallback(void* userdata, SDL_AudioStream* stream, int additionalBytes, int totalBytes);
    void mix(float* out, int frames);

    std::array<Samples, size_t(game::Sound::Count)> samples_;
    Samples hum_;

    SDL_AudioStream* stream_ = nullptr;
    std::mutex mutex_; // guards voices_ and pending_
    std::vector<Voice> voices_;
    std::vector<game::Sound> pending_;

    // Audio thread only.
    std::vector<float> scratch_;
    size_t humPosition_ = 0;
    float humGain_ = 0;

    std::atomic<bool> humOn_ = false;
    std::atomic<bool> muted_ = false;
};

} // namespace audio
