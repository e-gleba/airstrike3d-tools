#pragma once

#include "sdk/lua/callback.hpp"
#include "sdk/lua/script_engine.hpp"

#include <safetyhook.hpp>

#include <atomic>
#include <cstdint>
#include <mutex>
#include <optional>

#include <windows.h>

#include <GL/gl.h>

namespace sdk
{

enum class render_api : std::uint8_t
{
    unknown,
    opengl,
    directx
};

struct hook_registry
{
    safetyhook::InlineHook wgl_swap_buffers;
    safetyhook::InlineHook gl_matrix_mode;
    safetyhook::InlineHook gl_load_identity;
    safetyhook::InlineHook glu_look_at;
};

struct context
{
    // Window state
    HWND    window{ nullptr };
    WNDPROC original_wnd_proc{ nullptr };

    // Overlay state
    std::atomic<bool> overlay_initialized{ false };
    std::atomic<bool> overlay_visible{ true };
    std::atomic<bool> should_exit{ false };

    // Render API detection
    std::atomic<render_api> detected_api{ render_api::unknown };

    // GL state tracking
    GLenum current_matrix_mode{ GL_MODELVIEW };

    // Hook registry
    hook_registry hooks;

    // Scripting engine (pimpl, no sol2 exposure)
    std::optional<script_engine> lua;
    std::recursive_mutex         lua_mutex;

    // Callbacks (type-erased, no sol2 exposure)
    callback_list callbacks{ lua_mutex };
};

inline context g_ctx;

} // namespace sdk
