/// @file context.hpp
/// @brief Global application context — no backend types exposed.

#pragma once

#include <GL/gl.h>
#include <atomic>
#include <cstdint>
#include <memory>
#include <mutex>
#include <utility>
#include <windows.h>

#include <safetyhook.hpp>

#include "sdk/core/callback.hpp"
#include "sdk/core/types.hpp"

namespace sdk
{

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

    // Scripting subsystem — owned by the scripting module, not exposed here.
    // The scripting engine manages its own state lifetime internally.
    std::recursive_mutex scripting_mutex;

    std::atomic<render_api> detected_api{ render_api::unknown };
    std::atomic<bool>       overlay_available{ false };

    // Callbacks — type-erased, no backend leakage.
    struct final
    {
        callback::callback_list<>               on_frame;
        callback::callback_list<>               on_overlay;
        callback::callback_list<GLenum>         on_gl_identity;
        callback::consuming_callback_list<double, double, double,
                                       double, double, double,
                                       double, double, double>  on_glu_lookat;
        callback::consuming_callback_list<int>  on_key_down;
        callback::callback_list<>               on_load;
        callback::callback_list<>               on_unload;
    } cb;

    context()
        : cb{ .on_frame       = callback::callback_list<>{ scripting_mutex },
              .on_overlay     = callback::callback_list<>{ scripting_mutex },
              .on_gl_identity = callback::callback_list<GLenum>{ scripting_mutex },
              .on_glu_lookat  = callback::consuming_callback_list<double, double, double,
                                                   double, double, double,
                                                   double, double, double>{ scripting_mutex },
              .on_key_down    = callback::consuming_callback_list<int>{ scripting_mutex },
              .on_load        = callback::callback_list<>{ scripting_mutex },
              .on_unload      = callback::callback_list<>{ scripting_mutex } }
    {
    }

    template <typename... CBs>
    static void clear_all(CBs&... cbs)
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
