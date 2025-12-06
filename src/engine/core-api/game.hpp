#pragma once

#include "audio.hpp"
#include "engine.hpp"
#include "renderer.hpp"

#include <entt/entt.hpp>

namespace euengine
{

struct input_state final
{
    const bool* keyboard       = nullptr;
    float       mouse_xrel     = 0.0f;
    float       mouse_yrel     = 0.0f;
    bool        mouse_captured = false;
};

struct display_info final
{
    int   width  = 0;
    int   height = 0;
    float aspect = 1.0f;
};

struct engine_context final
{
    entt::registry*    registry   = nullptr;
    i_renderer*        renderer   = nullptr;
    i_shader_manager*  shaders    = nullptr;
    i_audio*           audio      = nullptr;
    i_engine_settings* settings   = nullptr;
    void*              imgui_ctx  = nullptr;
    display_info       display    = {};
    input_state        input      = {};
    float              delta_time = 0.0f;
};

extern "C"
{
    using game_init_fn     = bool (*)(engine_context* ctx);
    using game_shutdown_fn = void (*)();
    using game_update_fn   = void (*)(engine_context* ctx);
    using game_render_fn   = void (*)(engine_context* ctx);
    using game_ui_fn       = void (*)(engine_context* ctx);
}

#if defined(_WIN32) || defined(_WIN64)
#define GAME_API extern "C" __declspec(dllexport)
#else
#define GAME_API extern "C" __attribute__((visibility("default")))
#endif

} // namespace euengine
