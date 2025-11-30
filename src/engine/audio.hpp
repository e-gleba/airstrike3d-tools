#pragma once

/// @file audio.hpp
/// @brief Audio manager for music and sound effects

#include "../shared/audio_interface.hpp"

#include <filesystem>
#include <string>
#include <unordered_map>
#include <vector>

// Forward declarations for SDL3_mixer
struct MIX_Mixer;
struct MIX_Audio;
struct MIX_Track;

namespace as3
{

/// Audio manager implementing IAudio interface
/// Handles music streaming and sound effect playback using SDL3_mixer
class AudioManager final : public IAudio
{
public:
    AudioManager() = default;
    ~AudioManager() override;

    // Non-copyable, non-movable
    AudioManager(const AudioManager&)            = delete;
    AudioManager& operator=(const AudioManager&) = delete;
    AudioManager(AudioManager&&)                 = delete;
    AudioManager& operator=(AudioManager&&)      = delete;

    /// Initialize audio subsystem
    [[nodiscard]] bool init();

    /// Shutdown and release all resources
    void shutdown();

    // Music operations
    music_handle        load_music(const std::filesystem::path& path) override;
    void                unload_music(music_handle music) override;
    void                play_music(music_handle music, int loops = -1) override;
    void                stop_music() override;
    void                pause_music() override;
    void                resume_music() override;
    void                set_music_volume(float volume) override;
    [[nodiscard]] float get_music_volume() const noexcept override
    {
        return music_volume_;
    }
    [[nodiscard]] bool         is_music_playing() const override;
    [[nodiscard]] bool         is_music_paused() const override;
    [[nodiscard]] music_handle current_music() const noexcept override
    {
        return current_music_;
    }

    // Sound operations
    sound_handle load_sound(const std::filesystem::path& path) override;
    void         unload_sound(sound_handle sound) override;
    void         play_sound(sound_handle sound, float volume = 1.0f) override;
    void         set_sound_volume(float volume) override;
    [[nodiscard]] float get_sound_volume() const noexcept override
    {
        return sound_volume_;
    }

    // File listing
    [[nodiscard]] std::vector<std::string> list_music_files() const override;
    [[nodiscard]] std::vector<std::string> list_sound_files() const override;

private:
    MIX_Mixer* mixer_       = nullptr;
    MIX_Track* music_track_ = nullptr;

    std::unordered_map<music_handle, MIX_Audio*> music_map_;
    std::unordered_map<sound_handle, MIX_Audio*> sounds_map_;

    std::uint64_t next_music_handle_ = 1;
    std::uint64_t next_sound_handle_ = 1;

    music_handle current_music_ = invalid_music;
    float        music_volume_  = 0.7f;
    float        sound_volume_  = 1.0f;
    bool         music_paused_  = false;
    bool         initialized_   = false;
};

} // namespace as3
