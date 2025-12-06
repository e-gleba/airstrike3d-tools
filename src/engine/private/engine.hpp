#pragma once

#include <core-api/game.hpp>

#include <core-api/engine.hpp>
#include <core-api/platform.hpp>
#include <core-api/window.hpp>

#include <SDL3/SDL.h>
#include <SDL3/SDL_gpu.h>
#include <SDL3/SDL_loadso.h>
#include <entt/entt.hpp>

#include <filesystem>
#include <memory>
#include <string>
#include <string_view>

namespace euengine
{

// Forward declarations
class ShaderManager;
class ImGuiLayer;
class Renderer;
class audio_manager;

/// RAII deleter for SDL_Window
struct sdl_window_deleter final
{
    void operator()(SDL_Window* w) const noexcept;
};

/// RAII deleter for SDL_GPUDevice
struct sdl_gpu_device_deleter final
{
    void operator()(SDL_GPUDevice* d) const noexcept;
};

/// RAII deleter for SDL_SharedObject (dynamic libraries)
struct sdl_shared_object_deleter final
{
    void operator()(SDL_SharedObject* o) const noexcept;
};

// Smart pointer type aliases
using sdl_window_ptr = std::unique_ptr<SDL_Window, sdl_window_deleter>;
using sdl_gpu_device_ptr =
    std::unique_ptr<SDL_GPUDevice, sdl_gpu_device_deleter>;
using sdl_shared_object_ptr =
    std::unique_ptr<SDL_SharedObject, sdl_shared_object_deleter>;

/// Main engine class - manages window, GPU, subsystems and game loop
/// Also implements i_engine_settings for game access to runtime settings
/// Designed for SDL3 callback architecture
class engine final : public i_engine_settings
{
public:
    engine();
    ~engine() override;

    // Non-copyable, non-movable
    engine(const engine&)            = delete;
    engine& operator=(const engine&) = delete;
    engine(engine&&)                 = delete;
    engine& operator=(engine&&)      = delete;

    /// Initialize all engine subsystems with preinit settings
    [[nodiscard]] bool init(const preinit_settings& settings);

    /// Shutdown all subsystems (also called by destructor)
    void shutdown() noexcept;

    /// Process a single SDL event (for callback architecture)
    /// Returns false if quit was requested
    [[nodiscard]] bool process_event(const SDL_Event& event);

    /// Run single iteration of game loop (for callback architecture)
    void iterate();

    /// Signal the engine to stop
    void stop() noexcept override { running_ = false; }

    /// Check if engine should continue running
    [[nodiscard]] bool is_running() const noexcept { return running_; }

    /// Hot-load a game library
    [[nodiscard]] bool load_game(const std::filesystem::path& path);

    /// Unload current game library
    void unload_game() noexcept;

    /// Reload game library from same path (for hot-reload)
    [[nodiscard]] bool reload_game() noexcept override;

    // Direct accessors (for engine internal use)
    [[nodiscard]] entt::registry& registry() noexcept { return registry_; }
    [[nodiscard]] SDL_GPUDevice*  device() const noexcept
    {
        return device_.get();
    }
    [[nodiscard]] SDL_Window* window() const noexcept { return window_.get(); }
    [[nodiscard]] ShaderManager* shaders() const noexcept
    {
        return shader_manager_.get();
    }
    [[nodiscard]] Renderer* renderer() const noexcept
    {
        return renderer_.get();
    }

    // i_engine_settings implementation
    void                     set_vsync(vsync_mode mode) noexcept override;
    [[nodiscard]] vsync_mode get_vsync() const noexcept override
    {
        return vsync_dirty_ ? pending_vsync_ : current_vsync_;
    }
    void               set_fullscreen(bool fullscreen) noexcept override;
    [[nodiscard]] bool is_fullscreen() const noexcept override;
    [[nodiscard]] std::int32_t     get_window_width() const noexcept override;
    [[nodiscard]] std::int32_t     get_window_height() const noexcept override;
    [[nodiscard]] std::string_view get_gpu_driver() const noexcept override;
    [[nodiscard]] float            get_target_fps() const noexcept override
    {
        return target_fps_;
    }
    void               set_target_fps(float fps) noexcept override;
    void               set_mouse_captured(bool captured) noexcept override;
    [[nodiscard]] bool is_mouse_captured() const noexcept override
    {
        return mouse_captured_;
    }

    // Audio control
    void                set_master_volume(float volume) noexcept override;
    [[nodiscard]] float get_master_volume() const noexcept override;
    [[nodiscard]] bool  is_audio_available() const noexcept override
    {
        return audio_ != nullptr;
    }

    void request_quit() noexcept override { running_ = false; }

private:
    void update();
    void render();
    void update_context() noexcept;
    void cleanup_game_pointers() noexcept;
    void apply_vsync_mode() noexcept;

    // ECS registry shared with game
    entt::registry registry_;
    engine_context context_ {};

    // SDL resources
    sdl_window_ptr     window_;
    sdl_gpu_device_ptr device_;
    bool               sdl_initialized_ = false;
    std::string        gpu_driver_name_;

    // Subsystems
    std::unique_ptr<ShaderManager> shader_manager_;
    std::unique_ptr<ImGuiLayer>    imgui_layer_;
    std::unique_ptr<Renderer>      renderer_;
    std::unique_ptr<audio_manager> audio_;

    // Game library hot-loading
    sdl_shared_object_ptr game_lib_;
    std::filesystem::path game_lib_path_;
    std::filesystem::path game_temp_path_; // Temp copy for hot-reload
    game_preinit_fn       game_preinit_  = nullptr;
    game_init_fn          game_init_     = nullptr;
    game_shutdown_fn      game_shutdown_ = nullptr;
    game_update_fn        game_update_   = nullptr;
    game_render_fn        game_render_   = nullptr;
    game_ui_fn            game_ui_       = nullptr;

    // Frame timing
    Uint64   last_time_    = 0;
    Uint64   start_time_   = 0;
    float    delta_time_   = 0.016f;
    float    elapsed_time_ = 0.0f;
    float    target_fps_   = 0.0f; // 0 = unlimited/vsync controlled
    float    smoothed_fps_ = 60.0f;
    uint64_t frame_count_  = 0;

    // State
    bool        running_        = false;
    bool        mouse_captured_ = false;
    vsync_mode  current_vsync_  = vsync_mode::enabled;
    vsync_mode  pending_vsync_  = vsync_mode::enabled;
    bool        vsync_dirty_    = false;
    input_state input_ {};
    clear_color background_ = clear_color::dark();

    // Audio
    float master_volume_     = 1.0f;
    float music_volume_mult_ = 0.7f;
    float sfx_volume_mult_   = 1.0f;

    // Constants
    static constexpr float k_max_delta_time = 0.25f;
    static constexpr float k_pitch_limit    = 89.0f;
};

} // namespace euengine
