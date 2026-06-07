#include "lua_engine.hpp"

#include "sdk/core/context.hpp"

#include <filesystem>
#include <spdlog/spdlog.h>

namespace sdk::lua
{

void load_plugins()
{
    g_ctx.lua.emplace();
    g_ctx.lua->register_bindings();

    const std::filesystem::path plugin_dir{ "plugins" };
    g_ctx.lua->load_plugins(plugin_dir);

    g_ctx.callbacks.invoke<>("on_load");
    spdlog::info("[sdk] plugins loaded");
}

void unload_plugins()
{
    std::lock_guard lk{ g_ctx.lua_mutex };

    g_ctx.callbacks.invoke<>("on_unload");
    g_ctx.callbacks.clear();
    g_ctx.lua.reset();

    spdlog::info("[sdk] plugins unloaded");
}

} // namespace sdk::lua
