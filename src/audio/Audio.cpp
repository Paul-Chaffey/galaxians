#include "audio/Audio.h"

#include <SDL3/SDL_audio.h>
#include <SDL3/SDL_init.h>
#include <SDL3/SDL_log.h>

#include <algorithm>
#include <cmath>

namespace audio {

namespace {

constexpr size_t kMaxVoices = 16;
constexpr int kMaxVoicesPerSound = 3; // for sounds outside the tone generator
constexpr float kMasterGain = 0.7f;
constexpr float kHumFadePerSample = 1.0f / (0.15f * float(kSampleRate)); // 150 ms fade

// Sounds made by the original board's single tone generator, which could only
// play one of them at a time.
bool usesToneGenerator(game::Sound sound)
{
    return sound == game::Sound::AlienExplosion || sound == game::Sound::FlagshipExplosion ||
           sound == game::Sound::Dive;
}

} // namespace

Audio::Audio()
{
    samples_[size_t(game::Sound::PlayerShot)] = synthPlayerShot();
    samples_[size_t(game::Sound::AlienExplosion)] = synthAlienExplosion();
    samples_[size_t(game::Sound::FlagshipExplosion)] = synthFlagshipExplosion();
    samples_[size_t(game::Sound::PlayerExplosion)] = synthPlayerExplosion();
    samples_[size_t(game::Sound::Dive)] = synthDive();
    scratch_.reserve(4096);

    if (!SDL_InitSubSystem(SDL_INIT_AUDIO)) {
        SDL_LogWarn(SDL_LOG_CATEGORY_AUDIO, "No audio: %s", SDL_GetError());
        return;
    }
    const SDL_AudioSpec spec{SDL_AUDIO_F32, 1, kSampleRate};
    stream_ = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &spec, &Audio::mixCallback, this);
    if (!stream_) {
        SDL_LogWarn(SDL_LOG_CATEGORY_AUDIO, "No audio device: %s", SDL_GetError());
        SDL_QuitSubSystem(SDL_INIT_AUDIO);
        return;
    }
    SDL_ResumeAudioStreamDevice(stream_);
}

Audio::~Audio()
{
    if (stream_) {
        SDL_DestroyAudioStream(stream_); // stops the callback before we go away
        SDL_QuitSubSystem(SDL_INIT_AUDIO);
    }
}

void Audio::play(game::Sound sound)
{
    if (!stream_ || muted_)
        return;
    std::lock_guard lock(mutex_);
    pending_.push_back(sound);
}

void Audio::start(game::Sound sound)
{
    if (usesToneGenerator(sound)) {
        // One tone at a time: the newest replaces the current one, except that
        // a dive swoop never cuts off the sound of a hit.
        auto current = std::find_if(voices_.begin(), voices_.end(),
                                    [](const Voice& v) { return usesToneGenerator(v.sound); });
        if (current != voices_.end()) {
            if (sound == game::Sound::Dive && current->sound != game::Sound::Dive)
                return;
            voices_.erase(current);
        }
    } else {
        auto sameSound = [sound](const Voice& v) { return v.sound == sound; };
        if (std::count_if(voices_.begin(), voices_.end(), sameSound) >= kMaxVoicesPerSound)
            voices_.erase(std::find_if(voices_.begin(), voices_.end(), sameSound));
    }
    if (voices_.size() >= kMaxVoices)
        voices_.erase(voices_.begin());
    voices_.push_back({sound});
}

void Audio::mixCallback(void* userdata, SDL_AudioStream* stream, int additionalBytes, int)
{
    auto* self = static_cast<Audio*>(userdata);
    const int frames = additionalBytes / int(sizeof(float));
    if (frames <= 0)
        return;
    self->scratch_.assign(size_t(frames), 0.0f);
    self->mix(self->scratch_.data(), frames);
    SDL_PutAudioStreamData(stream, self->scratch_.data(), frames * int(sizeof(float)));
}

void Audio::mix(float* out, int frames)
{
    std::lock_guard lock(mutex_);

    for (game::Sound sound : pending_)
        start(sound);
    pending_.clear();

    if (muted_) {
        voices_.clear();
        humGain_ = 0;
        return; // `out` is already silent
    }

    for (Voice& voice : voices_) {
        const Samples& samples = samples_[size_t(voice.sound)];
        const size_t count = std::min(size_t(frames), samples.size() - voice.position);
        for (size_t i = 0; i < count; ++i)
            out[i] += samples[voice.position + i];
        voice.position += count;
    }
    std::erase_if(voices_, [this](const Voice& v) { return v.position >= samples_[size_t(v.sound)].size(); });

    // The drone runs continuously and fades rather than cutting, to avoid clicks.
    const float humTarget = humOn_ ? 1.0f : 0.0f;
    for (int i = 0; i < frames; ++i) {
        if (humGain_ < humTarget)
            humGain_ = std::min(humTarget, humGain_ + kHumFadePerSample);
        else if (humGain_ > humTarget)
            humGain_ = std::max(humTarget, humGain_ - kHumFadePerSample);
        const float drone = drone_.next();
        if (humGain_ > 0)
            out[i] += drone * humGain_;
    }

    // Gentle soft clip so stacked explosions saturate instead of wrapping.
    for (int i = 0; i < frames; ++i)
        out[i] = std::tanh(out[i] * kMasterGain);
}

} // namespace audio
