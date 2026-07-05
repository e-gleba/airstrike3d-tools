/// @file engine_lua.cpp
/// @brief Script engine implementation using sol2/Lua (private backend).

#include "sdk/scripting/engine.hpp"

#include "sdk/core/context.hpp"
#include "sdk/core/contract.hpp"
#include "sdk/core/logging.hpp"
#include "sdk/graphics/graphics.hpp"
#include "sdk/math/math.hpp"
#include "sdk/platform/platform.hpp"
#include "sdk/scripting/callback.hpp"
#include "sdk/ui/ui.hpp"

#include <sol/sol.hpp>

#include <cstdint>
#include <filesystem>
#include <format>
#include <ranges>
#include <stdexcept>
#include <vector>
#include <array>
#include <span>

namespace fs = std::filesystem;

namespace sdk::scripting
{

struct engine::impl final
{
    sol::state lua;

    impl()
    {
        lua.open_libraries(sol::lib::base,
                           sol::lib::math,
                           sol::lib::string,
                           sol::lib::table,
                           sol::lib::io,
                           sol::lib::os);

        register_math_bindings();
        register_constants();
        register_sdk_bindings();
        register_ui_bindings();
        register_callback_hooks();

        sdk::log_info("script engine initialized");
    }

    [[nodiscard]] auto wrap_void(sol::protected_function fn) -> callback_list<>::slot_fn
    {
        return [fn = std::move(fn)]() {
            const auto result = fn();
            if (!result.valid())
            {
                const sol::error err = result;
                sdk::log_error(std::format("script callback error: {}", err.what()));
            }
        };
    }

    [[nodiscard]] auto wrap_bool(sol::protected_function fn)
        -> consuming_callback_list<std::int32_t>::slot_fn
    {
        return [fn = std::move(fn)](std::int32_t key) -> bool {
            const auto result = fn(key);
            if (!result.valid())
            {
                const sol::error err = result;
                sdk::log_error(std::format("script callback error: {}", err.what()));
                return false;
            }
            return result.get_type() == sol::type::boolean && result.get<bool>();
        };
    }

    [[nodiscard]] auto wrap_gl_identity(sol::protected_function fn)
        -> callback_list<matrix_mode>::slot_fn
    {
        return [fn = std::move(fn)](matrix_mode mode) {
            const auto result = fn(static_cast<std::int32_t>(mode));
            if (!result.valid())
            {
                const sol::error err = result;
                sdk::log_error(std::format("script callback error: {}", err.what()));
            }
        };
    }

    [[nodiscard]] auto wrap_glu_lookat(sol::protected_function fn)
        -> consuming_callback_list<double, double, double,
                                    double, double, double,
                                    double, double, double>::slot_fn
    {
        return [fn = std::move(fn)](double eyeX, double eyeY, double eyeZ,
                                     double centerX, double centerY, double centerZ,
                                     double upX, double upY, double upZ) -> bool {
            const auto result = fn(eyeX, eyeY, eyeZ, centerX, centerY, centerZ,
                                   upX, upY, upZ);
            if (!result.valid())
            {
                const sol::error err = result;
                sdk::log_error(std::format("script callback error: {}", err.what()));
                return false;
            }
            return result.get_type() == sol::type::boolean && result.get<bool>();
        };
    }

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

    void register_math_bindings()
    {
        using namespace sdk::math;

        auto gmath = lua.create_named_table("gmath");

        gmath.set_function("radians", &radians);
        gmath.set_function("cos", &cos);
        gmath.set_function("sin", &sin);
        gmath.set_function("mod", &mod);
        gmath.set_function("clamp", &clamp);

        gmath.set_function("normalize", [](double x, double y, double z) {
            const auto v = normalize(x, y, z);
            return std::make_tuple(v.x, v.y, v.z);
        });

        gmath.set_function("cross", [](double ax, double ay, double az,
                                        double bx, double by, double bz) {
            const auto v = cross(ax, ay, az, bx, by, bz);
            return std::make_tuple(v.x, v.y, v.z);
        });

        gmath.set_function("lookat_matrix",
                           [](double ex, double ey, double ez,
                              double cx, double cy, double cz,
                              double ux, double uy, double uz) {
                               return lookat_matrix(ex, ey, ez, cx, cy, cz, ux, uy, uz);
                           });
    }

    void register_constants()
    {
        using namespace sdk::graphics::constants;

        auto vk = lua.create_named_table("VK");

        vk["SHIFT"]   = vk_shift();
        vk["CONTROL"] = vk_control();
        vk["SPACE"]   = vk_space();
        vk["INSERT"]  = vk_insert();
        vk["ESCAPE"]  = vk_escape();
        vk["TAB"]     = vk_tab();
        vk["RETURN"]  = vk_return();
        vk["BACK"]    = vk_back();
        vk["DELETE"]  = vk_delete();
        vk["HOME"]    = vk_home();
        vk["END"]     = vk_end();
        vk["PRIOR"]   = vk_prior();
        vk["NEXT"]    = vk_next();
        vk["LEFT"]    = vk_left();
        vk["RIGHT"]   = vk_right();
        vk["UP"]      = vk_up();
        vk["DOWN"]    = vk_down();
        vk["F1"]      = vk_f1();
        vk["F2"]      = vk_f2();
        vk["F3"]      = vk_f3();
        vk["F4"]      = vk_f4();
        vk["F5"]      = vk_f5();
        vk["F6"]      = vk_f6();
        vk["F7"]      = vk_f7();
        vk["F8"]      = vk_f8();
        vk["F9"]      = vk_f9();
        vk["F10"]     = vk_f10();
        vk["F11"]     = vk_f11();
        vk["F12"]     = vk_f12();
        vk["LBUTTON"] = vk_lbutton();
        vk["RBUTTON"] = vk_rbutton();
        vk["MBUTTON"] = vk_mbutton();
        vk["W"]       = vk_w();
        vk["A"]       = vk_a();
        vk["S"]       = vk_s();
        vk["D"]       = vk_d();
        vk["Q"]       = vk_q();
        vk["E"]       = vk_e();
        vk["C"]       = vk_c();
        vk["R"]       = vk_r();
        vk["Z"]       = vk_z();
        vk["X"]       = vk_x();
        vk["V"]       = vk_v();

        auto gl = lua.create_named_table("GL");

        gl["MODELVIEW"]           = gl_modelview();
        gl["PROJECTION"]          = gl_projection();
        gl["TEXTURE"]             = gl_texture();
        gl["DEPTH_TEST"]          = gl_depth_test();
        gl["BLEND"]               = gl_blend();
        gl["ALPHA_TEST"]          = gl_alpha_test();
        gl["CULL_FACE"]           = gl_cull_face();
        gl["LIGHTING"]            = gl_lighting();
        gl["FOG"]                 = gl_fog();
        gl["TEXTURE_2D"]          = gl_texture_2d();
        gl["FRONT"]               = gl_front();
        gl["BACK"]                = gl_back();
        gl["FRONT_AND_BACK"]      = gl_front_and_back();
        gl["SRC_ALPHA"]           = gl_src_alpha();
        gl["ONE_MINUS_SRC_ALPHA"] = gl_one_minus_src_alpha();
        gl["ONE"]                 = gl_one();
        gl["ZERO"]                = gl_zero();
        gl["LINES"]               = gl_lines();
        gl["LINE_STRIP"]          = gl_line_strip();
        gl["LINE_LOOP"]           = gl_line_loop();
        gl["TRIANGLES"]           = gl_triangles();
        gl["TRIANGLE_STRIP"]      = gl_triangle_strip();
        gl["TRIANGLE_FAN"]        = gl_triangle_fan();
        gl["QUADS"]               = gl_quads();
        gl["POINTS"]              = gl_points();
        gl["POLYGON"]             = gl_polygon();
        gl["LINE"]                = gl_line();
        gl["FILL"]                = gl_fill();
        gl["ALL_ATTRIB_BITS"]     = gl_all_attrib_bits();
    }

    void register_sdk_bindings()
    {
        using namespace sdk::graphics;
        using namespace sdk::platform;

        auto sdk_table = lua.create_named_table("sdk");

        sdk_table.set_function("gl_enable", &enable);
        sdk_table.set_function("gl_disable", &disable);
        sdk_table.set_function("gl_depth_mask", &depth_mask);
        sdk_table.set_function("gl_blend_func", &blend_func);
        sdk_table.set_function("gl_line_width", &line_width);
        sdk_table.set_function("gl_point_size", &point_size);
        sdk_table.set_function("gl_color4f", &color4f);
        sdk_table.set_function("gl_color3f", &color3f);
        sdk_table.set_function("gl_polygon_mode", &polygon_mode);
        sdk_table.set_function("gl_push_attrib", &push_attrib);
        sdk_table.set_function("gl_pop_attrib", &pop_attrib);
        sdk_table.set_function("gl_push_matrix", &push_matrix);
        sdk_table.set_function("gl_pop_matrix", &pop_matrix);
        sdk_table.set_function("gl_begin", &begin);
        sdk_table.set_function("gl_end", &end);
        sdk_table.set_function("gl_vertex3f", &vertex3f);
        sdk_table.set_function("gl_vertex2f", &vertex2f);
        sdk_table.set_function("gl_translate", &translate);
        sdk_table.set_function("gl_rotate", &rotate);
        sdk_table.set_function("gl_scale", &scale);

        sdk_table.set_function("gl_mult_matrix_d",
                               [](sol::as_table_t<std::vector<double>> m) {
                                   const auto& vec = m.value();
                                   require(vec.size() >= 16,
                                           "gl_mult_matrix_d: expected at least 16 elements");
                                   std::array<double, 16> matrix{};
                                   std::copy_n(vec.begin(), 16, matrix.begin());
                                   mult_matrix(std::span<const double, 16>{ matrix });
                               });

        sdk_table.set_function("gl_apply_lookat", &apply_lookat);
        sdk_table.set_function("is_key_down", &is_key_down);
        sdk_table.set_function("get_cursor_pos", []() {
            const auto pos = get_cursor_pos();
            return std::make_tuple(pos.x, pos.y);
        });
        sdk_table.set_function("set_cursor_pos", &set_cursor_pos);
        sdk_table.set_function("show_cursor", &show_cursor);
        sdk_table.set_function("get_window_rect", []() {
            const auto r = get_window_rect();
            return std::make_tuple(r.left, r.top, r.right, r.bottom);
        });
        sdk_table.set_function("send_chars", &send_chars);
        sdk_table.set_function("log_info",
                               [](const std::string& m) { sdk::platform::log_info(m); });
        sdk_table.set_function("log_warn",
                               [](const std::string& m) { sdk::platform::log_warn(m); });
        sdk_table.set_function("log_error",
                               [](const std::string& m) { sdk::platform::log_error(m); });
        sdk_table.set_function("get_log_dir", []() -> std::string {
            return std::string{ sdk::platform::get_log_dir() };
        });
    }

    void register_ui_bindings()
    {
        using namespace sdk::ui;

        auto ui_table = lua.create_named_table("ui");

        ui_table.set_function("begin_window", &begin_window);
        ui_table.set_function("end_window", &end_window);
        ui_table.set_function("text", &text);
        ui_table.set_function("text_wrapped", &text_wrapped);
        ui_table.set_function("text_disabled", &text_disabled);
        ui_table.set_function("text_colored", &text_colored);
        ui_table.set_function("button", &button);
        ui_table.set_function("button_sized", &button_sized);

        ui_table.set_function("checkbox", [](const std::string& label, bool v) {
            const bool changed = checkbox(label, v);
            return std::make_tuple(v, changed);
        });

        ui_table.set_function("drag_float",
                              [](const std::string& label, float v,
                                 float spd, float mn, float mx) {
                                  const bool changed = drag_float(label, v, spd, mn, mx);
                                  return std::make_tuple(v, changed);
                              });

        ui_table.set_function("slider_float",
                              [](const std::string& label, float v, float mn, float mx) {
                                  const bool changed = slider_float(label, v, mn, mx);
                                  return std::make_tuple(v, changed);
                              });

        ui_table.set_function("slider_int",
                              [](const std::string& label, std::int32_t v,
                                 std::int32_t mn, std::int32_t mx) {
                                  const bool changed = slider_int(label, v, mn, mx);
                                  return std::make_tuple(v, changed);
                              });

        ui_table.set_function("input_text",
                              [](const std::string& label, std::string text) {
                                  const bool changed = input_text(label, text);
                                  return std::make_tuple(text, changed);
                              });

        ui_table.set_function("color_edit3",
                              [](const std::string& label, float r, float g, float b) {
                                  const bool changed = color_edit3(label, r, g, b);
                                  return std::make_tuple(r, g, b, changed);
                              });

        ui_table.set_function("separator", &separator);
        ui_table.set_function("same_line", &same_line);
        ui_table.set_function("spacing", &spacing);
        ui_table.set_function("tree_node", &tree_node);
        ui_table.set_function("tree_pop", &tree_pop);
        ui_table.set_function("tab_bar_begin", &tab_bar_begin);
        ui_table.set_function("tab_bar_end", &tab_bar_end);
        ui_table.set_function("tab_item_begin", &tab_item_begin);
        ui_table.set_function("tab_item_end", &tab_item_end);
        ui_table.set_function("collapsing_header", &collapsing_header);
        ui_table.set_function("begin_group", &begin_group);
        ui_table.set_function("end_group", &end_group);
        ui_table.set_function("set_next_window_pos", &set_next_window_pos);
        ui_table.set_function("set_next_window_size", &set_next_window_size);
        ui_table.set_function("set_cursor_pos_x", &set_cursor_pos_x);
        ui_table.set_function("get_window_width", &get_window_width);
        ui_table.set_function("push_style_color", &push_style_color);
        ui_table.set_function("pop_style_color", &pop_style_color);
        ui_table.set_function("push_style_var_float", &push_style_var_float);
        ui_table.set_function("push_style_var_vec2", &push_style_var_vec2);
        ui_table.set_function("pop_style_var", &pop_style_var);
        ui_table.set_function("columns", &columns);
        ui_table.set_function("next_column", &next_column);
        ui_table.set_function("set_column_width", &set_column_width);
        ui_table.set_function("get_delta_time", &get_delta_time);
        ui_table.set_function("get_framerate", &get_framerate);
        ui_table.set_function("want_capture_keyboard", &want_capture_keyboard);
        ui_table.set_function("want_capture_mouse", &want_capture_mouse);
        ui_table.set_function("progress_bar", &progress_bar);
        ui_table.set_function("tooltip", &tooltip);
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

engine::engine(engine&&) noexcept            = default;
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

        const auto result = pimpl_->lua.safe_script_file(path.string());
        if (!result.valid())
        {
            const sol::error err = result;
            sdk::log_error(std::format("failed to load {}: {}",
                                       path.filename().string(),
                                       err.what()));
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
