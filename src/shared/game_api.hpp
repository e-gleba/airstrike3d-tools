#pragma once

#include "camera.hpp"
#include "renderer_interface.hpp"

#include <entt/entt.hpp>

#include <cstdint>

namespace as3
{

// Input state passed to game each frame
struct input_state final
{
    const bool* keyboard       = nullptr;
    float       mouse_xrel     = 0.0f;
    float       mouse_yrel     = 0.0f;
    bool        mouse_captured = false;
};

// Window/display info
struct display_info final
{
    int   width  = 0;
    int   height = 0;
    float aspect = 1.0f;
};

// Context passed from engine to game
struct engine_context final
{
    entt::registry*  registry    = nullptr;
    IRenderer*       renderer    = nullptr;
    IShaderManager*  shaders     = nullptr;
    void*            imgui_ctx   = nullptr;  // Shared ImGui context (ImGuiContext*)
    display_info     display;
    input_state      input;
    float            delta_time  = 0.0f;
};

// Game interface - game DLL must export these
extern "C"
{
    using game_init_fn     = bool (*)(engine_context* ctx);
    using game_shutdown_fn = void (*)();
    using game_update_fn   = void (*)(engine_context* ctx);
    using game_render_fn   = void (*)(engine_context* ctx);
    using game_ui_fn       = void (*)(engine_context* ctx);
}

// Macros to export game functions from DLL
#ifdef _WIN32
    #define GAME_API extern "C" __declspec(dllexport)
#else
    #define GAME_API extern "C" __attribute__((visibility("default")))
#endif

} // namespace as3
