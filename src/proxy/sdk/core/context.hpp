/// @file context.hpp
/// @brief Global application context — no render/Lua implementation types.

#pragma once

#include <atomic>
#include <mutex>
#include <windows.h>

#include "sdk/lua/callback.hpp"

namespace sdk
{

struct context final
{
    HWND window{};  // set by render subsystem, read by Lua bindings

    std::recursive_mutex lua_mutex;

    // Lua lifecycle callbacks only.
    // Render callbacks (frame, overlay, key, GL) are owned by render::HookSystem.
    struct final
    {
        lua::callback_list<> on_load;
        lua::callback_list<> on_unload;
    } cb;

    context()
        : cb{ .on_load  = lua::callback_list<>{ lua_mutex },
              .on_unload = lua::callback_list<>{ lua_mutex } }
    {}

    void clear_callbacks()
    {
        cb.on_load.clear();
        cb.on_unload.clear();
    }
};

inline context g_ctx;

constexpr auto k_plugin_dir = "plugins";

} // namespace sdk
