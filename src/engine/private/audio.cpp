/// @file audio.cpp
/// @brief Audio manager implementation using SDL3_mixer

#include "audio.hpp"

#include <SDL3_mixer/SDL_mixer.h>
#include <spdlog/spdlog.h>

#include <algorithm>

namespace euengine
{

AudioManager::~AudioManager()
{
    shutdown();
}

bool AudioManager::init()
{
    if (initialized_)
        return true;

    if (!MIX_Init())
    {
        spdlog::error("MIX_Init: {}", SDL_GetError());
        return false;
    }

    SDL_AudioSpec spec{};
    spec.format   = SDL_AUDIO_S16;
    spec.channels = 2;
    spec.freq     = 44100;

    mixer_ = MIX_CreateMixerDevice(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &spec);
    if (!mixer_)
    {
        spdlog::error("MIX_CreateMixerDevice: {}", SDL_GetError());
        MIX_Quit();
        return false;
    }

    music_track_ = MIX_CreateTrack(mixer_);
    if (!music_track_)
    {
        spdlog::error("MIX_CreateTrack: {}", SDL_GetError());
        MIX_DestroyMixer(mixer_);
        mixer_ = nullptr;
        MIX_Quit();
        return false;
    }

    initialized_ = true;
    set_music_volume(music_volume_);
    spdlog::info("=> audio init (44100 Hz, stereo)");
    return true;
}

void AudioManager::shutdown()
{
    if (!initialized_)
        return;

    stop_music();

    for (auto& [h, audio] : music_)
        if (audio)
            MIX_DestroyAudio(audio);
    music_.clear();

    for (auto& [h, audio] : sounds_)
        if (audio)
            MIX_DestroyAudio(audio);
    sounds_.clear();

    if (music_track_)
    {
        MIX_DestroyTrack(music_track_);
        music_track_ = nullptr;
    }

    if (mixer_)
    {
        MIX_DestroyMixer(mixer_);
        mixer_ = nullptr;
    }

    MIX_Quit();
    initialized_ = false;
    spdlog::info("=> audio shutdown");
}

music_handle AudioManager::load_music(const std::filesystem::path& path)
{
    if (!initialized_ || !mixer_ || path.empty())
        return invalid_music;

    auto* audio = MIX_LoadAudio(mixer_, path.c_str(), false); // Stream
    if (!audio)
    {
        spdlog::error("== music {}: {}", path.string(), SDL_GetError());
        return invalid_music;
    }

    music_[next_music_] = audio;
    spdlog::info("=> music: {}", path.filename().string());
    return next_music_++;
}

void AudioManager::unload_music(music_handle h)
{
    if (auto it = music_.find(h); it != music_.end())
    {
        if (current_music_ == h)
            stop_music();
        MIX_DestroyAudio(it->second);
        music_.erase(it);
    }
}

void AudioManager::play_music(music_handle h, bool loop)
{
    auto it = music_.find(h);
    if (it == music_.end() || !music_track_)
        return;

    if (!MIX_SetTrackAudio(music_track_, it->second))
    {
        spdlog::error("MIX_SetTrackAudio: {}", SDL_GetError());
        return;
    }

    SDL_PropertiesID props = SDL_CreateProperties();
    SDL_SetBooleanProperty(props, "loop", loop);

    if (!MIX_PlayTrack(music_track_, props))
        spdlog::error("MIX_PlayTrack: {}", SDL_GetError());

    SDL_DestroyProperties(props);
    current_music_ = h;
    music_paused_  = false;
}

void AudioManager::stop_music()
{
    if (music_track_)
        MIX_StopTrack(music_track_, 0);
    current_music_ = invalid_music;
    music_paused_  = false;
}

void AudioManager::pause_music()
{
    if (music_track_)
    {
        MIX_PauseTrack(music_track_);
        music_paused_ = true;
    }
}

void AudioManager::resume_music()
{
    if (music_track_)
    {
        MIX_ResumeTrack(music_track_);
        music_paused_ = false;
    }
}

void AudioManager::set_music_volume(float volume)
{
    music_volume_ = std::clamp(volume, 0.0f, 1.0f);
    if (music_track_)
        MIX_SetTrackGain(music_track_, music_volume_);
}

bool AudioManager::is_music_playing() const
{
    return music_track_ && MIX_TrackPlaying(music_track_);
}

bool AudioManager::is_music_paused() const
{
    return music_paused_;
}

sound_handle AudioManager::load_sound(const std::filesystem::path& path)
{
    if (!initialized_ || !mixer_ || path.empty())
        return invalid_sound;

    auto* audio = MIX_LoadAudio(mixer_, path.c_str(), true); // Predecode
    if (!audio)
    {
        spdlog::error("== sound {}: {}", path.string(), SDL_GetError());
        return invalid_sound;
    }

    sounds_[next_sound_] = audio;
    spdlog::info("=> sound: {}", path.filename().string());
    return next_sound_++;
}

void AudioManager::unload_sound(sound_handle h)
{
    if (auto it = sounds_.find(h); it != sounds_.end())
    {
        MIX_DestroyAudio(it->second);
        sounds_.erase(it);
    }
}

void AudioManager::play_sound(sound_handle h, [[maybe_unused]] float volume)
{
    auto it = sounds_.find(h);
    if (it == sounds_.end() || !mixer_)
        return;
    MIX_PlayAudio(mixer_, it->second);
}

void AudioManager::set_sound_volume(float volume)
{
    sound_volume_ = std::clamp(volume, 0.0f, 1.0f);
}

} // namespace euengine
