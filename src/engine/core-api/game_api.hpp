#pragma once

#include "audio_interface.hpp"
#include "camera.hpp"
#include "engine_settings_interface.hpp"
#include "platform.hpp"
#include "renderer_interface.hpp"

#include <entt/entt.hpp>

namespace as3
{

/// Input state passed to game each frame
struct input_state final
{
    const bool* keyboard       = nullptr;
    float       mouse_xrel     = 0.0f;
    float       mouse_yrel     = 0.0f;
    bool        mouse_captured = false;
};

/// Display information
struct display_info final
{
    int   width  = 0;
    int   height = 0;
    float aspect = 1.0f;
};

/// Engine context passed to game callbacks
struct engine_context final
{
    entt::registry*  registry   = nullptr;
    IRenderer*       renderer   = nullptr;
    IShaderManager*  shaders    = nullptr;
    IAudio*          audio      = nullptr;
    IEngineSettings* settings   = nullptr;
    void*            imgui_ctx  = nullptr;
    display_info     display    = {};
    input_state      input      = {};
    float            delta_time = 0.0f;
};

/// Game DLL function pointer types
extern "C"
{
    using game_init_fn     = bool (*)(engine_context* ctx);
    using game_shutdown_fn = void (*)();
    using game_update_fn   = void (*)(engine_context* ctx);
    using game_render_fn   = void (*)(engine_context* ctx);
    using game_ui_fn       = void (*)(engine_context* ctx);
}

/// Export macro for game library functions
/// Uses platform detection to select correct visibility attribute
#if defined(_WIN32) || defined(_WIN64)
#define GAME_EXPORT extern "C" __declspec(dllexport)
#else
#define GAME_EXPORT extern "C" __attribute__((visibility("default")))
#endif

// Legacy alias for compatibility
#define GAME_API GAME_EXPORT

} // namespace as3