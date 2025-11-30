#pragma once

#include "game_api.hpp"

#include <SDL3/SDL.h>
#include <SDL3/SDL_gpu.h>
#include <SDL3/SDL_loadso.h>
#include <entt/entt.hpp>

#include <filesystem>
#include <memory>
#include <string_view>

namespace as3
{

class ShaderManager;
class ImGuiLayer;
class Renderer;
class AudioManager;

struct engine_config final
{
    std::string_view      title    = "airstrike3d";
    int                   width    = 800;
    int                   height   = 600;
    std::filesystem::path game_lib = {};
};

struct SDLWindowDeleter
{
    void operator()(SDL_Window* w) const noexcept;
};
struct SDLGPUDeviceDeleter
{
    void operator()(SDL_GPUDevice* d) const noexcept;
};
struct SDLSharedObjectDeleter
{
    void operator()(SDL_SharedObject* o) const noexcept;
};

using SDLWindowPtr    = std::unique_ptr<SDL_Window, SDLWindowDeleter>;
using SDLGPUDevicePtr = std::unique_ptr<SDL_GPUDevice, SDLGPUDeviceDeleter>;
using SDLSharedObjectPtr =
    std::unique_ptr<SDL_SharedObject, SDLSharedObjectDeleter>;

class engine final
{
public:
    engine();
    ~engine();

    engine(const engine&)            = delete;
    engine& operator=(const engine&) = delete;
    engine(engine&&)                 = delete;
    engine& operator=(engine&&)      = delete;

    [[nodiscard]] bool init(const engine_config& config);
    void               shutdown() noexcept;
    void               run();
    void               stop() noexcept { running_ = false; }

    [[nodiscard]] bool load_game(const std::filesystem::path& path);
    void               unload_game() noexcept;
    [[nodiscard]] bool reload_game();

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

    void               set_mouse_captured(bool captured) noexcept;
    [[nodiscard]] bool mouse_captured() const noexcept
    {
        return mouse_captured_;
    }

private:
    void process_events();
    void update();
    void render();
    void update_context() noexcept;
    void cleanup_game_pointers() noexcept;

    entt::registry registry_;
    engine_context context_{};

    SDLWindowPtr    window_;
    SDLGPUDevicePtr device_;
    bool            sdl_initialized_ = false;

    std::unique_ptr<ShaderManager> shader_manager_;
    std::unique_ptr<ImGuiLayer>    imgui_layer_;
    std::unique_ptr<Renderer>      renderer_;
    std::unique_ptr<AudioManager>  audio_;

    SDLSharedObjectPtr    game_lib_;
    std::filesystem::path game_lib_path_;
    game_init_fn          game_init_     = nullptr;
    game_shutdown_fn      game_shutdown_ = nullptr;
    game_update_fn        game_update_   = nullptr;
    game_render_fn        game_render_   = nullptr;
    game_ui_fn            game_ui_       = nullptr;

    Uint64      last_time_      = 0;
    float       delta_time_     = 0.016f;
    bool        running_        = false;
    bool        mouse_captured_ = false;
    input_state input_{};

    static constexpr float k_max_delta_time = 0.25f;
    static constexpr float k_pitch_limit    = 89.0f;
};

} // namespace as3
