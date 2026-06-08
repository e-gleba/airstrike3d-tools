// src/proxy/sdk/lua/detail/lua_engine.cpp
// Lua engine implementation using sol2.
// This file contains all sol2-specific code.

#include "sdk/lua/lua_state.hpp"
#include "sdk/core/context.hpp"

#include <sol/sol.hpp>

#include <filesystem>
#include <spdlog/spdlog.h>

namespace sdk::lua
{

namespace fs = std::filesystem;

namespace
{

// Global sol2 state (owned by LuaState)
sol::state g_lua;
bool       g_initialized = false;

// Adapter: convert sol::protected_function to std::function<void()>
auto wrap_void(sol::protected_function fn) -> callback_list::fn_type
{
    return [fn = std::move(fn)]() {
        auto result = fn();
        if (!result.valid()) {
            sol::error err = result;
            spdlog::error("Lua callback error: {}", err.what());
        }
    };
}

// Adapter: convert sol::protected_function to std::function<bool()>
auto wrap_bool(sol::protected_function fn) -> consuming_callbacks::fn_type
{
    return [fn = std::move(fn)]() -> bool {
        auto result = fn();
        if (!result.valid()) {
            sol::error err = result;
            spdlog::error("Lua callback error: {}", err.what());
            return false;
        }
        return result.get_type() == sol::type::boolean && result.get<bool>();
    };
}

} // namespace

// ─────────────────────────────────────────────────────────────────────────────
// LuaState implementation

struct LuaState::impl
{
    // Future: per-instance state if needed
};

LuaState::LuaState() : pimpl(std::make_unique<impl>()) {}
LuaState::~LuaState() = default;

LuaState::LuaState(LuaState&&) noexcept = default;
LuaState& LuaState::operator=(LuaState&&) noexcept = default;

void LuaState::initialize()
{
    if (g_initialized) {
        return;
    }

    g_lua.open_libraries(
        sol::lib::base,
        sol::lib::math,
        sol::lib::string,
        sol::lib::table,
        sol::lib::io,
        sol::lib::os
    );

    // Register bindings (implemented in register_bindings.cpp)
    void register_all_bindings(sol::state&);
    register_all_bindings(g_lua);

    // Register callback hooks
    g_lua["hook_frame"] = [](sol::protected_function fn) {
        g_ctx.cb.on_frame.add(wrap_void(std::move(fn)));
    };

    g_lua["hook_overlay"] = [](sol::protected_function fn) {
        g_ctx.cb.on_overlay.add(wrap_void(std::move(fn)));
    };

    g_lua["hook_key_down"] = [](sol::protected_function fn) {
        g_ctx.cb.on_key_down.add(wrap_bool(std::move(fn)));
    };

    g_initialized = true;
    spdlog::info("Lua engine initialized");
}

void LuaState::shutdown()
{
    if (!g_initialized) {
        return;
    }

    g_ctx.cb.on_frame.clear();
    g_ctx.cb.on_overlay.clear();
    g_ctx.cb.on_key_down.clear();

    g_lua = sol::state{};
    g_initialized = false;

    spdlog::info("Lua engine shut down");
}

void LuaState::load_plugins()
{
    if (!g_initialized) {
        spdlog::warn("Lua engine not initialized");
        return;
    }

    auto plugin_dir = fs::current_path() / "plugins";
    if (!fs::exists(plugin_dir)) {
        spdlog::info("No plugins directory found");
        return;
    }

    for (const auto& entry : fs::directory_iterator(plugin_dir)) {
        if (entry.is_regular_file() && entry.path().extension() == ".lua") {
            auto path = entry.path();
            spdlog::info("Loading plugin: {}", path.filename().string());

            auto result = g_lua.safe_script_file(path.string());
            if (!result.valid()) {
                sol::error err = result;
                spdlog::error("Failed to load {}: {}", path.filename().string(), err.what());
            }
        }
    }

    g_ctx.cb.on_load.invoke();
}

void LuaState::unload_plugins()
{
    g_ctx.cb.on_unload.invoke();
    g_ctx.cb.on_frame.clear();
    g_ctx.cb.on_overlay.clear();
    g_ctx.cb.on_key_down.clear();
}

void LuaState::invoke_frame()
{
    g_ctx.cb.on_frame.invoke();
}

void LuaState::invoke_overlay()
{
    g_ctx.cb.on_overlay.invoke();
}

bool LuaState::invoke_key_down(int key)
{
    // For now, key_down callbacks don't receive the key parameter
    // This matches the original implementation
    return g_ctx.cb.on_key_down.invoke();
}

} // namespace sdk::lua
