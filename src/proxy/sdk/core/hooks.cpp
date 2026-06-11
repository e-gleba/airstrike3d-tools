#include "hooks.hpp"

#include "sdk/core/context.hpp"
#include "sdk/core/logging.hpp"
#include "sdk/lua/lua_state.hpp"
#include "sdk/render/render_hooks.hpp"

#include <format>
#include <memory>

namespace sdk
{

static std::unique_ptr<render::HookSystem> g_render;
static std::unique_ptr<lua::LuaState>      g_lua_state;

void install_hooks()
{
    sdk::log_info("initializing subsystems...");

    // 1. Render hooks (GL, DX detection, overlay, wndproc)
    g_render = std::make_unique<render::HookSystem>();
    g_render->install();

    // 2. Lua state
    try
    {
        sdk::log_info("creating Lua state...");
        g_lua_state = std::make_unique<lua::LuaState>(*g_render);
        sdk::log_info("Lua state created");

        sdk::log_info("loading plugins...");
        g_lua_state->load_plugins();
        sdk::log_info("plugins loaded");
    }
    catch (const std::exception& e)
    {
        sdk::log_error(std::format("Lua initialization failed: {}", e.what()));
        g_lua_state.reset();
    }
    catch (...)
    {
        sdk::log_error("Lua initialization failed: unknown exception");
        g_lua_state.reset();
    }
}

void uninstall_hooks()
{
    sdk::log_info("uninstalling...");

    if (g_lua_state)
    {
        g_lua_state->unload_plugins();
        g_lua_state.reset();
    }

    if (g_render)
    {
        g_render->uninstall();
        g_render.reset();
    }

    sdk::log_info("shutdown complete");
}

} // namespace sdk
