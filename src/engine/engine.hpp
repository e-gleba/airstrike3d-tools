#pragma once

#include "game_api.hpp"

#include <SDL3/SDL.h>
#include <SDL3/SDL_gpu.h>
#include <entt/entt.hpp>

#include <filesystem>
#include <memory>

namespace as3
{

class ShaderManager;
class CameraSystem;
class ImGuiLayer;
class Renderer;

struct engine_config final
{
    const char* title    = "airstrike3d";
    int         width    = 800;
    int         height   = 600;
    const char* game_lib = nullptr;
};

class engine final
{
public:
    engine();
    ~engine();
    engine(const engine&) = delete;
    engine& operator=(const engine&) = delete;

    bool init(const engine_config& config);
    void shutdown();
    void run();
    void stop() { running_ = false; }

    bool load_game(const std::filesystem::path& path);
    void unload_game();
    bool reload_game();

    [[nodiscard]] entt::registry& registry() { return registry_; }
    [[nodiscard]] SDL_GPUDevice*  device() const { return device_; }
    [[nodiscard]] SDL_Window*     window() const { return window_; }
    [[nodiscard]] ShaderManager*  shaders() const { return shader_manager_.get(); }
    [[nodiscard]] Renderer*       renderer() const { return renderer_.get(); }

    void set_mouse_captured(bool captured);
    [[nodiscard]] bool mouse_captured() const { return mouse_captured_; }

private:
    void process_events();
    void update();
    void render();
    void update_context();

    entt::registry registry_;
    engine_context context_;

    SDL_Window*    window_ = nullptr;
    SDL_GPUDevice* device_ = nullptr;

    std::unique_ptr<ShaderManager> shader_manager_;
    std::unique_ptr<CameraSystem>  camera_system_;
    std::unique_ptr<ImGuiLayer>    imgui_layer_;
    std::unique_ptr<Renderer>      renderer_;

    SDL_SharedObject* game_lib_handle_ = nullptr;
    std::filesystem::path game_lib_path_;
    game_init_fn     game_init_     = nullptr;
    game_shutdown_fn game_shutdown_ = nullptr;
    game_update_fn   game_update_   = nullptr;
    game_render_fn   game_render_   = nullptr;
    game_ui_fn       game_ui_       = nullptr;

    Uint64 last_time_ = 0;
    float  delta_time_ = 0.016f;
    bool   running_ = false;
    bool   mouse_captured_ = false;
    input_state input_;
};

} // namespace as3
