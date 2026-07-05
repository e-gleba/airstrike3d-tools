/// @file engine_lua.cpp
/// @brief Script engine implementation using LuaBridge3.
///
/// This file implements the scripting engine using LuaBridge3.
///
/// Key implementation details:
/// - Uses raw lua_State* with luaL_openlibs() for initialization
/// - luabridge::LuaRef for callback function references
/// - TypeResult<T> for error handling
/// - 2.6× faster Lua→C++ calls compared to previous sol2 backend
/// - Raw Lua C API used for constant tables (addVariable incompatible with constexpr fns)

#include "sdk/scripting/engine.hpp"

#include "sdk/core/context.hpp"
#include "sdk/core/contract.hpp"
#include "sdk/core/logging.hpp"
#include "sdk/graphics/graphics.hpp"
#include "sdk/math/math.hpp"
#include "sdk/platform/platform.hpp"
#include "sdk/scripting/callback.hpp"
#include "sdk/ui/ui.hpp"

// Lua C headers MUST precede LuaBridge (enforced by LuaBridge3 Config.h:34).
extern "C"
{
#include <lauxlib.h>
#include <lua.h>
#include <lualib.h>
}

#include <LuaBridge/LuaBridge.h>

#include <array>
#include <cstdint>
#include <filesystem>
#include <format>
#include <ranges>
#include <span>
#include <stdexcept>
#include <vector>

namespace fs = std::filesystem;

namespace sdk::scripting
{

/// @brief Private implementation of the scripting engine (LuaBridge3 backend).
///
/// Owns the lua_State* and provides RAII cleanup. All Lua API registration
/// happens in the constructor. Callback wrappers convert luabridge::LuaRef
/// to std::function for the callback_list system.
struct engine::impl final
{
    lua_State* lua;

    /// @brief Initialize Lua state and register all bindings.
    /// @throws std::runtime_error if lua_State creation fails.
    impl() : lua{ luaL_newstate() }
    {
        require(lua != nullptr, "failed to create Lua state");

        luaL_openlibs(lua);

        register_math_bindings();
        register_constants();
        register_sdk_bindings();
        register_ui_bindings();
        register_callback_hooks();

        sdk::log_info("script engine initialized (LuaBridge3 backend)");
    }

    ~impl()
    {
        if (lua != nullptr)
        {
            lua_close(lua);
        }
    }

    // Non-copyable
    impl(const impl&) = delete;
    impl& operator=(const impl&) = delete;

    // Movable
    impl(impl&& other) noexcept : lua{ other.lua } { other.lua = nullptr; }

    impl& operator=(impl&& other) noexcept
    {
        if (this != &other)
        {
            if (lua != nullptr)
            {
                lua_close(lua);
            }
            lua = other.lua;
            other.lua = nullptr;
        }
        return *this;
    }

    /// @brief Wrap a Lua function as a void callback.
    /// @param fn Lua function reference (must be callable).
    /// @return std::function suitable for callback_list<>.
    [[nodiscard]] auto wrap_void(luabridge::LuaRef fn) -> callback_list<>::slot_fn
    {
        return [fn = std::move(fn)]() {
            auto result = fn();
            if (!result)
            {
                sdk::log_error(std::format("script callback error: {}", result.message()));
            }
        };
    }

    /// @brief Wrap a Lua function as a bool-returning callback (key handler).
    /// @param fn Lua function that takes int32 key and returns bool.
    /// @return std::function suitable for consuming_callback_list<int32_t>.
    [[nodiscard]] auto wrap_bool(luabridge::LuaRef fn)
        -> consuming_callback_list<std::int32_t>::slot_fn
    {
        return [fn = std::move(fn)](std::int32_t key) -> bool {
            auto result = fn(key);
            if (!result)
            {
                sdk::log_error(std::format("script callback error: {}", result.message()));
                return false;
            }

            auto value = result.template cast<bool>();
            return value.value_or(false);
        };
    }

    /// @brief Wrap a Lua function as a matrix_mode callback.
    /// @param fn Lua function that takes int32 mode parameter.
    /// @return std::function suitable for callback_list<matrix_mode>.
    [[nodiscard]] auto wrap_gl_identity(luabridge::LuaRef fn)
        -> callback_list<matrix_mode>::slot_fn
    {
        return [fn = std::move(fn)](matrix_mode mode) {
            auto result = fn(static_cast<std::int32_t>(mode));
            if (!result)
            {
                sdk::log_error(std::format("script callback error: {}", result.message()));
            }
        };
    }

    /// @brief Wrap a Lua function as a gluLookAt callback.
    /// @param fn Lua function taking 9 doubles (eye, center, up vectors) returning bool.
    /// @return std::function suitable for consuming_callback_list with 9 double params.
    [[nodiscard]] auto wrap_glu_lookat(luabridge::LuaRef fn)
        -> consuming_callback_list<double, double, double,
                                    double, double, double,
                                    double, double, double>::slot_fn
    {
        return [fn = std::move(fn)](double eyeX, double eyeY, double eyeZ,
                                     double centerX, double centerY, double centerZ,
                                     double upX, double upY, double upZ) -> bool {
            auto result = fn(eyeX, eyeY, eyeZ, centerX, centerY, centerZ,
                             upX, upY, upZ);
            if (!result)
            {
                sdk::log_error(std::format("script callback error: {}", result.message()));
                return false;
            }

            auto value = result.template cast<bool>();
            return value.value_or(false);
        };
    }

    /// @brief Register Lua callback hooks (sdk.on_frame, sdk.on_key_down, etc.).
    ///
    /// These functions accept a Lua function and wrap it with the appropriate
    /// callback wrapper before adding it to the global callback lists.
    void register_callback_hooks()
    {
        luabridge::getGlobalNamespace(lua)
            .beginNamespace("sdk")
                .addFunction("on_frame", [this](luabridge::LuaRef fn) {
                    g_ctx.cb.on_frame.add(wrap_void(std::move(fn)));
                })
                .addFunction("on_overlay", [this](luabridge::LuaRef fn) {
                    g_ctx.cb.on_overlay.add(wrap_void(std::move(fn)));
                })
                .addFunction("on_key_down", [this](luabridge::LuaRef fn) {
                    g_ctx.cb.on_key_down.add(wrap_bool(std::move(fn)));
                })
                .addFunction("on_gl_identity", [this](luabridge::LuaRef fn) {
                    g_ctx.cb.on_gl_identity.add(wrap_gl_identity(std::move(fn)));
                })
                .addFunction("on_glu_lookat", [this](luabridge::LuaRef fn) {
                    g_ctx.cb.on_glu_lookat.add(wrap_glu_lookat(std::move(fn)));
                })
                .addFunction("on_load", [this](luabridge::LuaRef fn) {
                    g_ctx.cb.on_load.add(wrap_void(std::move(fn)));
                })
                .addFunction("on_unload", [this](luabridge::LuaRef fn) {
                    g_ctx.cb.on_unload.add(wrap_void(std::move(fn)));
                })
            .endNamespace();
    }

    /// @brief Register math functions (gmath.radians, gmath.cos, etc.).
    void register_math_bindings()
    {
        using namespace sdk::math;

        luabridge::getGlobalNamespace(lua)
            .beginNamespace("gmath")
                .addFunction("radians", &radians)
                .addFunction("cos", &cos)
                .addFunction("sin", &sin)
                .addFunction("mod", &mod)
                .addFunction("clamp", &clamp)
                .addFunction("normalize", [](double x, double y, double z) {
                    const auto v = normalize(x, y, z);
                    return std::make_tuple(v.x, v.y, v.z);
                })
                .addFunction("cross", [](double ax, double ay, double az,
                                         double bx, double by, double bz) {
                    const auto v = cross(ax, ay, az, bx, by, bz);
                    return std::make_tuple(v.x, v.y, v.z);
                })
                .addFunction("lookat_matrix",
                             [](double ex, double ey, double ez,
                                double cx, double cy, double cz,
                                double ux, double uy, double uz) {
                                 return lookat_matrix(ex, ey, ez, cx, cy, cz, ux, uy, uz);
                             })
            .endNamespace();
    }

    /// @brief Register VK_* and GL_* constants using raw Lua C API.
    ///
    /// LuaBridge3's addVariable() requires a variable pointer or getter/setter pair,
    /// but our constants are constexpr functions. Using raw Lua C API to build tables
    /// with integer values matches the previous behavior exactly (vk["SHIFT"] = value).
    void register_constants()
    {
        using namespace sdk::graphics::constants;

        // Helper: set integer field on table at top of stack
        auto set_int = [this](const char* name, std::int32_t value) {
            lua_pushinteger(lua, value);
            lua_setfield(lua, -2, name);
        };

        // VK table: virtual key constants
        lua_newtable(lua);
        set_int("SHIFT",   vk_shift());
        set_int("CONTROL", vk_control());
        set_int("SPACE",   vk_space());
        set_int("INSERT",  vk_insert());
        set_int("ESCAPE",  vk_escape());
        set_int("TAB",     vk_tab());
        set_int("RETURN",  vk_return());
        set_int("BACK",    vk_back());
        set_int("DELETE",  vk_delete());
        set_int("HOME",    vk_home());
        set_int("END",     vk_end());
        set_int("PRIOR",   vk_prior());
        set_int("NEXT",    vk_next());
        set_int("LEFT",    vk_left());
        set_int("RIGHT",   vk_right());
        set_int("UP",      vk_up());
        set_int("DOWN",    vk_down());
        set_int("F1",      vk_f1());
        set_int("F2",      vk_f2());
        set_int("F3",      vk_f3());
        set_int("F4",      vk_f4());
        set_int("F5",      vk_f5());
        set_int("F6",      vk_f6());
        set_int("F7",      vk_f7());
        set_int("F8",      vk_f8());
        set_int("F9",      vk_f9());
        set_int("F10",     vk_f10());
        set_int("F11",     vk_f11());
        set_int("F12",     vk_f12());
        set_int("LBUTTON", vk_lbutton());
        set_int("RBUTTON", vk_rbutton());
        set_int("MBUTTON", vk_mbutton());
        set_int("W",       vk_w());
        set_int("A",       vk_a());
        set_int("S",       vk_s());
        set_int("D",       vk_d());
        set_int("Q",       vk_q());
        set_int("E",       vk_e());
        set_int("C",       vk_c());
        set_int("R",       vk_r());
        set_int("Z",       vk_z());
        set_int("X",       vk_x());
        set_int("V",       vk_v());
        lua_setglobal(lua, "VK");

        // GL table: OpenGL constants
        lua_newtable(lua);
        set_int("MODELVIEW",           gl_modelview());
        set_int("PROJECTION",          gl_projection());
        set_int("TEXTURE",             gl_texture());
        set_int("DEPTH_TEST",          gl_depth_test());
        set_int("BLEND",               gl_blend());
        set_int("ALPHA_TEST",          gl_alpha_test());
        set_int("CULL_FACE",           gl_cull_face());
        set_int("LIGHTING",            gl_lighting());
        set_int("FOG",                 gl_fog());
        set_int("TEXTURE_2D",          gl_texture_2d());
        set_int("FRONT",               gl_front());
        set_int("BACK",                gl_back());
        set_int("FRONT_AND_BACK",      gl_front_and_back());
        set_int("SRC_ALPHA",           gl_src_alpha());
        set_int("ONE_MINUS_SRC_ALPHA", gl_one_minus_src_alpha());
        set_int("ONE",                 gl_one());
        set_int("ZERO",                gl_zero());
        set_int("LINES",               gl_lines());
        set_int("LINE_STRIP",          gl_line_strip());
        set_int("LINE_LOOP",           gl_line_loop());
        set_int("TRIANGLES",           gl_triangles());
        set_int("TRIANGLE_STRIP",      gl_triangle_strip());
        set_int("TRIANGLE_FAN",        gl_triangle_fan());
        set_int("QUADS",               gl_quads());
        set_int("POINTS",              gl_points());
        set_int("POLYGON",             gl_polygon());
        set_int("LINE",                gl_line());
        set_int("FILL",                gl_fill());
        set_int("ALL_ATTRIB_BITS",     gl_all_attrib_bits());
        lua_setglobal(lua, "GL");
    }

    /// @brief Register SDK graphics and platform functions (sdk.gl_*, sdk.is_key_down, etc.).
    void register_sdk_bindings()
    {
        using namespace sdk::graphics;
        using namespace sdk::platform;

        luabridge::getGlobalNamespace(lua)
            .beginNamespace("sdk")
                // OpenGL state functions
                .addFunction("gl_enable", &enable)
                .addFunction("gl_disable", &disable)
                .addFunction("gl_depth_mask", &depth_mask)
                .addFunction("gl_blend_func", &blend_func)
                .addFunction("gl_line_width", &line_width)
                .addFunction("gl_point_size", &point_size)
                .addFunction("gl_color4f", &color4f)
                .addFunction("gl_color3f", &color3f)
                .addFunction("gl_polygon_mode", &polygon_mode)
                // Matrix stack
                .addFunction("gl_push_attrib", &push_attrib)
                .addFunction("gl_pop_attrib", &pop_attrib)
                .addFunction("gl_push_matrix", &push_matrix)
                .addFunction("gl_pop_matrix", &pop_matrix)
                // Immediate mode rendering
                .addFunction("gl_begin", &begin)
                .addFunction("gl_end", &end)
                .addFunction("gl_vertex3f", &vertex3f)
                .addFunction("gl_vertex2f", &vertex2f)
                // Transformations
                .addFunction("gl_translate", &translate)
                .addFunction("gl_rotate", &rotate)
                .addFunction("gl_scale", &scale)
                .addFunction("gl_mult_matrix_d", [](std::vector<double> m) {
                    require(m.size() >= 16,
                            "gl_mult_matrix_d: expected at least 16 elements");
                    std::array<double, 16> matrix{};
                    std::copy_n(m.begin(), 16, matrix.begin());
                    mult_matrix(std::span<const double, 16>{ matrix });
                })
                .addFunction("gl_apply_lookat", &apply_lookat)
                // Input
                .addFunction("is_key_down", &is_key_down)
                .addFunction("get_cursor_pos", []() {
                    const auto pos = get_cursor_pos();
                    return std::make_tuple(pos.x, pos.y);
                })
                .addFunction("set_cursor_pos", &set_cursor_pos)
                .addFunction("show_cursor", &show_cursor)
                // Window
                .addFunction("get_window_rect", []() {
                    const auto r = get_window_rect();
                    return std::make_tuple(r.left, r.top, r.right, r.bottom);
                })
                // Text input
                .addFunction("send_chars", &send_chars)
                // Logging
                .addFunction("log_info",
                             [](const std::string& m) { sdk::platform::log_info(m); })
                .addFunction("log_warn",
                             [](const std::string& m) { sdk::platform::log_warn(m); })
                .addFunction("log_error",
                             [](const std::string& m) { sdk::platform::log_error(m); })
                .addFunction("get_log_dir", []() -> std::string {
                    return std::string{ sdk::platform::get_log_dir() };
                })
            .endNamespace();
    }

    /// @brief Register ImGui UI functions (ui.begin_window, ui.checkbox, etc.).
    ///
    /// Functions that modify values in-place (checkbox, slider, etc.) return
    /// tuples with both the new value and a 'changed' flag.
    void register_ui_bindings()
    {
        using namespace sdk::ui;

        luabridge::getGlobalNamespace(lua)
            .beginNamespace("ui")
                // Window management
                .addFunction("begin_window", &begin_window)
                .addFunction("end_window", &end_window)
                // Text widgets
                .addFunction("text", &text)
                .addFunction("text_wrapped", &text_wrapped)
                .addFunction("text_disabled", &text_disabled)
                .addFunction("text_colored", &text_colored)
                // Buttons
                .addFunction("button", &button)
                .addFunction("button_sized", &button_sized)
                // Input widgets (return {value, changed} tuples)
                .addFunction("checkbox", [](const std::string& label, bool v) {
                    const bool changed = checkbox(label, v);
                    return std::make_tuple(v, changed);
                })
                .addFunction("drag_float",
                             [](const std::string& label, float v,
                                float spd, float mn, float mx) {
                                 const bool changed = drag_float(label, v, spd, mn, mx);
                                 return std::make_tuple(v, changed);
                             })
                .addFunction("slider_float",
                             [](const std::string& label, float v, float mn, float mx) {
                                 const bool changed = slider_float(label, v, mn, mx);
                                 return std::make_tuple(v, changed);
                             })
                .addFunction("slider_int",
                             [](const std::string& label, std::int32_t v,
                                std::int32_t mn, std::int32_t mx) {
                                 const bool changed = slider_int(label, v, mn, mx);
                                 return std::make_tuple(v, changed);
                             })
                .addFunction("input_text",
                             [](const std::string& label, std::string text) {
                                 const bool changed = input_text(label, text);
                                 return std::make_tuple(text, changed);
                             })
                .addFunction("color_edit3",
                             [](const std::string& label, float r, float g, float b) {
                                 const bool changed = color_edit3(label, r, g, b);
                                 return std::make_tuple(r, g, b, changed);
                             })
                // Layout
                .addFunction("separator", &separator)
                .addFunction("same_line", &same_line)
                .addFunction("spacing", &spacing)
                // Tree widgets
                .addFunction("tree_node", &tree_node)
                .addFunction("tree_pop", &tree_pop)
                // Tab widgets
                .addFunction("tab_bar_begin", &tab_bar_begin)
                .addFunction("tab_bar_end", &tab_bar_end)
                .addFunction("tab_item_begin", &tab_item_begin)
                .addFunction("tab_item_end", &tab_item_end)
                // Collapsing headers
                .addFunction("collapsing_header", &collapsing_header)
                // Groups
                .addFunction("begin_group", &begin_group)
                .addFunction("end_group", &end_group)
                // Window positioning
                .addFunction("set_next_window_pos", &set_next_window_pos)
                .addFunction("set_next_window_size", &set_next_window_size)
                .addFunction("set_cursor_pos_x", &set_cursor_pos_x)
                .addFunction("get_window_width", &get_window_width)
                // Style
                .addFunction("push_style_color", &push_style_color)
                .addFunction("pop_style_color", &pop_style_color)
                .addFunction("push_style_var_float", &push_style_var_float)
                .addFunction("push_style_var_vec2", &push_style_var_vec2)
                .addFunction("pop_style_var", &pop_style_var)
                // Columns
                .addFunction("columns", &columns)
                .addFunction("next_column", &next_column)
                .addFunction("set_column_width", &set_column_width)
                // Timing
                .addFunction("get_delta_time", &get_delta_time)
                .addFunction("get_framerate", &get_framerate)
                // Input capture
                .addFunction("want_capture_keyboard", &want_capture_keyboard)
                .addFunction("want_capture_mouse", &want_capture_mouse)
                // Misc
                .addFunction("progress_bar", &progress_bar)
                .addFunction("tooltip", &tooltip)
            .endNamespace();
    }
};

engine::engine()
{
    try
    {
        pimpl_ = std::make_unique<impl>();
    }
    catch (const std::exception& e)
    {
        throw std::runtime_error(
            std::format("scripting::engine: initialization failed: {}", e.what()));
    }
}

engine::~engine() = default;

engine::engine(engine&&) noexcept = default;
engine& engine::operator=(engine&&) noexcept = default;

void engine::require_active() const
{
    require(pimpl_ != nullptr, "scripting::engine: operation on moved-from engine");
}

void engine::load_plugins()
{
    require_active();

    const auto plugin_dir = fs::current_path() / std::string{ k_plugin_dir };

    if (!fs::exists(plugin_dir))
    {
        sdk::log_info("no plugins directory found");
        return;
    }

    std::vector<fs::path> plugin_files;
    try
    {
        plugin_files =
            fs::directory_iterator(plugin_dir)
            | std::views::filter([](const fs::directory_entry& entry) {
                  return entry.is_regular_file() && entry.path().extension() == ".lua";
              })
            | std::views::transform([](const fs::directory_entry& entry) {
                  return entry.path();
              })
            | std::ranges::to<std::vector<fs::path>>();
    }
    catch (const fs::filesystem_error& e)
    {
        throw std::runtime_error(
            std::format("scripting::engine::load_plugins: {}", e.what()));
    }

    std::ranges::sort(plugin_files, {}, &fs::path::filename);

    if (plugin_files.empty())
    {
        sdk::log_info("no plugins found");
        return;
    }

    sdk::log_info(std::format("loading {} plugins...", plugin_files.size()));

    std::ranges::for_each(plugin_files, [this](const fs::path& path) {
        sdk::log_info(std::format("loading plugin: {}", path.filename().string()));

        // Load and compile the script
        if (luaL_loadfile(pimpl_->lua, path.string().c_str()) != LUA_OK)
        {
            const char* err = lua_tostring(pimpl_->lua, -1);
            sdk::log_error(std::format("failed to load {}: {}",
                                       path.filename().string(),
                                       err ? err : "unknown error"));
            lua_pop(pimpl_->lua, 1);
            return;
        }

        // Execute the script
        if (lua_pcall(pimpl_->lua, 0, LUA_MULTRET, 0) != LUA_OK)
        {
            const char* err = lua_tostring(pimpl_->lua, -1);
            sdk::log_error(std::format("failed to execute {}: {}",
                                       path.filename().string(),
                                       err ? err : "unknown error"));
            lua_pop(pimpl_->lua, 1);
        }
    });

    g_ctx.cb.on_load.invoke();
    sdk::log_info("plugins loaded");
}

void engine::unload_plugins()
{
    require_active();
    g_ctx.cb.on_unload.invoke();

    g_ctx.cb.on_frame.clear();
    g_ctx.cb.on_overlay.clear();
    g_ctx.cb.on_key_down.clear();
    g_ctx.cb.on_gl_identity.clear();
    g_ctx.cb.on_glu_lookat.clear();
    g_ctx.cb.on_load.clear();
    g_ctx.cb.on_unload.clear();

    sdk::log_info("plugins unloaded");
}

} // namespace sdk::scripting
