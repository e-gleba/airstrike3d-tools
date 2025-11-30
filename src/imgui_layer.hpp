#pragma once

#include <SDL3/SDL.h>
#include <SDL3/SDL_gpu.h>

#include <functional>

namespace as3
{

class ImGuiLayer
{
public:
    using DrawCallback = std::function<void()>;

    ImGuiLayer()                             = default;
    ~ImGuiLayer();
    ImGuiLayer(const ImGuiLayer&)            = delete;
    ImGuiLayer& operator=(const ImGuiLayer&) = delete;
    ImGuiLayer(ImGuiLayer&&)                 = delete;
    ImGuiLayer& operator=(ImGuiLayer&&)      = delete;

    bool init(SDL_GPUDevice* device, SDL_Window* window);
    void shutdown();

    void process_event(const SDL_Event* event);

    void begin_frame();
    void end_frame(SDL_GPUCommandBuffer* cmd, SDL_GPUTexture* target);

    void set_input_enabled(bool enabled) { input_enabled_ = enabled; }
    [[nodiscard]] bool input_enabled() const { return input_enabled_; }

    void set_draw_callback(DrawCallback callback);

private:
    SDL_GPUDevice*     device_          = nullptr;
    SDL_GPUTextureFormat target_format_ = SDL_GPU_TEXTUREFORMAT_INVALID;
    DrawCallback       draw_callback_;
    bool               initialized_    = false;
    bool               input_enabled_  = true;
};

} // namespace as3

