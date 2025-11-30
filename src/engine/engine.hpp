#pragma once

/// @file engine.hpp
/// @brief Main engine class with SDL3 GPU backend

#include "game_api.hpp"

#include "../shared/engine_settings_interface.hpp"
#include "../shared/platform.hpp"
#include "../shared/window_settings.hpp"

#include <SDL3/SDL.h>
#include <SDL3/SDL_gpu.h>
#include <SDL3/SDL_loadso.h>
#include <entt/entt.hpp>

#include <filesystem>
#include <memory>
#include <string>
#include <string_view>

namespace as3
{

// Forward declarations
class ShaderManager;
class ImGuiLayer;
class Renderer;
class AudioManager;

/// Engine configuration combining window and game settings
struct engine_config final
{
    window_settings       window   = {};
    std::filesystem::path game_lib = {};
};

/// RAII deleter for SDL_Window
struct SDLWindowDeleter final
{
    void operator()(SDL_Window* w) const noexcept;
};

/// RAII deleter for SDL_GPUDevice
struct SDLGPUDeviceDeleter final
{
    void operator()(SDL_GPUDevice* d) const noexcept;
};

/// RAII deleter for SDL_SharedObject (dynamic libraries)
struct SDLSharedObjectDeleter final
{
    void operator()(SDL_SharedObject* o) const noexcept;
};

// Smart pointer type aliases
using SDLWindowPtr    = std::unique_ptr<SDL_Window, SDLWindowDeleter>;
using SDLGPUDevicePtr = std::unique_ptr<SDL_GPUDevice, SDLGPUDeviceDeleter>;
using SDLSharedObjectPtr =
    std::unique_ptr<SDL_SharedObject, SDLSharedObjectDeleter>;

/// Main engine class - manages window, GPU, subsystems and game loop
/// Also implements IEngineSettings for game access to runtime settings
class engine final : public IEngineSettings
{
public:
    engine();
    ~engine() override;

    // Non-copyable, non-movable
    engine(const engine&)            = delete;
    engine& operator=(const engine&) = delete;
    engine(engine&&)                 = delete;
    engine& operator=(engine&&)      = delete;

    /// Initialize all engine subsystems
    [[nodiscard]] bool init(const engine_config& config);

    /// Shutdown all subsystems (also called by destructor)
    void shutdown() noexcept;

    /// Run the main game loop
    void run();

    /// Signal the engine to stop
    void stop() noexcept { running_ = false; }

    /// Hot-load a game library
    [[nodiscard]] bool load_game(const std::filesystem::path& path);

    /// Unload current game library
    void unload_game() noexcept;

    /// Reload game library from same path (for hot-reload)
    [[nodiscard]] bool reload_game();

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
    [[nodiscard]] bool is_running() const noexcept { return running_; }

    // IEngineSettings implementation
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
    void request_quit() noexcept override { running_ = false; }

private:
    void process_events();
    void update();
    void render();
    void update_context() noexcept;
    void cleanup_game_pointers() noexcept;
    void apply_vsync_mode() noexcept;

    // ECS registry shared with game
    entt::registry registry_;
    engine_context context_{};

    // SDL resources
    SDLWindowPtr    window_;
    SDLGPUDevicePtr device_;
    bool            sdl_initialized_ = false;
    std::string     gpu_driver_name_;

    // Subsystems
    std::unique_ptr<ShaderManager> shader_manager_;
    std::unique_ptr<ImGuiLayer>    imgui_layer_;
    std::unique_ptr<Renderer>      renderer_;
    std::unique_ptr<AudioManager>  audio_;

    // Game library hot-loading
    SDLSharedObjectPtr    game_lib_;
    std::filesystem::path game_lib_path_;
    std::filesystem::path game_temp_path_; // Temp copy for hot-reload
    game_init_fn          game_init_     = nullptr;
    game_shutdown_fn      game_shutdown_ = nullptr;
    game_update_fn        game_update_   = nullptr;
    game_render_fn        game_render_   = nullptr;
    game_ui_fn            game_ui_       = nullptr;

    // Frame timing
    Uint64 last_time_  = 0;
    float  delta_time_ = 0.016f;
    float  target_fps_ = 0.0f; // 0 = unlimited/vsync controlled

    // State
    bool        running_        = false;
    bool        mouse_captured_ = false;
    vsync_mode  current_vsync_  = vsync_mode::enabled;
    vsync_mode  pending_vsync_  = vsync_mode::enabled;
    bool        vsync_dirty_    = false;
    input_state input_{};

    // Constants
    static constexpr float k_max_delta_time = 0.25f;
    static constexpr float k_pitch_limit    = 89.0f;
};

} // namespace as3
