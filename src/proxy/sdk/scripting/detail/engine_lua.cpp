/// @file scripting/detail/engine_lua.cpp
/// @brief Engine implementation using Lua/sol2.
///
/// This is the **only** file that owns the sol::state instance.
/// All sol2-specific code is isolated here.

#include "sdk/scripting/engine.hpp"
#include "sdk/core/callback.hpp"
#include "sdk/core/context.hpp"
#include "sdk/core/logging.hpp"

#include <sol/sol.hpp>

#include <algorithm>
#include <filesystem>
#include <format>
#include <ranges>
#include <stdexcept>

namespace fs = std::filesystem;

namespace sdk::scripting
{

// ── Forward declarations for binding registration ───────────────────────────

namespace bindings
{
    void register_math(sol::state& lua);
    void register_constants(sol::state& lua);
    void register_sdk(sol::state& lua);
    void register_ui(sol::state& lua);
}

// ── Trampoline helpers ──────────────────────────────────────────────────────

using glu_look_at_fn = void(APIENTRY*)(GLdouble, GLdouble, GLdouble,
                                        GLdouble, GLdouble, GLdouble,
                                        GLdouble, GLdouble, GLdouble);

static void call_orig_glu_lookat(double ex, double ey, double ez,
                                  double cx, double cy, double cz,
                                  double ux, double uy, double uz)
{
    auto orig = call_orig<glu_look_at_fn>(g_ctx.hooks.glu_look_at);
    if (orig)
    {
        orig(ex, ey, ez, cx, cy, cz, ux, uy, uz);
    }
}

// ── Engine::impl ────────────────────────────────────────────────────────────

struct Engine::impl
{
    sol::state lua;

    impl()
    {
        lua.open_libraries(
            sol::lib::base,
            sol::lib::math,
            sol::lib::string,
            sol::lib::table,
            sol::lib::io,
            sol::lib::os
        );

        bindings::register_math(lua);
        bindings::register_constants(lua);
        bindings::register_sdk(lua);
        bindings::register_ui(lua);
        register_callback_hooks();

        sdk::log_info("Scripting engine initialized (Lua backend)");
    }

    // ── Callback adapters ───────────────────────────────────────────────────

    auto wrap_void(sol::protected_function fn) -> callback::callback_list<>::slot_fn
    {
        return [fn = std::move(fn)]() {
            auto result = fn();
            if (!result.valid()) {
                sol::error err = result;
                sdk::log_error(std::format("Script callback error: {}", err.what()));
            }
        };
    }

    auto wrap_bool(sol::protected_function fn) -> callback::consuming_callback_list<int>::slot_fn
    {
        return [fn = std::move(fn)](int key) -> bool {
            auto result = fn(key);
            if (!result.valid()) {
                sol::error err = result;
                sdk::log_error(std::format("Script callback error: {}", err.what()));
                return false;
            }
            return result.get_type() == sol::type::boolean && result.get<bool>();
        };
    }

    auto wrap_gl_identity(sol::protected_function fn) -> callback::callback_list<GLenum>::slot_fn
    {
        return [fn = std::move(fn)](GLenum mode) {
            auto result = fn(mode);
            if (!result.valid()) {
                sol::error err = result;
                sdk::log_error(std::format("Script callback error: {}", err.what()));
            }
        };
    }

    auto wrap_glu_lookat(sol::protected_function fn)
        -> callback::consuming_callback_list<double, double, double,
                                    double, double, double,
                                    double, double, double>::slot_fn
    {
        return [fn = std::move(fn)](double eyeX, double eyeY, double eyeZ,
                                     double centerX, double centerY, double centerZ,
                                     double upX, double upY, double upZ) -> bool {
            auto result = fn(eyeX, eyeY, eyeZ, centerX, centerY, centerZ,
                             upX, upY, upZ);
            if (!result.valid()) {
                sol::error err = result;
                sdk::log_error(std::format("Script callback error: {}", err.what()));
                return false;
            }
            return result.get_type() == sol::type::boolean && result.get<bool>();
        };
    }

    // ── Callback hook registration ──────────────────────────────────────────

    void register_callback_hooks()
    {
        auto sdk_table = lua["sdk"];

        sdk_table["on_frame"] = [this](sol::protected_function fn) {
            g_ctx.cb.on_frame.add(wrap_void(std::move(fn)));
        };

        sdk_table["on_overlay"] = [this](sol::protected_function fn) {
            g_ctx.cb.on_overlay.add(wrap_void(std::move(fn)));
        };

        sdk_table["on_key_down"] = [this](sol::protected_function fn) {
            g_ctx.cb.on_key_down.add(wrap_bool(std::move(fn)));
        };

        sdk_table["on_gl_identity"] = [this](sol::protected_function fn) {
            g_ctx.cb.on_gl_identity.add(wrap_gl_identity(std::move(fn)));
        };

        sdk_table["on_glu_lookat"] = [this](sol::protected_function fn) {
            g_ctx.cb.on_glu_lookat.add(wrap_glu_lookat(std::move(fn)));
        };

        sdk_table["on_load"] = [this](sol::protected_function fn) {
            g_ctx.cb.on_load.add(wrap_void(std::move(fn)));
        };

        sdk_table["on_unload"] = [this](sol::protected_function fn) {
            g_ctx.cb.on_unload.add(wrap_void(std::move(fn)));
        };
    }
};

// ── Engine public interface ─────────────────────────────────────────────────

Engine::Engine() : pimpl(std::make_unique<impl>()) {}

Engine::~Engine() = default;

Engine::Engine(Engine&&) noexcept = default;
Engine& Engine::operator=(Engine&&) noexcept = default;

void Engine::load_plugins()
{
    auto plugin_dir = fs::current_path() / "plugins";

    if (!fs::exists(plugin_dir)) {
        sdk::log_info("No plugins directory found");
        return;
    }

    // C++23: use ranges for cleaner filtering
    auto plugin_files = fs::directory_iterator(plugin_dir)
        | std::views::filter([](const auto& entry) {
            return entry.is_regular_file() && entry.path().extension() == ".lua";
        })
        | std::views::transform([](const auto& entry) {
            return entry.path();
        })
        | std::ranges::to<std::vector>();

    std::ranges::sort(plugin_files, {}, &fs::path::filename);

    if (plugin_files.empty()) {
        sdk::log_info("No plugins found");
        return;
    }

    sdk::log_info(std::format("Loading {} plugins...", plugin_files.size()));

    for (const auto& path : plugin_files) {
        sdk::log_info(std::format("Loading plugin: {}", path.filename().string()));

        auto result = pimpl->lua.safe_script_file(path.string());
        if (!result.valid()) {
            sol::error err = result;
            throw std::runtime_error(std::format("Failed to load {}: {}",
                                      path.filename().string(), err.what()));
        }
    }

    g_ctx.cb.on_load.invoke();
    sdk::log_info("Plugins loaded");
}

void Engine::unload_plugins()
{
    g_ctx.cb.on_unload.invoke();

    g_ctx.cb.on_frame.clear();
    g_ctx.cb.on_overlay.clear();
    g_ctx.cb.on_key_down.clear();
    g_ctx.cb.on_gl_identity.clear();
    g_ctx.cb.on_glu_lookat.clear();
    g_ctx.cb.on_load.clear();
    g_ctx.cb.on_unload.clear();

    sdk::log_info("Plugins unloaded");
}

void Engine::execute(std::string_view code)
{
    auto result = pimpl->lua.safe_script(code);
    if (!result.valid()) {
        sol::error err = result;
        throw std::runtime_error(std::format("Script execution failed: {}", err.what()));
    }
}

void Engine::execute_file(const fs::path& path)
{
    if (!fs::exists(path)) {
        throw std::runtime_error(std::format("Script file not found: {}", path.string()));
    }

    auto result = pimpl->lua.safe_script_file(path.string());
    if (!result.valid()) {
        sol::error err = result;
        throw std::runtime_error(std::format("Script execution failed: {}", err.what()));
    }
}

} // namespace sdk::scripting
