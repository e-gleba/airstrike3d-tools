#pragma once

#include <cstdint>
#include <filesystem>

namespace euengine
{

using music_handle = std::uint64_t;
using sound_handle = std::uint64_t;

constexpr music_handle invalid_music = 0;
constexpr sound_handle invalid_sound = 0;

struct i_audio
{
    virtual ~i_audio() = default;

    [[nodiscard]] virtual music_handle load_music(
        const std::filesystem::path& path)                        = 0;
    virtual void unload_music(music_handle music)                 = 0;
    virtual void play_music(music_handle music, bool loop = true) = 0;
    virtual void stop_music()                                     = 0;
    virtual void pause_music()                                    = 0;
    virtual void resume_music()                                   = 0;
    virtual void set_music_volume(float volume)                   = 0;
    [[nodiscard]] virtual float        get_music_volume() const   = 0;
    [[nodiscard]] virtual bool         is_music_playing() const   = 0;
    [[nodiscard]] virtual bool         is_music_paused() const    = 0;
    [[nodiscard]] virtual music_handle current_music() const      = 0;

    [[nodiscard]] virtual sound_handle load_sound(
        const std::filesystem::path& path)                           = 0;
    virtual void unload_sound(sound_handle sound)                    = 0;
    virtual void play_sound(sound_handle sound, float volume = 1.0f) = 0;
    virtual void set_sound_volume(float volume)                      = 0;
    [[nodiscard]] virtual float get_sound_volume() const             = 0;
};

} // namespace euengine