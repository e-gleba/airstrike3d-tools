#pragma once

#include <SDL3/SDL.h>
#include <SDL3/SDL_gpu.h>

#include <entt/entt.hpp>

#include <memory>

namespace as3
{

class ShaderManager;
class CameraSystem;
class ImGuiLayer;
class Renderer;

struct EngineConfig
{
    const char* title  = "airstrike3d";
    int         width  = 800;
    int         height = 600;
};

class Engine
{
public:
    Engine();
    ~Engine();
    Engine(const Engine&)            = delete;
    Engine& operator=(const Engine&) = delete;
    Engine(Engine&&)                 = delete;
    Engine& operator=(Engine&&)      = delete;

    bool init(const EngineConfig& config);
    void shutdown();

    void run();
    void stop() { running_ = false; }

    [[nodiscard]] entt::registry&       registry() { return registry_; }
    [[nodiscard]] const entt::registry& registry() const { return registry_; }

    [[nodiscard]] SDL_GPUDevice*   device() const { return device_; }
    [[nodiscard]] SDL_Window*      window() const { return window_; }
    [[nodiscard]] ShaderManager*   shaders() const { return shader_manager_.get(); }
    [[nodiscard]] CameraSystem*    camera_system() const { return camera_system_.get(); }
    [[nodiscard]] ImGuiLayer*      imgui() const { return imgui_layer_.get(); }
    [[nodiscard]] Renderer*        renderer() const { return renderer_.get(); }

    [[nodiscard]] float delta_time() const { return delta_time_; }
    [[nodiscard]] bool  mouse_captured() const { return mouse_captured_; }

    void set_mouse_captured(bool captured);

private:
    void process_events();
    void update();
    void render();

    void setup_default_scene();

    entt::registry registry_;

    SDL_Window*    window_ = nullptr;
    SDL_GPUDevice* device_ = nullptr;

    std::unique_ptr<ShaderManager> shader_manager_;
    std::unique_ptr<CameraSystem>  camera_system_;
    std::unique_ptr<ImGuiLayer>    imgui_layer_;
    std::unique_ptr<Renderer>      renderer_;

    SDL_GPUTextureFormat swapchain_format_ = SDL_GPU_TEXTUREFORMAT_INVALID;

    Uint64 last_time_   = 0;
    float  delta_time_  = 0.016f;
    bool   running_     = false;
    bool   mouse_captured_ = false;
};

} // namespace as3

