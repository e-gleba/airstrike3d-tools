/// @file context.hpp
/// @brief Global application context — no sol2 / Lua types exposed.

#pragma once

#include <GL/gl.h>
#include <atomic>
#include <cstdint>
#include <memory>
#include <mutex>
#include <utility>
#include <windows.h>

#include <safetyhook.hpp>

#include "sdk/lua/callback.hpp"

namespace sdk
{

enum class render_api : uint8_t
{
    unknown,
    opengl,
    directx
};

struct hook_registry final
{
    safetyhook::InlineHook wgl_swap, gl_matrix_mode, gl_load_identity,
        glu_look_at;
    void reset() { *this = {}; }
};

struct context final
{
    HWND    window{};
    WNDPROC original_wnd_proc{};

    std::atomic<bool> imgui_initialized{ false }, should_unload{ false },
        show_ui{ true };

    GLenum        current_matrix_mode{ GL_MODELVIEW };
    hook_registry hooks;

    // Lua subsystem — owned by the lua module, not exposed here.
    // The lua engine manages its own state lifetime internally.
    std::recursive_mutex lua_mutex;

    std::atomic<render_api> detected_api{ render_api::unknown };
    std::atomic<bool>       overlay_available{ false };

    // Callbacks — type-erased, no sol2 leakage.
    struct final
    {
        lua::callback_list<>               on_frame;
        lua::callback_list<>               on_overlay;
        lua::callback_list<GLenum>         on_gl_identity;
        lua::consuming_callback_list<double, double, double,
                                       double, double, double,
                                       double, double, double>  on_glu_lookat;
        lua::consuming_callback_list<int>  on_key_down;
        lua::callback_list<>               on_load;
        lua::callback_list<>               on_unload;
    } cb;

    context()
        : cb{ .on_frame       = lua::callback_list<>{ lua_mutex },
              .on_overlay     = lua::callback_list<>{ lua_mutex },
              .on_gl_identity = lua::callback_list<GLenum>{ lua_mutex },
              .on_glu_lookat  = lua::consuming_callback_list<double, double, double,
                                                   double, double, double,
                                                   double, double, double>{ lua_mutex },
              .on_key_down    = lua::consuming_callback_list<int>{ lua_mutex },
              .on_load        = lua::callback_list<>{ lua_mutex },
              .on_unload      = lua::callback_list<>{ lua_mutex } }
    {
    }

    void clear_callbacks()
    {
        cb.on_frame.clear();
        cb.on_overlay.clear();
        cb.on_gl_identity.clear();
        cb.on_glu_lookat.clear();
        cb.on_key_down.clear();
        cb.on_load.clear();
        cb.on_unload.clear();
    }
};

inline context g_ctx;

constexpr auto k_ui_toggle_key = VK_INSERT;
constexpr auto k_glsl_version  = "#version 110";
constexpr auto k_plugin_dir    = "plugins";

template <typename fn_ptr>
[[nodiscard]] auto call_orig(safetyhook::InlineHook& hook) -> fn_ptr
{
    return reinterpret_cast<fn_ptr>(hook.trampoline().address());
}

} // namespace sdk
