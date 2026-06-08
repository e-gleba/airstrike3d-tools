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
#include <algorithm>

namespace fs = std::filesystem;

namespace sdk::lua
{

// ── Trampoline helpers ───────────────────────────────────────────────────────
//
// Hooked functions have their entry points patched inline by safetyhook.
// Calling GetProcAddress() returns the hooked address, causing infinite
// recursion if a callback re-invokes the same function.  These helpers
// call through the safetyhook trampoline — the original, un-hooked code.

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

// ── impl ─────────────────────────────────────────────────────────────────────

struct LuaState::impl
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

        register_math_bindings();
        register_constants();
        register_sdk_bindings();
        register_ui_bindings();
        register_callback_hooks();

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
        -> consuming_callback_list<double, double, double,
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
                spdlog::error("Lua callback error: {}", err.what());
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
        auto vk = lua.create_named_table("VK");

        // ── Modifiers ──
        vk.set_function("SHIFT",    &bindings::constants::vk_shift);
        vk.set_function("CONTROL",  &bindings::constants::vk_control);
        vk.set_function("SPACE",    &bindings::constants::vk_space);

        // ── Navigation ──
        vk.set_function("INSERT",   &bindings::constants::vk_insert);
        vk.set_function("ESCAPE",   &bindings::constants::vk_escape);
        vk.set_function("TAB",      &bindings::constants::vk_tab);
        vk.set_function("RETURN",   &bindings::constants::vk_return);
        vk.set_function("BACK",     &bindings::constants::vk_back);
        vk.set_function("DELETE",   &bindings::constants::vk_delete);
        vk.set_function("HOME",     &bindings::constants::vk_home);
        vk.set_function("END",      &bindings::constants::vk_end);
        vk.set_function("PRIOR",    &bindings::constants::vk_prior);
        vk.set_function("NEXT",     &bindings::constants::vk_next);

        // ── Arrow keys ──
        vk.set_function("LEFT",     &bindings::constants::vk_left);
        vk.set_function("RIGHT",    &bindings::constants::vk_right);
        vk.set_function("UP",       &bindings::constants::vk_up);
        vk.set_function("DOWN",     &bindings::constants::vk_down);

        // ── F-keys ──
        vk.set_function("F1",       &bindings::constants::vk_f1);
        vk.set_function("F2",       &bindings::constants::vk_f2);
        vk.set_function("F3",       &bindings::constants::vk_f3);
        vk.set_function("F4",       &bindings::constants::vk_f4);
        vk.set_function("F5",       &bindings::constants::vk_f5);
        vk.set_function("F6",       &bindings::constants::vk_f6);
        vk.set_function("F7",       &bindings::constants::vk_f7);
        vk.set_function("F8",       &bindings::constants::vk_f8);
        vk.set_function("F9",       &bindings::constants::vk_f9);
        vk.set_function("F10",      &bindings::constants::vk_f10);
        vk.set_function("F11",      &bindings::constants::vk_f11);
        vk.set_function("F12",      &bindings::constants::vk_f12);

        // ── Mouse buttons ──
        vk.set_function("LBUTTON",  &bindings::constants::vk_lbutton);
        vk.set_function("RBUTTON",  &bindings::constants::vk_rbutton);
        vk.set_function("MBUTTON",  &bindings::constants::vk_mbutton);

        // ── Letter keys ──
        vk.set_function("W",        &bindings::constants::vk_w);
        vk.set_function("A",        &bindings::constants::vk_a);
        vk.set_function("S",        &bindings::constants::vk_s);
        vk.set_function("D",        &bindings::constants::vk_d);
        vk.set_function("Q",        &bindings::constants::vk_q);
        vk.set_function("E",        &bindings::constants::vk_e);
        vk.set_function("C",        &bindings::constants::vk_c);
        vk.set_function("R",        &bindings::constants::vk_r);
        vk.set_function("Z",        &bindings::constants::vk_z);
        vk.set_function("X",        &bindings::constants::vk_x);
        vk.set_function("V",        &bindings::constants::vk_v);

        auto gl = lua.create_named_table("GL");

        // ── Matrix mode ──
        gl.set_function("MODELVIEW",               &bindings::constants::gl_modelview);
        gl.set_function("PROJECTION",              &bindings::constants::gl_projection);
        gl.set_function("TEXTURE",                 &bindings::constants::gl_texture);

        // ── State caps ──
        gl.set_function("DEPTH_TEST",              &bindings::constants::gl_depth_test);
        gl.set_function("BLEND",                   &bindings::constants::gl_blend);
        gl.set_function("ALPHA_TEST",              &bindings::constants::gl_alpha_test);
        gl.set_function("CULL_FACE",               &bindings::constants::gl_cull_face);
        gl.set_function("LIGHTING",                &bindings::constants::gl_lighting);
        gl.set_function("FOG",                     &bindings::constants::gl_fog);
        gl.set_function("TEXTURE_2D",              &bindings::constants::gl_texture_2d);

        // ── Face selection ──
        gl.set_function("FRONT",                   &bindings::constants::gl_front);
        gl.set_function("BACK",                    &bindings::constants::gl_back);
        gl.set_function("FRONT_AND_BACK",          &bindings::constants::gl_front_and_back);

        // ── Blend factors ──
        gl.set_function("SRC_ALPHA",               &bindings::constants::gl_src_alpha);
        gl.set_function("ONE_MINUS_SRC_ALPHA",     &bindings::constants::gl_one_minus_src_alpha);
        gl.set_function("ONE",                     &bindings::constants::gl_one);
        gl.set_function("ZERO",                    &bindings::constants::gl_zero);

        // ── Primitive types (for glBegin) ──
        gl.set_function("LINES",                   &bindings::constants::gl_lines);
        gl.set_function("LINE_STRIP",              &bindings::constants::gl_line_strip);
        gl.set_function("LINE_LOOP",               &bindings::constants::gl_line_loop);
        gl.set_function("TRIANGLES",               &bindings::constants::gl_triangles);
        gl.set_function("TRIANGLE_STRIP",          &bindings::constants::gl_triangle_strip);
        gl.set_function("TRIANGLE_FAN",            &bindings::constants::gl_triangle_fan);
        gl.set_function("QUADS",                   &bindings::constants::gl_quads);
        gl.set_function("POINTS",                  &bindings::constants::gl_points);
        gl.set_function("POLYGON",                 &bindings::constants::gl_polygon);

        // ── Polygon mode (for glPolygonMode) ──
        gl.set_function("LINE",                    &bindings::constants::gl_line);
        gl.set_function("FILL",                    &bindings::constants::gl_fill);

        // ── State masks ──
        gl.set_function("ALL_ATTRIB_BITS",         &bindings::constants::gl_all_attrib_bits);
    }

    void register_sdk_bindings()
    {
        auto sdk = lua.create_named_table("sdk");

        // ── GL state management ──

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

        // ── Matrix operations ──
        //
        // gl_mult_matrix_d takes a table of 16 doubles (column-major order)
        // and multiplies the current matrix by it.

        sdk.set_function("gl_mult_matrix_d", [](sol::as_table_t<std::vector<double>> m) {
            auto& vec = m.value();
            if (vec.size() >= 16) {
                glMultMatrixd(vec.data());
            } else {
                spdlog::warn("gl_mult_matrix_d: expected 16 elements, got {}", vec.size());
            }
        });

        // ── Camera override ──
        //
        // gl_apply_lookat calls through the hook trampoline to the
        // ORIGINAL gluLookAt.  This avoids infinite recursion when
        // called from within an on_glu_lookat callback.

        sdk.set_function("gl_apply_lookat",
            [](double ex, double ey, double ez,
               double cx, double cy, double cz,
               double ux, double uy, double uz) {
                call_orig_glu_lookat(ex, ey, ez, cx, cy, cz, ux, uy, uz);
            });

        // ── Input ──

        sdk.set_function("is_key_down", &bindings::sdk::is_key_down);
        sdk.set_function("get_cursor_pos", []() {
            auto pos = bindings::sdk::get_cursor_pos();
            return std::make_tuple(pos.x, pos.y);
        });
        sdk.set_function("set_cursor_pos", &bindings::sdk::set_cursor_pos);
        sdk.set_function("show_cursor",    &bindings::sdk::show_cursor);

        // ── Window ──

        sdk.set_function("get_window_rect", []() {
            auto r = bindings::sdk::get_window_rect();
            return std::make_tuple(r.left, r.top, r.right, r.bottom);
        });

        // ── Keyboard simulation ──

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

        // ── Logging ──

        sdk.set_function("log_info",    &bindings::sdk::log_info);
        sdk.set_function("log_warn",    &bindings::sdk::log_warn);
        sdk.set_function("log_error",   &bindings::sdk::log_error);
        sdk.set_function("get_log_dir", &bindings::sdk::get_log_dir);
    }

    void register_ui_bindings()
    {
        auto ui = lua.create_named_table("ui");

        // ── Window management ──
        ui.set_function("begin_window", &bindings::ui::begin_window);
        ui.set_function("end_window",   &bindings::ui::end_window);

        // ── Text rendering ──
        ui.set_function("text",          &bindings::ui::text);
        ui.set_function("text_wrapped",  &bindings::ui::text_wrapped);
        ui.set_function("text_disabled", &bindings::ui::text_disabled);
        ui.set_function("text_colored",  &bindings::ui::text_colored);

        // ── Buttons ──
        ui.set_function("button",       &bindings::ui::button);
        ui.set_function("button_sized", &bindings::ui::button_sized);

        // ── Input widgets (return value, changed) ──
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

        // ── Layout ──
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

        // ── Groups ──
        ui.set_function("begin_group",      &bindings::ui::begin_group);
        ui.set_function("end_group",        &bindings::ui::end_group);

        // ── Positioning ──
        ui.set_function("set_next_window_pos",  &bindings::ui::set_next_window_pos);
        ui.set_function("set_next_window_size", &bindings::ui::set_next_window_size);
        ui.set_function("set_cursor_pos_x",     &bindings::ui::set_cursor_pos_x);
        ui.set_function("get_window_width",     &bindings::ui::get_window_width);

        // ── Styling ──
        ui.set_function("push_style_color",    &bindings::ui::push_style_color);
        ui.set_function("pop_style_color",     &bindings::ui::pop_style_color);
        ui.set_function("push_style_var_float",&bindings::ui::push_style_var_float);
        ui.set_function("push_style_var_vec2", &bindings::ui::push_style_var_vec2);
        ui.set_function("pop_style_var",       &bindings::ui::pop_style_var);

        // ── Columns ──
        ui.set_function("columns",          &bindings::ui::columns);
        ui.set_function("next_column",      &bindings::ui::next_column);
        ui.set_function("set_column_width", &bindings::ui::set_column_width);

        // ── Utilities ──
        ui.set_function("get_delta_time",        &bindings::ui::get_delta_time);
        ui.set_function("get_framerate",         &bindings::ui::get_framerate);
        ui.set_function("want_capture_keyboard", &bindings::ui::want_capture_keyboard);
        ui.set_function("want_capture_mouse",    &bindings::ui::want_capture_mouse);
        ui.set_function("progress_bar",          &bindings::ui::progress_bar);
        ui.set_function("tooltip",               &bindings::ui::tooltip);
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

    // ── Collect all .lua files ───────────────────────────────────────────────
    //
    // fs::directory_iterator doesn't guarantee alphabetical order.
    // We must sort to ensure framework files (prefixed with _) load
    // before plugin files that depend on them.

    std::vector<fs::path> plugin_files;
    for (const auto& entry : fs::directory_iterator(plugin_dir)) {
        if (entry.is_regular_file() && entry.path().extension() == ".lua") {
            plugin_files.push_back(entry.path());
        }
    }

    // Sort alphabetically by filename
    // Underscore (_) = 0x5F sorts before letters (a-z = 0x61-0x7A, A-Z = 0x41-0x5A)
    // So _ui_framework.lua loads before cheats.lua, freecam.lua, etc.
    std::sort(plugin_files.begin(), plugin_files.end(),
        [](const fs::path& a, const fs::path& b) {
            return a.filename() < b.filename();
        });

    if (plugin_files.empty()) {
        spdlog::info("[sdk] No plugins found");
        return;
    }

    spdlog::info("[sdk] Loading {} plugins...", plugin_files.size());

    // ── Load plugins in sorted order ─────────────────────────────────────────

    for (const auto& path : plugin_files) {
        spdlog::info("[sdk] Loading plugin: {}", path.filename().string());

        auto result = pimpl->lua.safe_script_file(path.string());
        if (!result.valid()) {
            sol::error err = result;
            spdlog::error("[sdk] Failed to load {}: {}",
                          path.filename().string(), err.what());
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
