#pragma once

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace as3
{

using music_handle = uint64_t;
using sound_handle = uint64_t;

constexpr music_handle invalid_music = 0;
constexpr sound_handle invalid_sound = 0;

// Abstract audio interface for game to use
class IAudio
{
public:
    virtual ~IAudio() = default;
    
    // Music (streaming, one at a time)
    virtual music_handle load_music(const std::filesystem::path& path) = 0;
    virtual void unload_music(music_handle music) = 0;
    virtual void play_music(music_handle music, int loops = -1) = 0;  // -1 = loop forever
    virtual void stop_music() = 0;
    virtual void pause_music() = 0;
    virtual void resume_music() = 0;
    virtual void set_music_volume(float volume) = 0;  // 0.0 - 1.0
    [[nodiscard]] virtual float get_music_volume() const = 0;
    [[nodiscard]] virtual bool is_music_playing() const = 0;
    [[nodiscard]] virtual bool is_music_paused() const = 0;
    [[nodiscard]] virtual music_handle current_music() const = 0;
    
    // Sound effects (multiple can play at once)
    virtual sound_handle load_sound(const std::filesystem::path& path) = 0;
    virtual void unload_sound(sound_handle sound) = 0;
    virtual void play_sound(sound_handle sound, float volume = 1.0f) = 0;
    virtual void set_sound_volume(float volume) = 0;  // Master sound volume
    [[nodiscard]] virtual float get_sound_volume() const = 0;
    
    // List available audio files
    [[nodiscard]] virtual std::vector<std::string> list_music_files() const = 0;
    [[nodiscard]] virtual std::vector<std::string> list_sound_files() const = 0;
};

} // namespace as3

