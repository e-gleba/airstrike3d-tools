/// @file sdk.cpp
/// @brief Lua bindings for SDK core functionality (callbacks, GL, input, logging).

#include "bindings.hpp"

#include "sdk/core/context.hpp"
#include "sdk/util/win32.hpp"

#include <GL/gl.h>
#include <array>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <ranges>
#include <spdlog/spdlog.h>
#include <string>
#include <tuple>
#include <utility>

namespace sdk::lua::bindings
{

namespace detail
{

/// @brief Callback descriptors for automatic registration.
/// @details Maps event names to their corresponding callback_list members.
inline constexpr auto callback_descriptors = std::tuple{
    std::pair{"on_frame", &decltype(g_ctx.cb)::on_frame},
    std::pair{"on_overlay", &decltype(g_ctx.cb)::on_overlay},
    std::pair{"on_gl_identity", &decltype(g_ctx.cb)::on_gl_identity},
    std::pair{"on_glu_lookat", &decltype(g_ctx.cb)::on_glu_lookat},
    std::pair{"on_key_down", &decltype(g_ctx.cb)::on_key_down},
    std::pair{"on_load", &decltype(g_ctx.cb)::on_load},
    std::pair{"on_unload", &decltype(g_ctx.cb)::on_unload},
};

/// @brief Register all callbacks using template metaprogramming.
template <std::size_t... Is>
void register_callbacks_impl(sol::table& tbl, std::index_sequence<Is...>)
{
    (tbl.set_function(std::get<Is>(callback_descriptors).first,
                      [](sol::protected_function f)
                      {
                          (g_ctx.cb.*std::get<Is>(callback_descriptors).second)
                              .add(std::move(f));
                      }),
     ...);
}

/// @brief Entry point for callback registration.
inline void register_callbacks(sol::table& tbl)
{
    register_callbacks_impl(tbl,
                            std::make_index_sequence<
                                std::tuple_size_v<decltype(callback_descriptors)>>{});
}

/// @brief Template wrapper for OpenGL functions with automatic type conversion.
template <auto GlFn, typename... Args>
constexpr auto gl_wrapper = [](Args... args) noexcept
{
    GlFn(static_cast<std::conditional_t<std::is_integral_v<Args>, GLenum, Args>>(args)...);
};

/// @brief Convert a range to a fixed-size array.
template <typename T, std::size_t N, std::ranges::input_range R>
[[nodiscard]] constexpr auto to_array_from_range(R&& rng) -> std::array<T, N>
{
    std::array<T, N> result{};
    std::ranges::copy(std::forward<R>(rng), result.begin());
    return result;
}

} // namespace detail

void register_sdk(sol::state& state)
{
    auto sdk{state.create_named_table("sdk")};

    // Event callbacks
    detail::register_callbacks(sdk);

    // OpenGL state management
    sdk.set_function("gl_enable", detail::gl_wrapper<glEnable, int>);
    sdk.set_function("gl_disable", detail::gl_wrapper<glDisable, int>);
    sdk.set_function("gl_depth_mask",
                     [](const bool flag) noexcept { glDepthMask(flag ? GL_TRUE : GL_FALSE); });
    sdk.set_function("gl_blend_func", detail::gl_wrapper<glBlendFunc, int, int>);
    sdk.set_function("gl_polygon_mode", detail::gl_wrapper<glPolygonMode, int, int>);

    // OpenGL drawing parameters
    sdk.set_function("gl_line_width", [](const float w) noexcept { glLineWidth(w); });
    sdk.set_function("gl_point_size", [](const float sz) noexcept { glPointSize(sz); });
    sdk.set_function("gl_color4f",
                     [](const float r, const float g, const float b, const float a) noexcept
                     { glColor4f(r, g, b, a); });
    sdk.set_function("gl_color3f",
                     [](const float r, const float g, const float b) noexcept
                     { glColor3f(r, g, b); });

    // OpenGL matrix stack
    sdk.set_function("gl_push_attrib",
                     [](const int mask) noexcept { glPushAttrib(static_cast<GLbitfield>(mask)); });
    sdk.set_function("gl_pop_attrib", []() noexcept { glPopAttrib(); });
    sdk.set_function("gl_push_matrix", []() noexcept { glPushMatrix(); });
    sdk.set_function("gl_pop_matrix", []() noexcept { glPopMatrix(); });

    // OpenGL primitive drawing
    sdk.set_function("gl_begin", detail::gl_wrapper<glBegin, int>);
    sdk.set_function("gl_end", []() noexcept { glEnd(); });
    sdk.set_function("gl_vertex3f",
                     [](const float x, const float y, const float z) noexcept
                     { glVertex3f(x, y, z); });
    sdk.set_function("gl_vertex2f",
                     [](const float x, const float y) noexcept { glVertex2f(x, y); });

    // OpenGL transformations
    sdk.set_function("gl_translate",
                     [](const double x, const double y, const double z) noexcept
                     { glTranslated(x, y, z); });
    sdk.set_function("gl_rotate",
                     [](const double angle, const double x, const double y, const double z) noexcept
                     { glRotated(angle, x, y, z); });
    sdk.set_function("gl_scale",
                     [](const double x, const double y, const double z) noexcept
                     { glScaled(x, y, z); });

    // Matrix operations
    const auto load_matrix = []<typename GlFn>(sol::table tbl, GlFn gl_fn)
    {
        static constexpr int k_matrix_size{16};
        if (std::cmp_less(tbl.size(), k_matrix_size))
        {
            return;
        }

        auto mat{detail::to_array_from_range<GLdouble, k_matrix_size>(
            std::views::iota(1, k_matrix_size + 1) |
            std::views::transform([&](const int i) -> GLdouble { return tbl[i].template get<double>(); }))};

        gl_fn(mat.data());
    };

    sdk.set_function("gl_mult_matrix_d",
                     [=](sol::table tbl) { load_matrix(std::move(tbl), glMultMatrixd); });
    sdk.set_function("gl_load_matrix_d",
                     [=](sol::table tbl) { load_matrix(std::move(tbl), glLoadMatrixd); });

    sdk.set_function("gl_apply_lookat",
                     [](const double ex, const double ey, const double ez,
                        const double cx, const double cy, const double cz,
                        const double ux, const double uy, const double uz) noexcept
                     {
                         auto view{glm::lookAt(glm::dvec3{ex, ey, ez},
                                               glm::dvec3{cx, cy, cz},
                                               glm::dvec3{ux, uy, uz})};
                         glMultMatrixd(glm::value_ptr(view));
                     });

    // Input functions
    sdk.set_function("is_key_down", [](const int vk) noexcept -> bool { return win32::is_key_down(vk); });
    sdk.set_function("get_cursor_pos", []() noexcept { return win32::cursor_pos(); });
    sdk.set_function("set_cursor_pos",
                     [](const int x, const int y) noexcept { SetCursorPos(x, y); });
    sdk.set_function("show_cursor", [](const bool v) noexcept { ShowCursor(v ? TRUE : FALSE); });
    sdk.set_function("get_window_rect",
                     []() noexcept { return win32::window_rect(g_ctx.window); });

    sdk.set_function("send_chars",
                     [](const std::string& text)
                     {
                         if (!g_ctx.window)
                         {
                             return;
                         }
                         for (const char c : text)
                         {
                             PostMessageA(g_ctx.window, WM_CHAR, static_cast<WPARAM>(c), 0);
                         }
                     });

    // Logging functions
    sdk.set_function("log_info", [](const std::string& msg) { spdlog::info("[lua] {}", msg); });
    sdk.set_function("log_warn", [](const std::string& msg) { spdlog::warn("[lua] {}", msg); });
    sdk.set_function("log_error", [](const std::string& msg) { spdlog::error("[lua] {}", msg); });

    // Utility functions
    sdk.set_function("get_log_dir", []() noexcept -> std::string { return "logs"; });
}

} // namespace sdk::lua::bindings
