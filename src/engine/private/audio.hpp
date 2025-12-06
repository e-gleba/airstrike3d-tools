#pragma once

/// @file audio.hpp
/// @brief Audio manager for music and sound effects using SDL3_mixer

#include <core-api/audio.hpp>

#include <unordered_map>

// Forward declarations for SDL3_mixer
struct MIX_Mixer;
struct MIX_Audio;
struct MIX_Track;

namespace as3
{

/// Audio manager implementing IAudio interface
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

    // IAudio implementation
    [[nodiscard]] music_handle load_music(
        const std::filesystem::path& path) override;
    void                unload_music(music_handle music) override;
    void                play_music(music_handle music, bool loop) override;
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

    [[nodiscard]] sound_handle load_sound(
        const std::filesystem::path& path) override;
    void                unload_sound(sound_handle sound) override;
    void                play_sound(sound_handle sound, float volume) override;
    void                set_sound_volume(float volume) override;
    [[nodiscard]] float get_sound_volume() const noexcept override
    {
        return sound_volume_;
    }

private:
    MIX_Mixer* mixer_       = nullptr;
    MIX_Track* music_track_ = nullptr;

    std::unordered_map<music_handle, MIX_Audio*> music_;
    std::unordered_map<sound_handle, MIX_Audio*> sounds_;

    std::uint64_t next_music_ = 1;
    std::uint64_t next_sound_ = 1;

    music_handle current_music_ = invalid_music;
    float        music_volume_  = 0.7f;
    float        sound_volume_  = 1.0f;
    bool         music_paused_  = false;
    bool         initialized_   = false;
};

} // namespace as3