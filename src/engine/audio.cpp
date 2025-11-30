#include "audio.hpp"

#include <SDL3_mixer/SDL_mixer.h>
#include <spdlog/spdlog.h>

#include <algorithm>
#include <ranges>

namespace as3
{

AudioManager::~AudioManager() { shutdown(); }

bool AudioManager::init()
{
    if (initialized_) return true;
    
    if (!MIX_Init()) { spdlog::error("MIX_Init: {}", SDL_GetError()); return false; }
    
    SDL_AudioSpec spec{};
    spec.format = SDL_AUDIO_S16;
    spec.freq = 44100;
    spec.channels = 2;
    mixer_ = MIX_CreateMixerDevice(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &spec);
    if (!mixer_) { spdlog::error("MIX_CreateMixerDevice: {}", SDL_GetError()); MIX_Quit(); return false; }
    
    music_track_ = MIX_CreateTrack(mixer_);
    if (!music_track_) {
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
    if (!initialized_) return;
    
    stop_music();
    for (auto& [h, m] : music_map_) if (m) MIX_DestroyAudio(m);
    for (auto& [h, s] : sounds_map_) if (s) MIX_DestroyAudio(s);
    music_map_.clear();
    sounds_map_.clear();
    
    if (music_track_) { MIX_DestroyTrack(music_track_); music_track_ = nullptr; }
    if (mixer_) { MIX_DestroyMixer(mixer_); mixer_ = nullptr; }
    
    MIX_Quit();
    initialized_ = false;
    spdlog::info("=> audio shutdown");
}

music_handle AudioManager::load_music(const std::filesystem::path& path)
{
    if (!initialized_ || !mixer_ || path.empty()) return invalid_music;
    
    // false = stream instead of predecode for faster loading
    auto* audio = MIX_LoadAudio(mixer_, path.c_str(), false);
    if (!audio) { spdlog::error("== music {}: {}", path.string(), SDL_GetError()); return invalid_music; }
    
    music_map_[next_music_handle_] = audio;
    spdlog::info("=> music: {}", path.filename().string());
    return next_music_handle_++;
}

void AudioManager::unload_music(music_handle h)
{
    if (auto it = music_map_.find(h); it != music_map_.end())
    {
        if (current_music_ == h) stop_music();
        MIX_DestroyAudio(it->second);
        music_map_.erase(it);
    }
}

void AudioManager::play_music(music_handle h, int loops)
{
    auto it = music_map_.find(h);
    if (it == music_map_.end() || !music_track_) return;
    
    if (!MIX_SetTrackAudio(music_track_, it->second)) {
        spdlog::error("MIX_SetTrackAudio: {}", SDL_GetError());
        return;
    }
    
    SDL_PropertiesID props = SDL_CreateProperties();
    if (loops != 0) SDL_SetBooleanProperty(props, "loop", true);
    
    if (!MIX_PlayTrack(music_track_, props))
        spdlog::error("MIX_PlayTrack: {}", SDL_GetError());
    
    SDL_DestroyProperties(props);
    current_music_ = h;
    music_paused_ = false;
}

void AudioManager::stop_music()
{
    if (music_track_) MIX_StopTrack(music_track_, 0);
    current_music_ = invalid_music;
    music_paused_ = false;
}

void AudioManager::pause_music()
{
    if (music_track_) { MIX_PauseTrack(music_track_); music_paused_ = true; }
}

void AudioManager::resume_music()
{
    if (music_track_) { MIX_ResumeTrack(music_track_); music_paused_ = false; }
}

void AudioManager::set_music_volume(float volume)
{
    music_volume_ = std::clamp(volume, 0.0f, 1.0f);
    if (music_track_) MIX_SetTrackGain(music_track_, music_volume_);
}

bool AudioManager::is_music_playing() const
{
    return music_track_ && MIX_TrackPlaying(music_track_);
}

bool AudioManager::is_music_paused() const { return music_paused_; }

sound_handle AudioManager::load_sound(const std::filesystem::path& path)
{
    if (!initialized_ || !mixer_ || path.empty()) return invalid_sound;
    
    // true = predecode for low-latency SFX
    auto* audio = MIX_LoadAudio(mixer_, path.c_str(), true);
    if (!audio) { spdlog::error("== sound {}: {}", path.string(), SDL_GetError()); return invalid_sound; }
    
    sounds_map_[next_sound_handle_] = audio;
    spdlog::info("=> sound: {}", path.filename().string());
    return next_sound_handle_++;
}

void AudioManager::unload_sound(sound_handle h)
{
    if (auto it = sounds_map_.find(h); it != sounds_map_.end())
    {
        MIX_DestroyAudio(it->second);
        sounds_map_.erase(it);
    }
}

void AudioManager::play_sound(sound_handle h, [[maybe_unused]] float volume)
{
    auto it = sounds_map_.find(h);
    if (it == sounds_map_.end() || !mixer_) return;
    MIX_PlayAudio(mixer_, it->second);
}

void AudioManager::set_sound_volume(float volume)
{
    sound_volume_ = std::clamp(volume, 0.0f, 1.0f);
}

std::vector<std::string> AudioManager::list_music_files() const
{
    std::vector<std::string> files;
    const std::filesystem::path dir = "assets/music";
    if (!std::filesystem::exists(dir)) return files;
    
    for (const auto& entry : std::filesystem::directory_iterator(dir))
    {
        if (!entry.is_regular_file()) continue;
        auto ext = entry.path().extension().string();
        std::ranges::transform(ext, ext.begin(), ::tolower);
        if (ext == ".ogg" || ext == ".mp3" || ext == ".wav" || ext == ".flac")
            files.push_back(entry.path().filename().string());
    }
    
    std::ranges::sort(files);
    return files;
}

std::vector<std::string> AudioManager::list_sound_files() const
{
    std::vector<std::string> files;
    const std::filesystem::path dir = "assets/sounds";
    if (!std::filesystem::exists(dir)) return files;
    
    for (const auto& entry : std::filesystem::directory_iterator(dir))
    {
        if (!entry.is_regular_file()) continue;
        auto ext = entry.path().extension().string();
        std::ranges::transform(ext, ext.begin(), ::tolower);
        if (ext == ".wav" || ext == ".ogg" || ext == ".mp3")
            files.push_back(entry.path().filename().string());
    }
    
    std::ranges::sort(files);
    return files;
}

} // namespace as3
