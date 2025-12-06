/// @file audio.cpp
/// @brief Audio manager implementation using SDL3_mixer

#include "audio.hpp"

#include <SDL3_mixer/SDL_mixer.h>
#include <spdlog/spdlog.h>

#include <algorithm>

namespace euengine
{

audio_manager::~audio_manager()
{
    shutdown();
}

bool audio_manager::init()
{
    if (is_initialized)
    {
        return true;
    }

    if (!MIX_Init())
    {
        spdlog::error("MIX_Init: {}", SDL_GetError());
        return false;
    }

    SDL_AudioSpec spec {};
    spec.format   = SDL_AUDIO_S16;
    spec.channels = 2;
    spec.freq     = 44100;

    mixer = MIX_CreateMixerDevice(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &spec);
    if (mixer == nullptr)
    {
        spdlog::error("MIX_CreateMixerDevice: {}", SDL_GetError());
        MIX_Quit();
        return false;
    }

    music_track = MIX_CreateTrack(mixer);
    if (music_track == nullptr)
    {
        spdlog::error("MIX_CreateTrack: {}", SDL_GetError());
        MIX_DestroyMixer(mixer);
        mixer = nullptr;
        MIX_Quit();
        return false;
    }

    is_initialized = true;
    set_music_volume(music_volume);
    spdlog::info("=> audio init (44100 Hz, stereo)");
    return true;
}

void audio_manager::shutdown()
{
    if (!is_initialized)
    {
        return;
    }

    stop_music();

    for (auto& [h, audio] : music)
    {
        if (audio != nullptr)
        {
            MIX_DestroyAudio(audio);
        }
    }
    music.clear();

    for (auto& [h, audio] : sounds)
    {
        if (audio != nullptr)
        {
            MIX_DestroyAudio(audio);
        }
    }
    sounds.clear();

    if (music_track != nullptr)
    {
        MIX_DestroyTrack(music_track);
        music_track = nullptr;
    }

    if (mixer != nullptr)
    {
        MIX_DestroyMixer(mixer);
        mixer = nullptr;
    }

    MIX_Quit();
    is_initialized = false;
    spdlog::info("=> audio shutdown");
}

music_handle audio_manager::load_music(const std::filesystem::path& path)
{
    if (!is_initialized || (mixer == nullptr) || path.empty())
    {
        return invalid_music;
    }

    auto* audio = MIX_LoadAudio(mixer, path.c_str(), false); // Stream
    if (audio == nullptr)
    {
        spdlog::error("== music {}: {}", path.string(), SDL_GetError());
        return invalid_music;
    }

    music[next_music] = audio;
    spdlog::info("=> music: {}", path.filename().string());
    return next_music++;
}

void audio_manager::unload_music(music_handle h)
{
    if (auto it = music.find(h); it != music.end())
    {
        if (current_playing_music == h)
        {
            stop_music();
        }
        MIX_DestroyAudio(it->second);
        music.erase(it);
    }
}

void audio_manager::play_music(music_handle h, bool loop)
{
    auto it = music.find(h);
    if (it == music.end() || (music_track == nullptr))
    {
        return;
    }

    if (!MIX_SetTrackAudio(music_track, it->second))
    {
        spdlog::error("MIX_SetTrackAudio: {}", SDL_GetError());
        return;
    }

    SDL_PropertiesID props = SDL_CreateProperties();
    SDL_SetBooleanProperty(props, "loop", loop);

    if (!MIX_PlayTrack(music_track, props))
    {
        spdlog::error("MIX_PlayTrack: {}", SDL_GetError());
    }

    SDL_DestroyProperties(props);
    current_playing_music = h;
    music_paused          = false;
}

void audio_manager::stop_music()
{
    if (music_track != nullptr)
    {
        MIX_StopTrack(music_track, 0);
    }
    current_playing_music = invalid_music;
    music_paused          = false;
}

void audio_manager::pause_music()
{
    if (music_track != nullptr)
    {
        MIX_PauseTrack(music_track);
        music_paused = true;
    }
}

void audio_manager::resume_music()
{
    if (music_track != nullptr)
    {
        MIX_ResumeTrack(music_track);
        music_paused = false;
    }
}

void audio_manager::set_music_volume(float volume)
{
    music_volume = std::clamp(volume, 0.0f, 1.0f);
    if (music_track != nullptr)
    {
        MIX_SetTrackGain(music_track, music_volume);
    }
}

bool audio_manager::is_music_playing() const
{
    return (music_track != nullptr) && MIX_TrackPlaying(music_track);
}

bool audio_manager::is_music_paused() const
{
    return music_paused;
}

sound_handle audio_manager::load_sound(const std::filesystem::path& path)
{
    if (!is_initialized || (mixer == nullptr) || path.empty())
    {
        return invalid_sound;
    }

    auto* audio = MIX_LoadAudio(mixer, path.c_str(), true); // Predecode
    if (audio == nullptr)
    {
        spdlog::error("== sound {}: {}", path.string(), SDL_GetError());
        return invalid_sound;
    }

    sounds[next_sound] = audio;
    spdlog::info("=> sound: {}", path.filename().string());
    return next_sound++;
}

void audio_manager::unload_sound(sound_handle h)
{
    if (auto it = sounds.find(h); it != sounds.end())
    {
        MIX_DestroyAudio(it->second);
        sounds.erase(it);
    }
}

void audio_manager::play_sound(sound_handle h, [[maybe_unused]] float volume)
{
    auto it = sounds.find(h);
    if (it == sounds.end() || (mixer == nullptr))
    {
        return;
    }
    MIX_PlayAudio(mixer, it->second);
}

void audio_manager::set_sound_volume(float volume)
{
    sound_volume = std::clamp(volume, 0.0f, 1.0f);
}

} // namespace euengine