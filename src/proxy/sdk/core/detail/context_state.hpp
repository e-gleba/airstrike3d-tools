/// @file context_state.hpp
/// @brief Platform-specific runtime state (internal — not part of the public
/// API).

#pragma once

#include <windows.h>

namespace sdk::detail
{

struct context_state final
{
    HWND    window{};
    WNDPROC original_wnd_proc{};
};

inline context_state g_state;

} // namespace sdk::detail
