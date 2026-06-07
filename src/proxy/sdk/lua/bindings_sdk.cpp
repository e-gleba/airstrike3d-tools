#include "bindings_sdk.hpp"

#include "sdk/core/context.hpp"
#include "sdk/util/win32.hpp"

#include <GL/gl.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <array>
#include <functional>
#include <ranges>
#include <sol/sol.hpp>
#include <string>
#include <tuple>
#include <utility>

namespace sdk::lua
{

namespace detail
{

template <std::size_t... Is>
void register_callbacks(sol::table& sdk_table, std::index_sequence<Is...>)
{
    constexpr std::tuple callback_names{
        "on_frame",
        "on_overlay",
        "on_gl_identity",
        "on_glu_lookat",
        "on_key_down",
        "on_load",
        "on_unload",
    };

    (sdk_table.set_function(std::get<Is>(callback_names),
                            [name = std::get<Is>(callback_names)](sol::protected_function fn) {
                                // Convert sol2 function → std::function at boundary.
                                // Captures fn by value (sol2 ref-counted).
                                if constexpr (Is == 0 || Is == 1 || Is == 2 || Is == 5 || Is == 6)
                                {
                                    // void()
                                    g_ctx.callbacks.add(name, [fn = std::move(fn)]() mutable {
                                        fn();
                                    });
                                }
                                else if constexpr (Is == 4)
                                {
                                    // void(int)
                                    g_ctx.callbacks.add(name, [fn = std::move(fn)](int key) mutable {
                                        fn(key);
                                    });
                                }
                                else if constexpr (Is == 3)
                                {
                                    // void(double×9)
                                    g_ctx.callbacks.add(name,
                                                        [fn = std::move(fn)](double eyeX,
                                                                             double eyeY,
                                                                             double eyeZ,
                                                                             double centerX,
                                                                             double centerY,
                                                                             double centerZ,
                                                                             double upX,
                                                                             double upY,
                                                                             double upZ) mutable {
                                                            fn(eyeX, eyeY, eyeZ, centerX, centerY, centerZ, upX, upY, upZ);
                                                        });
                                }
                            }),
     ...);
}

void register_callbacks(sol::table& sdk_table)
{
    register_callbacks(sdk_table, std::make_index_sequence<7>{});
}

template <auto GlFn, typename... Args>
constexpr auto gl_wrap = [](Args... args) noexcept {
    GlFn(static_cast<std::conditional_t<std::is_integral_v<Args>, GLenum, Args>>(args)...);
};

template <typename T, std::size_t N, std::ranges::input_range R>
[[nodiscard]] constexpr auto to_array_from_range(R&& rng) -> std::array<T, N>
{
    std::array<T, N> result{};
    std::ranges::copy(std::forward<R>(rng), result.begin());
    return result;
}

} // namespace detail

void register_sdk_bindings(sol::state& lua)
{
    auto sdk_table = lua.create_named_table("sdk");

    detail::register_callbacks(sdk_table);

    // GL wrappers
    sdk_table.set_function("gl_enable", detail::gl_wrap<glEnable, int>);
    sdk_table.set_function("gl_disable", detail::gl_wrap<glDisable, int>);
    sdk_table.set_function("gl_depth_mask", [](bool flag) noexcept { glDepthMask(flag ? GL_TRUE : GL_FALSE); });
    sdk_table.set_function("gl_blend_func", detail::gl_wrap<glBlendFunc, int, int>);
    sdk_table.set_function("gl_line_width", [](float w) noexcept { glLineWidth(w); });
    sdk_table.set_function("gl_point_size", [](float sz) noexcept { glPointSize(sz); });
    sdk_table.set_function("gl_color4f", [](float r, float g, float b, float a) noexcept { glColor4f(r, g, b, a); });
    sdk_table.set_function("gl_color3f", [](float r, float g, float b) noexcept { glColor3f(r, g, b); });
    sdk_table.set_function("gl_polygon_mode", detail::gl_wrap<glPolygonMode, int, int>);
    sdk_table.set_function("gl_push_attrib", [](int mask) noexcept { glPushAttrib(static_cast<GLbitfield>(mask)); });
    sdk_table.set_function("gl_pop_attrib", []() noexcept { glPopAttrib(); });
    sdk_table.set_function("gl_push_matrix", []() noexcept { glPushMatrix(); });
    sdk_table.set_function("gl_pop_matrix", []() noexcept { glPopMatrix(); });
    sdk_table.set_function("gl_begin", detail::gl_wrap<glBegin, int>);
    sdk_table.set_function("gl_end", []() noexcept { glEnd(); });
    sdk_table.set_function("gl_vertex3f", [](float x, float y, float z) noexcept { glVertex3f(x, y, z); });
    sdk_table.set_function("gl_vertex2f", [](float x, float y) noexcept { glVertex2f(x, y); });
    sdk_table.set_function("gl_translate", [](double x, double y, double z) noexcept { glTranslated(x, y, z); });
    sdk_table.set_function("gl_rotate",
                           [](double angle, double x, double y, double z) noexcept { glRotated(angle, x, y, z); });
    sdk_table.set_function("gl_scale", [](double x, double y, double z) noexcept { glScaled(x, y, z); });

    // Input
    sdk_table.set_function("is_key_down", [](int vk) noexcept -> bool { return win32::is_key_down(vk); });
    sdk_table.set_function("get_cursor_pos", []() noexcept { return win32::cursor_pos(); });
    sdk_table.set_function("set_cursor_pos", [](int x, int y) noexcept { SetCursorPos(x, y); });
    sdk_table.set_function("show_cursor", [](bool v) noexcept { ShowCursor(v ? TRUE : FALSE); });
    sdk_table.set_function("get_window_rect", []() noexcept { return win32::window_rect(g_ctx.window); });

    sdk_table.set_function("send_chars", [](const std::string& text) {
        if (!g_ctx.window)
        {
            return;
        }
        for (char c : text)
        {
            PostMessageA(g_ctx.window, WM_CHAR, static_cast<WPARAM>(c), 0);
        }
    });

    // Matrix operations
    auto load_matrix = []<typename GlFn>(sol::table t, GlFn gl_fn) {
        static constexpr int k_matrix_size{ 16 };
        if (std::cmp_less(t.size(), k_matrix_size))
        {
            return;
        }

        auto m = detail::to_array_from_range<GLdouble, k_matrix_size>(
            std::views::iota(1, k_matrix_size + 1) |
            std::views::transform([&](int i) -> GLdouble { return t[i].template get<double>(); }));

        gl_fn(m.data());
    };

    sdk_table.set_function("gl_mult_matrix_d", [=](sol::table t) { load_matrix(std::move(t), glMultMatrixd); });
    sdk_table.set_function("gl_load_matrix_d", [=](sol::table t) { load_matrix(std::move(t), glLoadMatrixd); });

    sdk_table.set_function("gl_apply_lookat",
                           [](double ex, double ey, double ez, double cx, double cy, double cz, double ux, double uy, double uz) noexcept {
                               auto view = glm::lookAt(glm::dvec3{ ex, ey, ez }, glm::dvec3{ cx, cy, cz }, glm::dvec3{ ux, uy, uz });
                               glMultMatrixd(glm::value_ptr(view));
                           });

    // Logging
    sdk_table.set_function("log_info", [](const std::string& m) { spdlog::info("[lua] {}", m); });
    sdk_table.set_function("log_warn", [](const std::string& m) { spdlog::warn("[lua] {}", m); });
    sdk_table.set_function("log_error", [](const std::string& m) { spdlog::error("[lua] {}", m); });

    sdk_table.set_function("get_log_dir", []() noexcept -> std::string { return "logs"; });
}

} // namespace sdk::lua
