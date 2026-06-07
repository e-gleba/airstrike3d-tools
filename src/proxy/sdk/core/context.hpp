#pragma once

#include <GL/gl.h>
#include <atomic>
#include <cstdint>
#include <memory>
#include <mutex>
#include <utility>
#include <windows.h>

#include <safetyhook.hpp>
#include <sol/sol.hpp>

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

    std::unique_ptr<sol::state> lua;
    std::recursive_mutex        lua_mutex;

    std::atomic<render_api> detected_api{ render_api::unknown };
    std::atomic<bool>       overlay_available{ false };

    struct final
    {
        callback_list on_frame, on_overlay, on_gl_identity, on_glu_lookat,
            on_key_down, on_load, on_unload;
    } cb;

    context()
        : cb{ .on_frame       = callback_list{ lua_mutex },
              .on_overlay     = callback_list{ lua_mutex },
              .on_gl_identity = callback_list{ lua_mutex },
              .on_glu_lookat  = callback_list{ lua_mutex },
              .on_key_down    = callback_list{ lua_mutex },
              .on_load        = callback_list{ lua_mutex },
              .on_unload      = callback_list{ lua_mutex } }
    {
    }

    template <typename... CBs> static void clear_all(CBs&... cbs)
    {
        (cbs.clear(), ...);
    }

    void clear_callbacks()
    {
        clear_all(cb.on_frame,
                  cb.on_overlay,
                  cb.on_gl_identity,
                  cb.on_glu_lookat,
                  cb.on_key_down,
                  cb.on_load,
                  cb.on_unload);
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