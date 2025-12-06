#pragma once

#include "audio.hpp"
#include "engine.hpp"
#include "renderer.hpp"
#include "window.hpp"

#include <entt/entt.hpp>

namespace euengine
{

/// Input state provided to game each frame
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

/// Pre-initialization settings that game can configure before engine init
/// All SDL-free, pure data structures
struct preinit_settings final
{
    window_settings   window   = {};
    renderer_settings renderer = {};
    audio_settings    audio    = {};

    [[nodiscard]] static constexpr preinit_settings defaults() noexcept
    {
        return preinit_settings {
            .window   = window_settings {},
            .renderer = renderer_settings::defaults(),
            .audio    = audio_settings::defaults(),
        };
    }
};

/// Result from game preinit callback
enum class preinit_result : std::uint8_t
{
    ok,       ///< Continue with initialization
    skip,     ///< Skip game loading (engine runs without game)
    quit,     ///< Abort application launch
};

extern "C"
{
    /// Called before SDL initialization - game can modify settings
    /// Return preinit_result to control engine behavior
    using game_preinit_fn = preinit_result (*)(preinit_settings* settings);

    /// Called after engine initialization
    using game_init_fn = bool (*)(engine_context* ctx);

    /// Called on engine shutdown
    using game_shutdown_fn = void (*)();

    /// Called each frame for game logic
    using game_update_fn = void (*)(engine_context* ctx);

    /// Called each frame for rendering
    using game_render_fn = void (*)(engine_context* ctx);

    /// Called each frame for UI (ImGui)
    using game_ui_fn = void (*)(engine_context* ctx);
}

#if defined(_WIN32) || defined(_WIN64)
#define GAME_API extern "C" __declspec(dllexport)
#else
#define GAME_API extern "C" __attribute__((visibility("default")))
#endif

} // namespace euengine
