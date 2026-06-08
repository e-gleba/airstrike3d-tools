#include "detail/lua_engine_impl.hpp"

#include "sdk/core/context.hpp"
#include "sdk/lua/bindings/bindings_fwd.hpp"

#include <algorithm>
#include <filesystem>
#include <ranges>
#include <spdlog/spdlog.h>
#include <vector>

namespace sdk::lua
{

// ── impl ────────────────────────────────────────────────────────────────────

engine::impl::impl()
{
    lua.open_libraries(sol::lib::base,
                       sol::lib::math,
                       sol::lib::string,
                       sol::lib::table,
                       sol::lib::io,
                       sol::lib::os,
                       sol::lib::package);

    bindings::register_sdk(lua, callbacks);
    bindings::register_ui(lua);
    bindings::register_math(lua);
    bindings::register_constants(lua);
}

void engine::impl::load_plugins_from_directory()
{
    namespace fs = std::filesystem;

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
        std::views::filter([](const fs::directory_entry& e)
                           { return e.is_regular_file() && e.path().extension() == ".lua"; }) |
        std::views::transform(&fs::directory_entry::path)) };

    std::ranges::sort(scripts);

    for (const auto& path : scripts)
    {
        spdlog::info("[sdk] loading plugin: {}", path.filename().string());

        auto r{ lua.safe_script_file(path.string(), sol::script_pass_on_error) };
        if (!r.valid())
        {
            sol::error err{ r };
            spdlog::error("[sdk] failed to load {}: {}",
                          path.filename().string(),
                          err.what());
        }
    }

    plugins_loaded = !scripts.empty();
    spdlog::info("[sdk] all plugins loaded ({} scripts)", scripts.size());
}

void engine::impl::invoke_on_unload_and_clear()
{
    callbacks.on_unload.invoke();
    callbacks.clear_all();
    plugins_loaded = false;
}

// ── engine ──────────────────────────────────────────────────────────────────

engine::engine() : pimpl_{ std::make_unique<impl>() } {}

engine::~engine() = default;

engine::engine(engine&&) noexcept            = default;
engine& engine::operator=(engine&&) noexcept = default;

void engine::load_plugins()
{
    if (!pimpl_)
    {
        return;
    }

    pimpl_->load_plugins_from_directory();
    pimpl_->callbacks.on_load.invoke();
}

void engine::unload_plugins()
{
    if (!pimpl_)
    {
        return;
    }

    pimpl_->invoke_on_unload_and_clear();
    spdlog::info("[sdk] plugins unloaded");
}

void engine::invoke_on_frame()
{
    if (pimpl_)
    {
        pimpl_->callbacks.on_frame.invoke();
    }
}

void engine::invoke_on_overlay()
{
    if (pimpl_)
    {
        pimpl_->callbacks.on_overlay.invoke();
    }
}

void engine::invoke_on_gl_identity()
{
    if (pimpl_)
    {
        pimpl_->callbacks.on_gl_identity.invoke();
    }
}

void engine::invoke_on_glu_lookat()
{
    if (pimpl_)
    {
        pimpl_->callbacks.on_glu_lookat.invoke();
    }
}

bool engine::invoke_on_key_down(const int key)
{
    return pimpl_ ? pimpl_->callbacks.on_key_down.invoke_consuming(key) : false;
}

void engine::invoke_on_load()
{
    if (pimpl_)
    {
        pimpl_->callbacks.on_load.invoke();
    }
}

void engine::invoke_on_unload()
{
    if (pimpl_)
    {
        pimpl_->callbacks.on_unload.invoke();
    }
}

bool engine::has_plugins() const
{
    return pimpl_ && pimpl_->plugins_loaded;
}

} // namespace sdk::lua
