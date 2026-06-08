/// @file detail/lua_engine.cpp
/// @brief LuaState implementation using sol2.
///
/// This is the **only** file that owns the sol::state instance.
/// All sol2-specific code is isolated here.

#include "sdk/lua/lua_state.hpp"
#include "sdk/lua/callback.hpp"
#include "sdk/core/context.hpp"
#include "sdk/lua/bindings/math.hpp"
#include "sdk/lua/bindings/constants.hpp"
#include "sdk/lua/bindings/sdk.hpp"
#include "sdk/lua/bindings/ui.hpp"

#include <sol/sol.hpp>
#include <spdlog/spdlog.h>

#include <filesystem>

namespace fs = std::filesystem;

namespace sdk::lua
{

// ── impl ─────────────────────────────────────────────────────────────────────

struct LuaState::impl
{
    sol::state lua;
    
    impl()
    {
        // Open standard libraries
        lua.open_libraries(
            sol::lib::base,
            sol::lib::math,
            sol::lib::string,
            sol::lib::table,
            sol::lib::io,
            sol::lib::os
        );
        
        // Register bindings
        register_math_bindings();
        register_constants();
        register_sdk_bindings();
        register_ui_bindings();
        
        // Register callback hooks
        lua["hook_frame"] = [this](sol::protected_function fn) {
            g_ctx.cb.on_frame.add(wrap_void(std::move(fn)));
        };
        
        lua["hook_overlay"] = [this](sol::protected_function fn) {
            g_ctx.cb.on_overlay.add(wrap_void(std::move(fn)));
        };
        
        lua["hook_key_down"] = [this](sol::protected_function fn) {
            g_ctx.cb.on_key_down.add(wrap_bool(std::move(fn)));
        };
        
        lua["hook_gl_identity"] = [this](sol::protected_function fn) {
            g_ctx.cb.on_gl_identity.add(wrap_gl_identity(std::move(fn)));
        };
        
        lua["hook_glu_lookat"] = [this](sol::protected_function fn) {
            g_ctx.cb.on_glu_lookat.add(wrap_glu_lookat(std::move(fn)));
        };
        
        lua["hook_load"] = [this](sol::protected_function fn) {
            g_ctx.cb.on_load.add(wrap_void(std::move(fn)));
        };
        
        lua["hook_unload"] = [this](sol::protected_function fn) {
            g_ctx.cb.on_unload.add(wrap_void(std::move(fn)));
        };
        
        spdlog::info("[sdk] Lua interpreter initialized");
    }
    
    // ── Callback adapters ────────────────────────────────────────────────────
    
    auto wrap_void(sol::protected_function fn) -> callback_list<>::slot_fn
    {
        return [fn = std::move(fn)]() {
            auto result = fn();
            if (!result.valid()) {
                sol::error err = result;
                spdlog::error("Lua callback error: {}", err.what());
            }
        };
    }
    
    auto wrap_bool(sol::protected_function fn) -> consuming_callback_list<int>::slot_fn
    {
        return [fn = std::move(fn)](int key) -> bool {
            auto result = fn(key);
            if (!result.valid()) {
                sol::error err = result;
                spdlog::error("Lua callback error: {}", err.what());
                return false;
            }
            return result.get_type() == sol::type::boolean && result.get<bool>();
        };
    }
    
    auto wrap_gl_identity(sol::protected_function fn) -> callback_list<GLenum>::slot_fn
    {
        return [fn = std::move(fn)](GLenum mode) {
            auto result = fn(mode);
            if (!result.valid()) {
                sol::error err = result;
                spdlog::error("Lua callback error: {}", err.what());
            }
        };
    }
    
    auto wrap_glu_lookat(sol::protected_function fn) 
        -> consuming_callback_list<double, double, double, double, double, double, double, double, double>::slot_fn
    {
        return [fn = std::move(fn)](double eyeX, double eyeY, double eyeZ,
                                     double centerX, double centerY, double centerZ,
                                     double upX, double upY, double upZ) -> bool {
            auto result = fn(eyeX, eyeY, eyeZ, centerX, centerY, centerZ, upX, upY, upZ);
            if (!result.valid()) {
                sol::error err = result;
                spdlog::error("Lua callback error: {}", err.what());
                return false;
            }
            return result.get_type() == sol::type::boolean && result.get<bool>();
        };
    }
    
    // ── Binding registration ─────────────────────────────────────────────────
    
    void register_math_bindings()
    {
        auto math_table = lua.create_named_table("math");
        
        math_table.set_function("radians", &bindings::math::radians);
        math_table.set_function("cos", &bindings::math::cos);
        math_table.set_function("sin", &bindings::math::sin);
        math_table.set_function("mod", &bindings::math::mod);
        math_table.set_function("clamp", &bindings::math::clamp);
        
        // Vector operations - wrap to return tuples for Lua
        math_table.set_function("normalize", [](double x, double y, double z) {
            auto v = bindings::math::normalize(x, y, z);
            return std::make_tuple(v.x, v.y, v.z);
        });
        
        math_table.set_function("cross", [](double ax, double ay, double az,
                                             double bx, double by, double bz) {
            auto v = bindings::math::cross(ax, ay, az, bx, by, bz);
            return std::make_tuple(v.x, v.y, v.z);
        });
        
        math_table.set_function("lookat_matrix", [](double ex, double ey, double ez,
                                                      double cx, double cy, double cz,
                                                      double ux, double uy, double uz) {
            return bindings::math::lookat_matrix(ex, ey, ez, cx, cy, cz, ux, uy, uz);
        });
    }
    
    void register_constants()
    {
        // Virtual key constants
        auto vk_table = lua.create_named_table("VK");
        
        vk_table.set_function("SHIFT", &bindings::constants::vk_shift);
        vk_table.set_function("CONTROL", &bindings::constants::vk_control);
        vk_table.set_function("SPACE", &bindings::constants::vk_space);
        vk_table.set_function("INSERT", &bindings::constants::vk_insert);
        vk_table.set_function("ESCAPE", &bindings::constants::vk_escape);
        vk_table.set_function("TAB", &bindings::constants::vk_tab);
        vk_table.set_function("RETURN", &bindings::constants::vk_return);
        vk_table.set_function("BACK", &bindings::constants::vk_back);
        vk_table.set_function("DELETE", &bindings::constants::vk_delete);
        vk_table.set_function("HOME", &bindings::constants::vk_home);
        vk_table.set_function("END", &bindings::constants::vk_end);
        vk_table.set_function("PRIOR", &bindings::constants::vk_prior);
        vk_table.set_function("NEXT", &bindings::constants::vk_next);
        vk_table.set_function("LEFT", &bindings::constants::vk_left);
        vk_table.set_function("RIGHT", &bindings::constants::vk_right);
        vk_table.set_function("UP", &bindings::constants::vk_up);
        vk_table.set_function("DOWN", &bindings::constants::vk_down);
        
        // OpenGL constants
        auto gl_table = lua.create_named_table("GL");
        
        gl_table.set_function("MODELVIEW", &bindings::constants::gl_modelview);
        gl_table.set_function("PROJECTION", &bindings::constants::gl_projection);
        gl_table.set_function("TEXTURE", &bindings::constants::gl_texture);
        gl_table.set_function("DEPTH_TEST", &bindings::constants::gl_depth_test);
        gl_table.set_function("BLEND", &bindings::constants::gl_blend);
        gl_table.set_function("ALPHA_TEST", &bindings::constants::gl_alpha_test);
        gl_table.set_function("CULL_FACE", &bindings::constants::gl_cull_face);
        gl_table.set_function("LIGHTING", &bindings::constants::gl_lighting);
        gl_table.set_function("FOG", &bindings::constants::gl_fog);
        gl_table.set_function("FRONT", &bindings::constants::gl_front);
        gl_table.set_function("BACK", &bindings::constants::gl_back);
        gl_table.set_function("FRONT_AND_BACK", &bindings::constants::gl_front_and_back);
        gl_table.set_function("SRC_ALPHA", &bindings::constants::gl_src_alpha);
        gl_table.set_function("ONE_MINUS_SRC_ALPHA", &bindings::constants::gl_one_minus_src_alpha);
        gl_table.set_function("ONE", &bindings::constants::gl_one);
        gl_table.set_function("ZERO", &bindings::constants::gl_zero);
        gl_table.set_function("LINES", &bindings::constants::gl_lines);
        gl_table.set_function("LINE_STRIP", &bindings::constants::gl_line_strip);
        gl_table.set_function("LINE_LOOP", &bindings::constants::gl_line_loop);
        gl_table.set_function("TRIANGLES", &bindings::constants::gl_triangles);
        gl_table.set_function("TRIANGLE_STRIP", &bindings::constants::gl_triangle_strip);
        gl_table.set_function("TRIANGLE_FAN", &bindings::constants::gl_triangle_fan);
        gl_table.set_function("QUADS", &bindings::constants::gl_quads);
        gl_table.set_function("POINTS", &bindings::constants::gl_points);
        gl_table.set_function("POLYGON", &bindings::constants::gl_polygon);
    }
    
    void register_sdk_bindings()
    {
        auto sdk_table = lua.create_named_table("sdk");
        
        // GL state management
        sdk_table.set_function("gl_enable", &bindings::sdk::gl_enable);
        sdk_table.set_function("gl_disable", &bindings::sdk::gl_disable);
        sdk_table.set_function("gl_depth_mask", &bindings::sdk::gl_depth_mask);
        sdk_table.set_function("gl_blend_func", &bindings::sdk::gl_blend_func);
        sdk_table.set_function("gl_line_width", &bindings::sdk::gl_line_width);
        sdk_table.set_function("gl_point_size", &bindings::sdk::gl_point_size);
        sdk_table.set_function("gl_color4f", &bindings::sdk::gl_color4f);
        sdk_table.set_function("gl_color3f", &bindings::sdk::gl_color3f);
        sdk_table.set_function("gl_polygon_mode", &bindings::sdk::gl_polygon_mode);
        sdk_table.set_function("gl_push_attrib", &bindings::sdk::gl_push_attrib);
        sdk_table.set_function("gl_pop_attrib", &bindings::sdk::gl_pop_attrib);
        sdk_table.set_function("gl_push_matrix", &bindings::sdk::gl_push_matrix);
        sdk_table.set_function("gl_pop_matrix", &bindings::sdk::gl_pop_matrix);
        sdk_table.set_function("gl_begin", &bindings::sdk::gl_begin);
        sdk_table.set_function("gl_end", &bindings::sdk::gl_end);
        sdk_table.set_function("gl_vertex3f", &bindings::sdk::gl_vertex3f);
        sdk_table.set_function("gl_vertex2f", &bindings::sdk::gl_vertex2f);
        sdk_table.set_function("gl_translate", &bindings::sdk::gl_translate);
        sdk_table.set_function("gl_rotate", &bindings::sdk::gl_rotate);
        sdk_table.set_function("gl_scale", &bindings::sdk::gl_scale);
        
        // Input
        sdk_table.set_function("is_key_down", &bindings::sdk::is_key_down);
        sdk_table.set_function("get_cursor_pos", []() {
            auto pos = bindings::sdk::get_cursor_pos();
            return std::make_tuple(pos.x, pos.y);
        });
        sdk_table.set_function("set_cursor_pos", &bindings::sdk::set_cursor_pos);
        sdk_table.set_function("show_cursor", &bindings::sdk::show_cursor);
        
        // Window
        sdk_table.set_function("get_window_rect", []() {
            auto rect = bindings::sdk::get_window_rect();
            return std::make_tuple(rect.left, rect.top, rect.right, rect.bottom);
        });
        
        // Logging
        sdk_table.set_function("log_info", &bindings::sdk::log_info);
        sdk_table.set_function("log_warn", &bindings::sdk::log_warn);
        sdk_table.set_function("log_error", &bindings::sdk::log_error);
        sdk_table.set_function("get_log_dir", &bindings::sdk::get_log_dir);
    }
    
    void register_ui_bindings()
    {
        auto ui_table = lua.create_named_table("ui");
        
        // Window management
        ui_table.set_function("begin_window", &bindings::ui::begin_window);
        ui_table.set_function("end_window", &bindings::ui::end_window);
        
        // Text rendering
        ui_table.set_function("text", &bindings::ui::text);
        ui_table.set_function("text_wrapped", &bindings::ui::text_wrapped);
        ui_table.set_function("text_disabled", &bindings::ui::text_disabled);
        ui_table.set_function("text_colored", &bindings::ui::text_colored);
        
        // Buttons
        ui_table.set_function("button", &bindings::ui::button);
        ui_table.set_function("button_sized", &bindings::ui::button_sized);
        
        // Input widgets - wrap to return value + changed flag
        ui_table.set_function("checkbox", [](const std::string& label, bool v) {
            bool changed = bindings::ui::checkbox(label, v);
            return std::make_tuple(v, changed);
        });
        
        ui_table.set_function("drag_float", [](const std::string& label, float v, 
                                                float spd, float mn, float mx) {
            bool changed = bindings::ui::drag_float(label, v, spd, mn, mx);
            return std::make_tuple(v, changed);
        });
        
        ui_table.set_function("slider_float", [](const std::string& label, float v, 
                                                  float mn, float mx) {
            bool changed = bindings::ui::slider_float(label, v, mn, mx);
            return std::make_tuple(v, changed);
        });
        
        ui_table.set_function("slider_int", [](const std::string& label, int v, 
                                                int mn, int mx) {
            bool changed = bindings::ui::slider_int(label, v, mn, mx);
            return std::make_tuple(v, changed);
        });
        
        ui_table.set_function("input_text", [](const std::string& label, std::string text) {
            bool changed = bindings::ui::input_text(label, text);
            return std::make_tuple(text, changed);
        });
        
        ui_table.set_function("color_edit3", [](const std::string& label, 
                                                 float r, float g, float b) {
            bool changed = bindings::ui::color_edit3(label, r, g, b);
            return std::make_tuple(r, g, b, changed);
        });
        
        // Layout
        ui_table.set_function("separator", &bindings::ui::separator);
        ui_table.set_function("same_line", &bindings::ui::same_line);
        ui_table.set_function("spacing", &bindings::ui::spacing);
        ui_table.set_function("tree_node", &bindings::ui::tree_node);
        ui_table.set_function("tree_pop", &bindings::ui::tree_pop);
        ui_table.set_function("tab_bar_begin", &bindings::ui::tab_bar_begin);
        ui_table.set_function("tab_bar_end", &bindings::ui::tab_bar_end);
        ui_table.set_function("tab_item_begin", &bindings::ui::tab_item_begin);
        ui_table.set_function("tab_item_end", &bindings::ui::tab_item_end);
        ui_table.set_function("collapsing_header", &bindings::ui::collapsing_header);
        
        // Positioning
        ui_table.set_function("set_next_window_pos", &bindings::ui::set_next_window_pos);
        ui_table.set_function("set_next_window_size", &bindings::ui::set_next_window_size);
        ui_table.set_function("set_cursor_pos_x", &bindings::ui::set_cursor_pos_x);
        ui_table.set_function("get_window_width", &bindings::ui::get_window_width);
        
        // Styling
        ui_table.set_function("push_style_color", &bindings::ui::push_style_color);
        ui_table.set_function("pop_style_color", &bindings::ui::pop_style_color);
        ui_table.set_function("push_style_var_float", &bindings::ui::push_style_var_float);
        ui_table.set_function("push_style_var_vec2", &bindings::ui::push_style_var_vec2);
        ui_table.set_function("pop_style_var", &bindings::ui::pop_style_var);
        
        // Columns
        ui_table.set_function("columns", &bindings::ui::columns);
        ui_table.set_function("next_column", &bindings::ui::next_column);
        ui_table.set_function("set_column_width", &bindings::ui::set_column_width);
        
        // Utilities
        ui_table.set_function("get_delta_time", &bindings::ui::get_delta_time);
        ui_table.set_function("get_framerate", &bindings::ui::get_framerate);
        ui_table.set_function("want_capture_keyboard", &bindings::ui::want_capture_keyboard);
        ui_table.set_function("want_capture_mouse", &bindings::ui::want_capture_mouse);
        ui_table.set_function("progress_bar", &bindings::ui::progress_bar);
        ui_table.set_function("tooltip", &bindings::ui::tooltip);
    }
};

// ── LuaState ─────────────────────────────────────────────────────────────────

LuaState::LuaState() : pimpl(std::make_unique<impl>()) {}

LuaState::~LuaState() = default;

LuaState::LuaState(LuaState&&) noexcept = default;
LuaState& LuaState::operator=(LuaState&&) noexcept = default;

void LuaState::load_plugins()
{
    auto plugin_dir = fs::current_path() / "plugins";
    
    if (!fs::exists(plugin_dir)) {
        spdlog::info("[sdk] No plugins directory found");
        return;
    }
    
    for (const auto& entry : fs::directory_iterator(plugin_dir)) {
        if (entry.is_regular_file() && entry.path().extension() == ".lua") {
            auto path = entry.path();
            spdlog::info("[sdk] Loading plugin: {}", path.filename().string());
            
            auto result = pimpl->lua.safe_script_file(path.string());
            if (!result.valid()) {
                sol::error err = result;
                spdlog::error("[sdk] Failed to load {}: {}", 
                              path.filename().string(), err.what());
            }
        }
    }
    
    g_ctx.cb.on_load.invoke();
    spdlog::info("[sdk] Plugins loaded");
}

void LuaState::unload_plugins()
{
    g_ctx.cb.on_unload.invoke();
    
    g_ctx.cb.on_frame.clear();
    g_ctx.cb.on_overlay.clear();
    g_ctx.cb.on_key_down.clear();
    g_ctx.cb.on_gl_identity.clear();
    g_ctx.cb.on_glu_lookat.clear();
    g_ctx.cb.on_load.clear();
    g_ctx.cb.on_unload.clear();
    
    spdlog::info("[sdk] Plugins unloaded");
}

} // namespace sdk::lua
