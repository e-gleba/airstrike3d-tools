#pragma once

#include <GL/gl.h>
#include <atomic>
#include <cstdint>
#include <memory>
#include <mutex>
#include <optional>
#include <utility>
#include <windows.h>

#include <safetyhook.hpp>

#include "sdk/lua/callback.hpp"
#include "sdk/lua/script_engine.hpp"

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
    safetyhook::InlineHook wgl_swap, gl_matrix_mode, gl_load_identity, glu_look_at;
    void reset() { *this = {}; }
};

struct context final
{
    HWND    window{};
    WNDPROC original_wnd_proc{};

    std::atomic<bool> imgui_initialized{ false }, should_unload{ false }, show_ui{ true };

    GLenum        current_matrix_mode{ GL_MODELVIEW };
    hook_registry hooks;

    // ─── Scripting (opaque pimpl — no sol2 exposure) ─────────────────────────
    std::optional<script_engine> lua;
    std::recursive_mutex        lua_mutex;
    callback_list               callbacks{ lua_mutex };

    std::atomic<render_api> detected_api{ render_api::unknown };
    std::atomic<bool>       overlay_available{ false };
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
