/// @file opengl_state.hpp
/// @brief OpenGL hook state (internal).

#pragma once

#include <safetyhook.hpp>

namespace sdk::gl::detail
{

struct hook_registry final
{
    safetyhook::InlineHook wgl_swap;
    safetyhook::InlineHook gl_matrix_mode;
    safetyhook::InlineHook gl_load_identity;
    safetyhook::InlineHook glu_look_at;

    void reset() noexcept { *this = {}; }
};

inline hook_registry g_hooks;

template <typename fn_ptr>
[[nodiscard]] auto call_orig(safetyhook::InlineHook& hook) noexcept -> fn_ptr
{
    return reinterpret_cast<fn_ptr>(hook.trampoline().address());
}

} // namespace sdk::gl::detail
