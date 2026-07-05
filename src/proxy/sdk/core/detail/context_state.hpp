/// @file context_state.hpp
/// @brief Platform-specific runtime state (internal — not part of the public API).

#pragma once

#include <safetyhook.hpp>
#include <windows.h>

namespace sdk::detail
{

struct hook_registry final
{
    safetyhook::InlineHook wgl_swap;
    safetyhook::InlineHook gl_matrix_mode;
    safetyhook::InlineHook gl_load_identity;
    safetyhook::InlineHook glu_look_at;

    void reset() noexcept { *this = {}; }
};

struct context_state final
{
    HWND    window{};
    WNDPROC original_wnd_proc{};
    hook_registry hooks;
};

inline context_state g_state;

template <typename fn_ptr>
[[nodiscard]] auto call_orig(safetyhook::InlineHook& hook) noexcept -> fn_ptr
{
    return reinterpret_cast<fn_ptr>(hook.trampoline().address());
}

} // namespace sdk::detail
