/// @file detail/lua_engine.cpp
/// @brief LuaState implementation using sol2.
///
/// This is the **only** file that owns the sol::state instance.
/// All sol2-specific code is isolated here.

#include "sdk/lua/lua_state.hpp"
#include "sdk/lua/callback.hpp"
#include "sdk/core/context.hpp"
#include "sdk/core/logging.hpp"
#include "sdk/render/render_hooks.hpp"
#include "sdk/lua/bindings/math.hpp"
#include "sdk/lua/bindings/constants.hpp"
#include "sdk/lua/bindings/sdk.hpp"
#include "sdk/lua/bindings/ui.hpp"

#include <sol/sol.hpp>

#include <algorithm>
#include <filesystem>
#include <format>

namespace fs = std::filesystem;

namespace sdk::lua
{

// ── impl ─────────────────────────────────────────────────────────────────────

struct LuaState::impl
{
    sol::state lua;
    render::HookSystem& render;

    explicit impl(render::HookSystem& r) : render(r)
    {
        lua.open_libraries(
            sol::lib::base,
            sol::lib::math,
            sol::lib::string,
            sol::lib::table,
            sol::lib::io,
            sol::lib::os
        );

        register_math_bindings();
        register_constants();
        register_sdk_bindings();
        register_ui_bindings();
        register_callback_hooks();

        sdk::log_info("Lua interpreter initialized");
    }

    // ── Callback adapters ────────────────────────────────────────────────────

    auto wrap_void(sol::protected_function fn) -> std::function<void()>
    {
        return [fn = std::move(fn)]() {
            auto result = fn();
            if (!result.valid()) {
                sol::error err = result;
                sdk::log_error(std::format("Lua callback error: {}", err.what()));
            }
        };
    }

    auto wrap_bool(sol::protected_function fn) -> std::function<bool(int)>
    {
        return [fn = std::move(fn)](int key) -> bool {
            auto result = fn(key);
            if (!result.valid()) {
                sol::error err = result;
                sdk::log_error(std::format("Lua callback error: {}", err.what()));
                return false;
            }
            return result.get_type() == sol::type::boolean && result.get<bool>();
        };
    }

    auto wrap_gl_identity(sol::protected_function fn) -> std::function<void(uint32_t)>
    {
        return [fn = std::move(fn)](uint32_t mode) {
            auto result = fn(mode);
            if (!result.valid()) {
                sol::error err = result;
                sdk::log_error(std::format("Lua callback error: {}", err.what()));
            }
        };
    }

    auto wrap_glu_lookat(sol::protected_function fn)
        -> std::function<bool(double, double, double,
                              double, double, double,
                              double, double, double)>
    {
        return [fn = std::move(fn)](double eyeX, double eyeY, double eyeZ,
                                     double centerX, double centerY, double centerZ,
                                     double upX, double upY, double upZ) -> bool {
            auto result = fn(eyeX, eyeY, eyeZ, centerX, centerY, centerZ,
                             upX, upY, upZ);
            if (!result.valid()) {
                sol::error err = result;
                sdk::log_error(std::format("Lua callback error: {}", err.what()));
                return false;
            }
            return result.get_type() == sol::type::boolean && result.get<bool>();
        };
    }

    // ── Callback hook registration ───────────────────────────────────────────

    void register_callback_hooks()
    {
        auto sdk_table = lua["sdk"];

        sdk_table["on_frame"] = [this](sol::protected_function fn) {
            render.on_frame(wrap_void(std::move(fn)));
        };

        sdk_table["on_overlay"] = [this](sol::protected_function fn) {
            render.on_overlay(wrap_void(std::move(fn)));
        };

        sdk_table["on_key_down"] = [this](sol::protected_function fn) {
            render.on_key_down(wrap_bool(std::move(fn)));
        };

        sdk_table["on_gl_identity"] = [this](sol::protected_function fn) {
            render.on_gl_identity(wrap_gl_identity(std::move(fn)));
        };

        sdk_table["on_glu_lookat"] = [this](sol::protected_function fn) {
            render.on_glu_lookat(wrap_glu_lookat(std::move(fn)));
        };

        sdk_table["on_load"] = [this](sol::protected_function fn) {
            g_ctx.cb.on_load.add(wrap_void(std::move(fn)));
        };

        sdk_table["on_unload"] = [this](sol::protected_function fn) {
            g_ctx.cb.on_unload.add(wrap_void(std::move(fn)));
        };
    }

    // ── Binding registration ─────────────────────────────────────────────────

    void register_math_bindings()
    {
        auto gmath = lua.create_named_table("gmath");

        gmath.set_function("radians", &bindings::math::radians);
        gmath.set_function("cos", &bindings::math::cos);
        gmath.set_function("sin", &bindings::math::sin);
        gmath.set_function("mod", &bindings::math::mod);
        gmath.set_function("clamp", &bindings::math::clamp);

        gmath.set_function("normalize", [](double x, double y, double z) {
            auto v = bindings::math::normalize(x, y, z);
            return std::make_tuple(v.x, v.y, v.z);
        });

        gmath.set_function("cross", [](double ax, double ay, double az,
                                        double bx, double by, double bz) {
            auto v = bindings::math::cross(ax, ay, az, bx, by, bz);
            return std::make_tuple(v.x, v.y, v.z);
        });

        gmath.set_function("lookat_matrix",
            [](double ex, double ey, double ez,
               double cx, double cy, double cz,
               double ux, double uy, double uz) {
                return bindings::math::lookat_matrix(
                    ex, ey, ez, cx, cy, cz, ux, uy, uz);
            });
    }

    void register_constants()
    {
        using namespace bindings::constants;

        auto vk = lua.create_named_table("VK");

        vk["SHIFT"]    = vk_shift();
        vk["CONTROL"]  = vk_control();
        vk["SPACE"]    = vk_space();

        vk["INSERT"]   = vk_insert();
        vk["ESCAPE"]   = vk_escape();
        vk["TAB"]      = vk_tab();
        vk["RETURN"]   = vk_return();
        vk["BACK"]     = vk_back();
        vk["DELETE"]   = vk_delete();
        vk["HOME"]     = vk_home();
        vk["END"]      = vk_end();
        vk["PRIOR"]    = vk_prior();
        vk["NEXT"]     = vk_next();

        vk["LEFT"]     = vk_left();
        vk["RIGHT"]    = vk_right();
        vk["UP"]       = vk_up();
        vk["DOWN"]     = vk_down();

        vk["F1"]       = vk_f1();
        vk["F2"]       = vk_f2();
        vk["F3"]       = vk_f3();
        vk["F4"]       = vk_f4();
        vk["F5"]       = vk_f5();
        vk["F6"]       = vk_f6();
        vk["F7"]       = vk_f7();
        vk["F8"]       = vk_f8();
        vk["F9"]       = vk_f9();
        vk["F10"]      = vk_f10();
        vk["F11"]      = vk_f11();
        vk["F12"]      = vk_f12();

        vk["LBUTTON"]  = vk_lbutton();
        vk["RBUTTON"]  = vk_rbutton();
        vk["MBUTTON"]  = vk_mbutton();

        vk["W"]        = vk_w();
        vk["A"]        = vk_a();
        vk["S"]        = vk_s();
        vk["D"]        = vk_d();
        vk["Q"]        = vk_q();
        vk["E"]        = vk_e();
        vk["C"]        = vk_c();
        vk["R"]        = vk_r();
        vk["Z"]        = vk_z();
        vk["X"]        = vk_x();
        vk["V"]        = vk_v();

        auto gl = lua.create_named_table("GL");

        gl["MODELVIEW"]               = gl_modelview();
        gl["PROJECTION"]              = gl_projection();
        gl["TEXTURE"]                 = gl_texture();

        gl["DEPTH_TEST"]              = gl_depth_test();
        gl["BLEND"]                   = gl_blend();
        gl["ALPHA_TEST"]              = gl_alpha_test();
        gl["CULL_FACE"]               = gl_cull_face();
        gl["LIGHTING"]                = gl_lighting();
        gl["FOG"]                     = gl_fog();
        gl["TEXTURE_2D"]              = gl_texture_2d();

        gl["FRONT"]                   = gl_front();
        gl["BACK"]                    = gl_back();
        gl["FRONT_AND_BACK"]          = gl_front_and_back();

        gl["SRC_ALPHA"]               = gl_src_alpha();
        gl["ONE_MINUS_SRC_ALPHA"]     = gl_one_minus_src_alpha();
        gl["ONE"]                     = gl_one();
        gl["ZERO"]                    = gl_zero();

        gl["LINES"]                   = gl_lines();
        gl["LINE_STRIP"]              = gl_line_strip();
        gl["LINE_LOOP"]               = gl_line_loop();
        gl["TRIANGLES"]               = gl_triangles();
        gl["TRIANGLE_STRIP"]          = gl_triangle_strip();
        gl["TRIANGLE_FAN"]            = gl_triangle_fan();
        gl["QUADS"]                   = gl_quads();
        gl["POINTS"]                  = gl_points();
        gl["POLYGON"]                 = gl_polygon();

        gl["LINE"]                    = gl_line();
        gl["FILL"]                    = gl_fill();

        gl["ALL_ATTRIB_BITS"]         = gl_all_attrib_bits();
    }

    void register_sdk_bindings()
    {
        auto sdk = lua.create_named_table("sdk");

        sdk.set_function("gl_enable",       &bindings::sdk::gl_enable);
        sdk.set_function("gl_disable",      &bindings::sdk::gl_disable);
        sdk.set_function("gl_depth_mask",   &bindings::sdk::gl_depth_mask);
        sdk.set_function("gl_blend_func",   &bindings::sdk::gl_blend_func);
        sdk.set_function("gl_line_width",   &bindings::sdk::gl_line_width);
        sdk.set_function("gl_point_size",   &bindings::sdk::gl_point_size);
        sdk.set_function("gl_color4f",      &bindings::sdk::gl_color4f);
        sdk.set_function("gl_color3f",      &bindings::sdk::gl_color3f);
        sdk.set_function("gl_polygon_mode", &bindings::sdk::gl_polygon_mode);
        sdk.set_function("gl_push_attrib",  &bindings::sdk::gl_push_attrib);
        sdk.set_function("gl_pop_attrib",   &bindings::sdk::gl_pop_attrib);
        sdk.set_function("gl_push_matrix",  &bindings::sdk::gl_push_matrix);
        sdk.set_function("gl_pop_matrix",   &bindings::sdk::gl_pop_matrix);
        sdk.set_function("gl_begin",        &bindings::sdk::gl_begin);
        sdk.set_function("gl_end",          &bindings::sdk::gl_end);
        sdk.set_function("gl_vertex3f",     &bindings::sdk::gl_vertex3f);
        sdk.set_function("gl_vertex2f",     &bindings::sdk::gl_vertex2f);
        sdk.set_function("gl_translate",    &bindings::sdk::gl_translate);
        sdk.set_function("gl_rotate",       &bindings::sdk::gl_rotate);
        sdk.set_function("gl_scale",        &bindings::sdk::gl_scale);

        sdk.set_function("gl_mult_matrix_d", [](sol::as_table_t<std::vector<double>> m) {
            auto& vec = m.value();
            if (vec.size() >= 16) {
                glMultMatrixd(vec.data());
            } else {
                sdk::log_warn(std::format(
                    "gl_mult_matrix_d: expected 16 elements, got {}", vec.size()));
            }
        });

        sdk.set_function("gl_apply_lookat",
            [this](double ex, double ey, double ez,
                   double cx, double cy, double cz,
                   double ux, double uy, double uz) {
                render.call_orig_glu_lookat(ex, ey, ez, cx, cy, cz, ux, uy, uz);
            });

        sdk.set_function("is_key_down", &bindings::sdk::is_key_down);
        sdk.set_function("get_cursor_pos", []() {
            auto pos = bindings::sdk::get_cursor_pos();
            return std::make_tuple(pos.x, pos.y);
        });
        sdk.set_function("set_cursor_pos", &bindings::sdk::set_cursor_pos);
        sdk.set_function("show_cursor",    &bindings::sdk::show_cursor);

        sdk.set_function("get_window_rect", []() {
            auto r = bindings::sdk::get_window_rect();
            return std::make_tuple(r.left, r.top, r.right, r.bottom);
        });

        sdk.set_function("send_chars", [](const std::string& chars) {
            for (char c : chars)
            {
                INPUT input{};
                input.type          = INPUT_KEYBOARD;
                input.ki.wVk        = 0;
                input.ki.wScan      = static_cast<WORD>(c);
                input.ki.dwFlags    = KEYEVENTF_UNICODE;
                SendInput(1, &input, sizeof(INPUT));
                input.ki.dwFlags   |= KEYEVENTF_KEYUP;
                SendInput(1, &input, sizeof(INPUT));
            }
        });

        sdk.set_function("log_info",    &bindings::sdk::log_info);
        sdk.set_function("log_warn",    &bindings::sdk::log_warn);
        sdk.set_function("log_error",   &bindings::sdk::log_error);
        sdk.set_function("get_log_dir", &bindings::sdk::get_log_dir);
    }

    void register_ui_bindings()
    {
        auto ui = lua.create_named_table("ui");

        ui.set_function("begin_window", &bindings::ui::begin_window);
        ui.set_function("end_window",   &bindings::ui::end_window);

        ui.set_function("text",          &bindings::ui::text);
        ui.set_function("text_wrapped",  &bindings::ui::text_wrapped);
        ui.set_function("text_disabled", &bindings::ui::text_disabled);
        ui.set_function("text_colored",  &bindings::ui::text_colored);

        ui.set_function("button",       &bindings::ui::button);
        ui.set_function("button_sized", &bindings::ui::button_sized);

        ui.set_function("checkbox", [](const std::string& label, bool v) {
            bool changed = bindings::ui::checkbox(label, v);
            return std::make_tuple(v, changed);
        });

        ui.set_function("drag_float",
            [](const std::string& label, float v,
               float spd, float mn, float mx) {
                bool changed = bindings::ui::drag_float(label, v, spd, mn, mx);
                return std::make_tuple(v, changed);
            });

        ui.set_function("slider_float",
            [](const std::string& label, float v, float mn, float mx) {
                bool changed = bindings::ui::slider_float(label, v, mn, mx);
                return std::make_tuple(v, changed);
            });

        ui.set_function("slider_int",
            [](const std::string& label, int v, int mn, int mx) {
                bool changed = bindings::ui::slider_int(label, v, mn, mx);
                return std::make_tuple(v, changed);
            });

        ui.set_function("input_text",
            [](const std::string& label, std::string text) {
                bool changed = bindings::ui::input_text(label, text);
                return std::make_tuple(text, changed);
            });

        ui.set_function("color_edit3",
            [](const std::string& label, float r, float g, float b) {
                bool changed = bindings::ui::color_edit3(label, r, g, b);
                return std::make_tuple(r, g, b, changed);
            });

        ui.set_function("separator",        &bindings::ui::separator);
        ui.set_function("same_line",        &bindings::ui::same_line);
        ui.set_function("spacing",          &bindings::ui::spacing);
        ui.set_function("tree_node",        &bindings::ui::tree_node);
        ui.set_function("tree_pop",         &bindings::ui::tree_pop);
        ui.set_function("tab_bar_begin",    &bindings::ui::tab_bar_begin);
        ui.set_function("tab_bar_end",      &bindings::ui::tab_bar_end);
        ui.set_function("tab_item_begin",   &bindings::ui::tab_item_begin);
        ui.set_function("tab_item_end",     &bindings::ui::tab_item_end);
        ui.set_function("collapsing_header",&bindings::ui::collapsing_header);

        ui.set_function("begin_group",      &bindings::ui::begin_group);
        ui.set_function("end_group",        &bindings::ui::end_group);

        ui.set_function("set_next_window_pos",  &bindings::ui::set_next_window_pos);
        ui.set_function("set_next_window_size", &bindings::ui::set_next_window_size);
        ui.set_function("set_cursor_pos_x",     &bindings::ui::set_cursor_pos_x);
        ui.set_function("get_window_width",     &bindings::ui::get_window_width);

        ui.set_function("push_style_color",    &bindings::ui::push_style_color);
        ui.set_function("pop_style_color",     &bindings::ui::pop_style_color);
        ui.set_function("push_style_var_float",&bindings::ui::push_style_var_float);
        ui.set_function("push_style_var_vec2", &bindings::ui::push_style_var_vec2);
        ui.set_function("pop_style_var",       &bindings::ui::pop_style_var);

        ui.set_function("columns",          &bindings::ui::columns);
        ui.set_function("next_column",      &bindings::ui::next_column);
        ui.set_function("set_column_width", &bindings::ui::set_column_width);

        ui.set_function("get_delta_time",        &bindings::ui::get_delta_time);
        ui.set_function("get_framerate",         &bindings::ui::get_framerate);
        ui.set_function("want_capture_keyboard", &bindings::ui::want_capture_keyboard);
        ui.set_function("want_capture_mouse",    &bindings::ui::want_capture_mouse);
        ui.set_function("progress_bar",          &bindings::ui::progress_bar);
        ui.set_function("tooltip",               &bindings::ui::tooltip);
    }

    // ── Plugin lifecycle ──────────────────────────────────────────────────

    void do_load_plugins()
    {
        auto plugin_dir = fs::current_path() / "plugins";

        if (!fs::exists(plugin_dir)) {
            sdk::log_info("No plugins directory found");
            return;
        }

        std::vector<fs::path> plugin_files;
        for (const auto& entry : fs::directory_iterator(plugin_dir)) {
            if (entry.is_regular_file() && entry.path().extension() == ".lua") {
                plugin_files.push_back(entry.path());
            }
        }

        std::sort(plugin_files.begin(), plugin_files.end(),
            [](const fs::path& a, const fs::path& b) {
                return a.filename() < b.filename();
            });

        if (plugin_files.empty()) {
            sdk::log_info("No plugins found");
            return;
        }

        sdk::log_info(std::format("Loading {} plugins...", plugin_files.size()));

        for (const auto& path : plugin_files) {
            sdk::log_info(std::format("Loading plugin: {}", path.filename().string()));

            auto result = lua.safe_script_file(path.string());
            if (!result.valid()) {
                sol::error err = result;
                sdk::log_error(std::format("Failed to load {}: {}",
                              path.filename().string(), err.what()));
            }
        }

        g_ctx.cb.on_load.invoke();
        sdk::log_info("Plugins loaded");
    }

    void do_unload_plugins()
    {
        g_ctx.cb.on_unload.invoke();

        render.clear_callbacks();

        g_ctx.cb.on_load.clear();
        g_ctx.cb.on_unload.clear();

        sdk::log_info("Plugins unloaded");
    }
};

// ── LuaState ─────────────────────────────────────────────────────────────────

LuaState::LuaState(render::HookSystem& render)
    : pimpl(std::make_unique<impl>(render)) {}

LuaState::~LuaState() = default;

LuaState::LuaState(LuaState&&) noexcept = default;
LuaState& LuaState::operator=(LuaState&&) noexcept = default;

void LuaState::load_plugins()
{
    pimpl->do_load_plugins();
}

void LuaState::unload_plugins()
{
    pimpl->do_unload_plugins();
}

} // namespace sdk::lua
