#pragma once

#include <windows.h>
#include <GL/gl.h>
#include <atomic>
#include <cstdint>
#include <memory>
#include <mutex>

#include <safetyhook.hpp>
#include <sol/sol.hpp>

#include "sdk/util/callback.hpp"

namespace sdk
{

// ─── Version — set by the per-proxy version_stub.cpp ─────────────────────────

extern const char* const k_version;

// ─── Render API ──────────────────────────────────────────────────────────────

enum class render_api : uint8_t
{
    unknown = 0,
    opengl  = 1,
};

// ─── Hook registry ───────────────────────────────────────────────────────────

struct hook_registry final
{
    safetyhook::InlineHook wgl_swap;
    safetyhook::InlineHook gl_matrix_mode;
    safetyhook::InlineHook gl_load_identity;
    safetyhook::InlineHook glu_look_at;

    void reset() { *this = {}; }
};

// ─── Global context ──────────────────────────────────────────────────────────

struct context final
{
    HWND    window{};
    WNDPROC original_wnd_proc{};

    std::atomic<bool> imgui_initialized{ false };
    std::atomic<bool> should_unload{ false };
    std::atomic<bool> show_ui{ true };

    GLenum current_matrix_mode{ GL_MODELVIEW };

    hook_registry hooks;

    std::unique_ptr<sol::state> lua;
    std::recursive_mutex        lua_mutex;

    // ─── Render API detection ────────────────────────────────────────────

    render_api        detected_api{ render_api::unknown };
    std::atomic<bool> overlay_available{ false };

    // ─── GDI fallback overlay ────────────────────────────────────────────

    HWND                  fallback_window{};
    WNDPROC               fallback_orig_wnd_proc{};
    safetyhook::InlineHook fallback_create_window_hook;

    struct final
    {
        callback_list on_frame;
        callback_list on_overlay;
        callback_list on_gl_identity;
        callback_list on_glu_lookat;
        callback_list on_key_down;
        callback_list on_load;
        callback_list on_unload;
    } cb;

    context()
        : cb{ .on_frame       = callback_list(lua_mutex),
              .on_overlay     = callback_list(lua_mutex),
              .on_gl_identity = callback_list(lua_mutex),
              .on_glu_lookat  = callback_list(lua_mutex),
              .on_key_down    = callback_list(lua_mutex),
              .on_load        = callback_list(lua_mutex),
              .on_unload      = callback_list(lua_mutex) }
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
