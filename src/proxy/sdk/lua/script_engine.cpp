#include "script_engine.hpp"

#include <sol/sol.hpp>

#include <algorithm>
#include <filesystem>
#include <ranges>
#include <spdlog/spdlog.h>
#include <vector>

namespace sdk::lua
{

// Forward declarations — binding functions are internal, no public header needed.
void register_sdk_bindings(sol::state& lua);
void register_ui_bindings(sol::state& lua);
void register_math_bindings(sol::state& lua);
void register_constant_bindings(sol::state& lua);

} // namespace sdk::lua

namespace sdk
{

struct script_engine::impl
{
    sol::state lua;

    impl()
    {
        lua.open_libraries(sol::lib::base,
                           sol::lib::math,
                           sol::lib::string,
                           sol::lib::table,
                           sol::lib::io,
                           sol::lib::os,
                           sol::lib::package);
    }

    void register_bindings()
    {
        lua::register_sdk_bindings(lua);
        lua::register_ui_bindings(lua);
        lua::register_math_bindings(lua);
        lua::register_constant_bindings(lua);
    }

    void load_plugins(const std::filesystem::path& plugin_dir)
    {
        namespace fs = std::filesystem;

        if (!fs::exists(plugin_dir))
        {
            spdlog::warn(
                "[sdk] plugins directory '{}' not found, creating it", plugin_dir.string());
            fs::create_directories(plugin_dir);
            return;
        }

        auto scripts = std::ranges::to<std::vector>(
            fs::directory_iterator(plugin_dir) |
            std::views::filter(
                [](const fs::directory_entry& e)
                { return e.is_regular_file() && e.path().extension() == ".lua"; }) |
            std::views::transform(&fs::directory_entry::path));

        std::ranges::sort(scripts);

        for (const auto& path : scripts)
        {
            spdlog::info("[sdk] loading plugin: {}", path.filename().string());

            auto result =
                lua.safe_script_file(path.string(), sol::script_pass_on_error);
            if (!result.valid())
            {
                sol::error err = result;
                spdlog::error("[sdk] failed to load {}: {}",
                              path.filename().string(),
                              err.what());
            }
        }

        spdlog::info("[sdk] all plugins loaded ({} scripts)", scripts.size());
    }

    void invoke_impl(std::string_view event_name, auto&&... args)
    {
        sol::protected_function fn = lua[std::string{ event_name }];
        if (!fn.valid())
        {
            return;
        }

        auto result = fn(std::forward<decltype(args)>(args)...);
        if (!result.valid())
        {
            sol::error err = result;
            spdlog::error("[sdk] Lua callback '{}' error: {}", event_name, err.what());
        }
    }
};

// ─── Special member functions ────────────────────────────────────────────────

script_engine::script_engine() : pimpl{ std::make_unique<impl>() }
{
}

script_engine::~script_engine() noexcept = default;

script_engine::script_engine(script_engine&&) noexcept = default;
script_engine& script_engine::operator=(script_engine&&) noexcept = default;

// ─── Public interface ────────────────────────────────────────────────────────

void script_engine::register_bindings()
{
    if (pimpl)
    {
        pimpl->register_bindings();
    }
}

void script_engine::load_plugins(const std::filesystem::path& plugin_dir)
{
    if (pimpl)
    {
        pimpl->load_plugins(plugin_dir);
    }
}

void script_engine::invoke(std::string_view event_name)
{
    if (pimpl)
    {
        pimpl->invoke_impl(event_name);
    }
}

void script_engine::invoke_impl(std::string_view event_name, auto&&... args)
{
    if (pimpl)
    {
        pimpl->invoke_impl(event_name, std::forward<decltype(args)>(args)...);
    }
}

script_engine::operator bool() const noexcept
{
    return pimpl != nullptr;
}

} // namespace sdk
