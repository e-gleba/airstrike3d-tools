/// @file detail/register_bindings.cpp
/// @brief Adapter that bridges pure C++ binding schemas to the sol2 backend.
///
/// This is the ONLY file that knows about both sol2 and the binding schemas.
/// To replace sol2 with another Lua binding library, rewrite only this file
/// and `detail/lua_engine.cpp`.

#include "sdk/lua/detail/register_bindings.hpp"
#include "sdk/lua/detail/binding_api.hpp"
#include "sdk/lua/detail/impl.hpp"

// Binding schemas (pure C++ — no sol2 in their headers)
#include "sdk/lua/bindings/math.hpp"
#include "sdk/lua/bindings/constants.hpp"
#include "sdk/lua/bindings/sdk.hpp"
#include "sdk/lua/bindings/ui.hpp"

#include <sol/sol.hpp>

#include <cmath>
#include <format>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <span>
#include <string>
#include <tuple>

// Platform headers for VK_ / GL_ constants
#include <GL/gl.h>
#include <windows.h>

namespace sdk::lua::detail
{

namespace
{

[[nodiscard]] constexpr auto to_tuple(const glm::dvec3& v) noexcept
    -> std::tuple<double, double, double>
{
    return {v.x, v.y, v.z};
}

[[nodiscard]] constexpr auto vec3(double x, double y, double z) noexcept
    -> glm::dvec3
{
    return {x, y, z};
}

[[nodiscard]] auto mat4_to_table(sol::state& s, const glm::dmat4& mat)
    -> sol::table
{
    auto t{s.create_table(16, 0)};
    auto p{std::span(glm::value_ptr(mat), 16)};
    for (std::size_t i = 0; i < 16; ++i) t[i + 1] = p[i];
    return t;
}

} // namespace

// ─── constants ───────────────────────────────────────────────────────────────

void register_constants(table_handle root)
{
    // Virtual keys
    auto vk = root.create_table("VK");

    constexpr std::pair<const char*, int> k_vk_map[] = {
        {"SHIFT", VK_SHIFT},     {"CONTROL", VK_CONTROL},
        {"SPACE", VK_SPACE},     {"INSERT", VK_INSERT},
        {"LBUTTON", VK_LBUTTON}, {"RBUTTON", VK_RBUTTON},
        {"ESCAPE", VK_ESCAPE},   {"TAB", VK_TAB},
        {"RETURN", VK_RETURN},   {"BACK", VK_BACK},
        {"DELETE", VK_DELETE},   {"HOME", VK_HOME},
        {"END", VK_END},         {"PRIOR", VK_PRIOR},
        {"NEXT", VK_NEXT},       {"LEFT", VK_LEFT},
        {"RIGHT", VK_RIGHT},     {"UP", VK_UP},
        {"DOWN", VK_DOWN},       {"MENU", VK_MENU},
        {"CAPITAL", VK_CAPITAL}, {"MBUTTON", VK_MBUTTON},
        {"XBUTTON1", VK_XBUTTON1}, {"XBUTTON2", VK_XBUTTON2},
    };

    for (auto [name, code] : k_vk_map) {
        vk.add_function(name, [code]{ return code; });
    }

    // A-Z, 0-9, F1-F24
    for (char c = 'A'; c <= 'Z'; ++c) {
        vk.add_function(std::string(1, c), [c]{ return static_cast<int>(c); });
    }
    for (char c = '0'; c <= '9'; ++c) {
        vk.add_function(std::string(1, c), [c]{ return static_cast<int>(c); });
    }
    for (int i = 1; i <= 24; ++i) {
        vk.add_function(std::format("F{}", i), [i]{ return VK_F1 + (i - 1); });
    }

    // GL constants
    auto gl = root.create_table("GL");

    constexpr std::pair<const char*, int> k_gl_map[] = {
        {"MODELVIEW", GL_MODELVIEW}, {"PROJECTION", GL_PROJECTION},
        {"TEXTURE", GL_TEXTURE},     {"DEPTH_TEST", GL_DEPTH_TEST},
        {"BLEND", GL_BLEND},         {"ALPHA_TEST", GL_ALPHA_TEST},
        {"CULL_FACE", GL_CULL_FACE}, {"LIGHTING", GL_LIGHTING},
        {"FOG", GL_FOG},             {"FRONT", GL_FRONT},
        {"BACK", GL_BACK},           {"FRONT_AND_BACK", GL_FRONT_AND_BACK},
        {"SRC_ALPHA", GL_SRC_ALPHA},
        {"ONE_MINUS_SRC_ALPHA", GL_ONE_MINUS_SRC_ALPHA},
        {"ONE", GL_ONE},             {"ZERO", GL_ZERO},
        {"LINES", GL_LINES},         {"LINE_STRIP", GL_LINE_STRIP},
        {"LINE_LOOP", GL_LINE_LOOP}, {"TRIANGLES", GL_TRIANGLES},
        {"TRIANGLE_STRIP", GL_TRIANGLE_STRIP},
        {"TRIANGLE_FAN", GL_TRIANGLE_FAN},
        {"QUADS", GL_QUADS},         {"POINTS", GL_POINTS},
        {"POLYGON", GL_POLYGON},     {"FILL", GL_FILL},
        {"LINE", GL_LINE},           {"POINT", GL_POINT},
        {"ALL_ATTRIB_BITS", GL_ALL_ATTRIB_BITS},
        {"ENABLE_BIT", GL_ENABLE_BIT},
        {"DEPTH_BUFFER_BIT", GL_DEPTH_BUFFER_BIT},
        {"COLOR_BUFFER_BIT", GL_COLOR_BUFFER_BIT},
        {"POLYGON_BIT", GL_POLYGON_BIT},
        {"LINE_BIT", GL_LINE_BIT},
        {"CURRENT_BIT", GL_CURRENT_BIT},
        {"LIGHTING_BIT", GL_LIGHTING_BIT},
        {"TEXTURE_2D", GL_TEXTURE_2D},
        {"NORMALIZE", GL_NORMALIZE},
        {"COLOR_MATERIAL", GL_COLOR_MATERIAL},
        {"SCISSOR_TEST", GL_SCISSOR_TEST},
        {"STENCIL_TEST", GL_STENCIL_TEST},
    };

    for (auto [name, val] : k_gl_map) {
        gl.add_function(name, [val]{ return val; });
    }
}

// ─── math ────────────────────────────────────────────────────────────────────

void register_math(table_handle m, sol::state& s)
{
    m.add_function("radians", [](double d) noexcept { return glm::radians(d); });
    m.add_function("cos",     [](double v) noexcept { return std::cos(v); });
    m.add_function("sin",     [](double v) noexcept { return std::sin(v); });
    m.add_function("mod",     [](double v, double d) noexcept { return glm::mod(v, d); });
    m.add_function("clamp",   [](double v, double lo, double hi) noexcept
                   { return glm::clamp(v, lo, hi); });
    m.add_function("normalize",
                   [](double x, double y, double z) noexcept
                   { return to_tuple(glm::normalize(vec3(x, y, z))); });
    m.add_function("cross",
                   [](double ax, double ay, double az,
                      double bx, double by, double bz) noexcept
                   { return to_tuple(glm::cross(vec3(ax, ay, az), vec3(bx, by, bz))); });
    m.add_function("lookat_matrix",
                   [&s](double ex, double ey, double ez,
                        double cx, double cy, double cz,
                        double ux, double uy, double uz) -> sol::table
                   {
                       return mat4_to_table(
                           s, glm::lookAt(vec3(ex, ey, ez),
                                          vec3(cx, cy, cz),
                                          vec3(ux, uy, uz)));
                   });
}

// ─── sdk ─────────────────────────────────────────────────────────────────────

void register_sdk(table_handle root, sol::state& s)
{
    // OpenGL wrappers
    root.add_function("gl_enable", &bindings::sdk::gl_enable);
    root.add_function("gl_disable", &bindings::sdk::gl_disable);
    root.add_function("gl_depth_mask", &bindings::sdk::gl_depth_mask);
    root.add_function("gl_blend_func", &bindings::sdk::gl_blend_func);
    root.add_function("gl_line_width", &bindings::sdk::gl_line_width);
    root.add_function("gl_point_size", &bindings::sdk::gl_point_size);
    root.add_function("gl_color4f", &bindings::sdk::gl_color4f);
    root.add_function("gl_color3f", &bindings::sdk::gl_color3f);
    root.add_function("gl_polygon_mode", &bindings::sdk::gl_polygon_mode);
    root.add_function("gl_push_attrib", &bindings::sdk::gl_push_attrib);
    root.add_function("gl_pop_attrib", &bindings::sdk::gl_pop_attrib);
    root.add_function("gl_push_matrix", &bindings::sdk::gl_push_matrix);
    root.add_function("gl_pop_matrix", &bindings::sdk::gl_pop_matrix);
    root.add_function("gl_begin", &bindings::sdk::gl_begin);
    root.add_function("gl_end", &bindings::sdk::gl_end);
    root.add_function("gl_vertex3f", &bindings::sdk::gl_vertex3f);
    root.add_function("gl_vertex2f", &bindings::sdk::gl_vertex2f);
    root.add_function("gl_translate", &bindings::sdk::gl_translate);
    root.add_function("gl_rotate", &bindings::sdk::gl_rotate);
    root.add_function("gl_scale", &bindings::sdk::gl_scale);

    // Input
    root.add_function("is_key_down", &bindings::sdk::is_key_down);
    root.add_function("get_cursor_pos", &bindings::sdk::get_cursor_pos);
    root.add_function("set_cursor_pos", &bindings::sdk::set_cursor_pos);
    root.add_function("show_cursor", &bindings::sdk::show_cursor);

    // Window
    root.add_function("get_window_rect", &bindings::sdk::get_window_rect);

    // Logging
    root.add_function("log_info", &bindings::sdk::log_info);
    root.add_function("log_warn", &bindings::sdk::log_warn);
    root.add_function("log_error", &bindings::sdk::log_error);
    root.add_function("get_log_dir", &bindings::sdk::get_log_dir);
}

// ─── ui ──────────────────────────────────────────────────────────────────────

void register_ui(table_handle ui)
{
    // Window management
    ui.add_function("begin_window", &bindings::ui::begin_window);
    ui.add_function("end_window", &bindings::ui::end_window);

    // Text rendering
    ui.add_function("text", &bindings::ui::text);
    ui.add_function("text_wrapped", &bindings::ui::text_wrapped);
    ui.add_function("text_disabled", &bindings::ui::text_disabled);
    ui.add_function("text_colored", &bindings::ui::text_colored);

    // Buttons
    ui.add_function("button", &bindings::ui::button);
    ui.add_function("button_sized", &bindings::ui::button_sized);

    // Input widgets
    ui.add_function("checkbox", &bindings::ui::checkbox);
    ui.add_function("drag_float", &bindings::ui::drag_float);
    ui.add_function("slider_float", &bindings::ui::slider_float);
    ui.add_function("slider_int", &bindings::ui::slider_int);
    ui.add_function("input_text", &bindings::ui::input_text);
    ui.add_function("color_edit3", &bindings::ui::color_edit3);

    // Layout
    ui.add_function("separator", &bindings::ui::separator);
    ui.add_function("same_line", &bindings::ui::same_line);
    ui.add_function("spacing", &bindings::ui::spacing);
    ui.add_function("tree_node", &bindings::ui::tree_node);
    ui.add_function("tree_pop", &bindings::ui::tree_pop);
    ui.add_function("tab_bar_begin", &bindings::ui::tab_bar_begin);
    ui.add_function("tab_bar_end", &bindings::ui::tab_bar_end);
    ui.add_function("tab_item_begin", &bindings::ui::tab_item_begin);
    ui.add_function("tab_item_end", &bindings::ui::tab_item_end);
    ui.add_function("collapsing_header", &bindings::ui::collapsing_header);

    // Positioning
    ui.add_function("set_next_window_pos", &bindings::ui::set_next_window_pos);
    ui.add_function("set_next_window_size", &bindings::ui::set_next_window_size);
    ui.add_function("set_cursor_pos_x", &bindings::ui::set_cursor_pos_x);
    ui.add_function("get_window_width", &bindings::ui::get_window_width);

    // Styling
    ui.add_function("push_style_color", &bindings::ui::push_style_color);
    ui.add_function("pop_style_color", &bindings::ui::pop_style_color);
    ui.add_function("push_style_var_float", &bindings::ui::push_style_var_float);
    ui.add_function("push_style_var_vec2", &bindings::ui::push_style_var_vec2);
    ui.add_function("pop_style_var", &bindings::ui::pop_style_var);

    // Columns
    ui.add_function("columns", &bindings::ui::columns);
    ui.add_function("next_column", &bindings::ui::next_column);
    ui.add_function("set_column_width", &bindings::ui::set_column_width);

    // Utilities
    ui.add_function("get_delta_time", &bindings::ui::get_delta_time);
    ui.add_function("get_framerate", &bindings::ui::get_framerate);
    ui.add_function("want_capture_keyboard", &bindings::ui::want_capture_keyboard);
    ui.add_function("want_capture_mouse", &bindings::ui::want_capture_mouse);
    ui.add_function("progress_bar", &bindings::ui::progress_bar);
    ui.add_function("tooltip", &bindings::ui::tooltip);
}

// ─── entry point ─────────────────────────────────────────────────────────────

void register_all(LuaState& vm)
{
    std::lock_guard lk{vm.pimpl->mutex};

    auto constants_tbl = get_table(vm.pimpl->state, "_constants");
    register_constants(constants_tbl);

    auto math_tbl = get_table(vm.pimpl->state, "gmath");
    register_math(math_tbl, vm.pimpl->state);

    auto sdk_tbl = get_table(vm.pimpl->state, "sdk");
    register_sdk(sdk_tbl, vm.pimpl->state);

    auto ui_tbl = get_table(vm.pimpl->state, "ui");
    register_ui(ui_tbl);
}

} // namespace sdk::lua::detail
