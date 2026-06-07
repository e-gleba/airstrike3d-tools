#include "script_engine.hpp"

#include "bindings_constants.hpp"
#include "bindings_math.hpp"
#include "bindings_sdk.hpp"
#include "bindings_ui.hpp"

#include <sol/sol.hpp>

#include <algorithm>
#include <filesystem>
#include <ranges>
#include <spdlog/spdlog.h>
#include <vector>

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
            spdlog::warn("[sdk] plugins directory '{}' not found, creating it",
                         plugin_dir.string());
            fs::create_directories(plugin_dir);
            return;
        }

        auto scripts = std::ranges::to<std::vector>(
            fs::directory_iterator(plugin_dir) |
            std::views::filter([](const fs::directory_entry& e) {
                return e.is_regular_file() && e.path().extension() == ".lua";
            }) |
            std::views::transform(&fs::directory_entry::path));

        std::ranges::sort(scripts);

        for (const auto& path : scripts)
        {
            spdlog::info("[sdk] loading plugin: {}", path.filename().string());

            auto result = lua.safe_script_file(path.string(), sol::script_pass_on_error);
            if (!result.valid())
            {
                sol::error err = result;
                spdlog::error("[sdk] failed to load {}: {}", path.filename().string(), err.what());
            }
        }

        spdlog::info("[sdk] all plugins loaded ({} scripts)", scripts.size());
    }

    template <typename... Args>
    void invoke(std::string_view event_name, Args&&... args)
    {
        sol::protected_function fn = lua[event_name];
        if (!fn.valid())
        {
            return;
        }

        auto result = fn(std::forward<Args>(args)...);
        if (!result.valid())
        {
            sol::error err = result;
            spdlog::error("[sdk] Lua callback '{}' error: {}", event_name, err.what());
        }
    }
};

script_engine::script_engine() : pimpl{ std::make_unique<impl>() }
{
}

script_engine::~script_engine() noexcept = default;

script_engine::script_engine(script_engine&&) noexcept = default;
script_engine& script_engine::operator=(script_engine&&) noexcept = default;

void script_engine::register_bindings()
{
    if (pimpl)
    {
        pimpl->register_bindings();
    }
}

void script_engine::load_plugins(std::filesystem::path plugin_dir)
{
    if (pimpl)
    {
        pimpl->load_plugins(plugin_dir);
    }
}

template <typename... Args>
void script_engine::invoke(std::string_view event_name, Args&&... args)
{
    if (pimpl)
    {
        pimpl->invoke(event_name, std::forward<Args>(args)...);
    }
}

script_engine::operator bool() const noexcept
{
    return pimpl != nullptr;
}

// Explicit instantiations for common callback signatures
template void script_engine::invoke<>(std::string_view);
template void script_engine::invoke<int>(std::string_view, int&&);
template void script_engine::invoke<double, double, double, double, double, double, double, double, double>(
    std::string_view,
    double&&,
    double&&,
    double&&,
    double&&,
    double&&,
    double&&,
    double&&,
    double&&,
    double&&);

} // namespace sdk
