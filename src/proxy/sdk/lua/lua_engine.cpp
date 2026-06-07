#include "lua_engine.hpp"
#include "sdk/core/context.hpp"

#include <algorithm>
#include <filesystem>
#include <ranges>
#include <sol/sol.hpp>
#include <spdlog/spdlog.h>
#include <vector>

namespace sdk::lua
{

void load_plugins()
{
    namespace fs = std::filesystem;

    g_ctx.lua = std::make_unique<sol::state>();
    g_ctx.lua->open_libraries(sol::lib::base,
                              sol::lib::math,
                              sol::lib::string,
                              sol::lib::table,
                              sol::lib::io,
                              sol::lib::os,
                              sol::lib::package);

    for (auto register_fn : { register_sdk_bindings,
                              register_ui_bindings,
                              register_math_bindings,
                              register_constant_bindings })
    {
        register_fn(*g_ctx.lua);
    }

    const auto plugin_dir{ fs::path(".") / k_plugin_dir };
    if (!fs::exists(plugin_dir))
    {
        spdlog::warn("[sdk] plugins directory '{}' not found, creating it",
                     plugin_dir.string());
        fs::create_directories(plugin_dir);
        return;
    }

    auto scripts{ std::ranges::to<std::vector>(
        fs::directory_iterator(plugin_dir) |
        std::views::filter(
            [](const fs::directory_entry& e)
            { return e.is_regular_file() && e.path().extension() == ".lua"; }) |
        std::views::transform(&fs::directory_entry::path)) };

    std::ranges::sort(scripts);

    for (const auto& path : scripts)
    {
        spdlog::info("[sdk] loading plugin: {}", path.filename().string());

        auto r{ g_ctx.lua->safe_script_file(path.string(),
                                            sol::script_pass_on_error) };
        if (!r.valid())
        {
            sol::error err{ r };
            spdlog::error("[sdk] failed to load {}: {}",
                          path.filename().string(),
                          err.what());
        }
    }

    g_ctx.cb.on_load.invoke();
    spdlog::info("[sdk] all plugins loaded ({} scripts)", scripts.size());
}

void unload_plugins()
{
    std::lock_guard lk{ g_ctx.lua_mutex };
    g_ctx.cb.on_unload.invoke();
    g_ctx.clear_callbacks();
    g_ctx.lua.reset();
    spdlog::info("[sdk] plugins unloaded");
}

} // namespace sdk::lua