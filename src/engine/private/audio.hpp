#pragma once

#include <core-api/audio.hpp>

#include <unordered_map>

struct MIX_Mixer;
struct MIX_Audio;
struct MIX_Track;

namespace euengine
{

class audio_manager final : public i_audio
{
public:
    audio_manager() = default;
    ~audio_manager() override;

    audio_manager(const audio_manager&)            = delete;
    audio_manager& operator=(const audio_manager&) = delete;
    audio_manager(audio_manager&&)                 = delete;
    audio_manager& operator=(audio_manager&&)      = delete;

    [[nodiscard]] bool init();

    void shutdown();

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
        return music_volume;
    }
    [[nodiscard]] bool         is_music_playing() const override;
    [[nodiscard]] bool         is_music_paused() const override;
    [[nodiscard]] music_handle current_music() const noexcept override
    {
        return current_playing_music;
    }

    [[nodiscard]] sound_handle load_sound(
        const std::filesystem::path& path) override;
    void                unload_sound(sound_handle sound) override;
    void                play_sound(sound_handle sound, float volume) override;
    void                set_sound_volume(float volume) override;
    [[nodiscard]] float get_sound_volume() const noexcept override
    {
        return sound_volume;
    }

private:
    MIX_Mixer* mixer       = nullptr;
    MIX_Track* music_track = nullptr;

    std::unordered_map<music_handle, MIX_Audio*> music;
    std::unordered_map<sound_handle, MIX_Audio*> sounds;

    std::uint64_t next_music = 1;
    std::uint64_t next_sound = 1;

    music_handle current_playing_music = invalid_music;
    float        music_volume          = 0.7f;
    float        sound_volume          = 1.0f;
    bool         music_paused          = false;
    bool         is_initialized        = false;
};

} // namespace euengine