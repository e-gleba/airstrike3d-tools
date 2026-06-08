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
#include <spdlog/spdlog.h>
#include <string>
#include <utility>

namespace sdk::lua
{

namespace detail
{

// ─── sol2 → std::function wrappers ──────────────────────────────────────────
//
// Converts sol::protected_function into std::function with concrete signatures.
// Error checking happens at the boundary — consumers never see sol2 types.

inline auto wrap_void(sol::protected_function fn) -> std::function<void()>
{
    return [fn = std::move(fn)]()
    {
        auto r = fn();
        if (!r.valid())
        {
            sol::error err = r;
            spdlog::error("[sdk] lua callback error: {}", err.what());
        }
    };
}

inline auto wrap_bool_int(sol::protected_function fn) -> std::function<bool(int)>
{
    return [fn = std::move(fn)](int v) -> bool
    {
        auto r = fn(v);
        if (!r.valid())
        {
            sol::error err = r;
            spdlog::error("[sdk] lua callback error: {}", err.what());
            return false;
        }
        return sol::optional<bool>{ r }.value_or(false);
    };
}

inline auto wrap_bool_9d(sol::protected_function fn)
    -> std::function<bool(double, double, double, double, double, double, double, double, double)>
{
    return [fn = std::move(fn)](double a, double b, double c,
                                double d, double e, double f,
                                double g, double h, double i) -> bool
    {
        auto r = fn(a, b, c, d, e, f, g, h, i);
        if (!r.valid())
        {
            sol::error err = r;
            spdlog::error("[sdk] lua callback error: {}", err.what());
            return false;
        }
        return sol::optional<bool>{ r }.value_or(false);
    };
}

// ─── Callback registration ──────────────────────────────────────────────────

inline void register_callbacks(sol::table& s)
{
    s.set_function("on_frame", [](sol::protected_function fn) {
        g_ctx.callbacks.add_on_frame(wrap_void(std::move(fn)));
    });
    s.set_function("on_overlay", [](sol::protected_function fn) {
        g_ctx.callbacks.add_on_overlay(wrap_void(std::move(fn)));
    });
    s.set_function("on_gl_identity", [](sol::protected_function fn) {
        g_ctx.callbacks.add_on_gl_identity(wrap_void(std::move(fn)));
    });
    s.set_function("on_glu_lookat", [](sol::protected_function fn) {
        g_ctx.callbacks.add_on_glu_lookat(wrap_bool_9d(std::move(fn)));
    });
    s.set_function("on_key_down", [](sol::protected_function fn) {
        g_ctx.callbacks.add_on_key_down(wrap_bool_int(std::move(fn)));
    });
    s.set_function("on_load", [](sol::protected_function fn) {
        g_ctx.callbacks.add_on_load(wrap_void(std::move(fn)));
    });
    s.set_function("on_unload", [](sol::protected_function fn) {
        g_ctx.callbacks.add_on_unload(wrap_void(std::move(fn)));
    });
}

// ─── GL wrapper utilities ────────────────────────────────────────────────────

template <auto GlFn, typename... Args>
constexpr auto gl_wrap = [](Args... args) noexcept
{
    GlFn(
        static_cast<std::conditional_t<std::is_integral_v<Args>, GLenum, Args>>(
            args)...);
};

template <typename T, std::size_t N, std::ranges::input_range R>
[[nodiscard]] constexpr auto to_array_from_range(R&& rng) -> std::array<T, N>
{
    std::array<T, N> result{};
    std::ranges::copy(std::forward<R>(rng), result.begin());
    return result;
}

} // namespace detail

void register_sdk_bindings(sol::state& sol_state)
{
    auto s{ sol_state.create_named_table("sdk") };

    detail::register_callbacks(s);

    // ─── OpenGL wrappers ─────────────────────────────────────────────────────

    s.set_function("gl_enable", detail::gl_wrap<glEnable, int>);
    s.set_function("gl_disable", detail::gl_wrap<glDisable, int>);
    s.set_function("gl_depth_mask",
                   [](bool flag) noexcept { glDepthMask(flag ? GL_TRUE : GL_FALSE); });
    s.set_function("gl_blend_func", detail::gl_wrap<glBlendFunc, int, int>);
    s.set_function("gl_line_width", [](float w) noexcept { glLineWidth(w); });
    s.set_function("gl_point_size", [](float sz) noexcept { glPointSize(sz); });
    s.set_function("gl_color4f",
                   [](float r, float g, float b, float a) noexcept { glColor4f(r, g, b, a); });
    s.set_function("gl_color3f",
                   [](float r, float g, float b) noexcept { glColor3f(r, g, b); });
    s.set_function("gl_polygon_mode", detail::gl_wrap<glPolygonMode, int, int>);
    s.set_function("gl_push_attrib",
                   [](int mask) noexcept { glPushAttrib(static_cast<GLbitfield>(mask)); });
    s.set_function("gl_pop_attrib", []() noexcept { glPopAttrib(); });
    s.set_function("gl_push_matrix", []() noexcept { glPushMatrix(); });
    s.set_function("gl_pop_matrix", []() noexcept { glPopMatrix(); });
    s.set_function("gl_begin", detail::gl_wrap<glBegin, int>);
    s.set_function("gl_end", []() noexcept { glEnd(); });
    s.set_function("gl_vertex3f",
                   [](float x, float y, float z) noexcept { glVertex3f(x, y, z); });
    s.set_function("gl_vertex2f", [](float x, float y) noexcept { glVertex2f(x, y); });
    s.set_function("gl_translate",
                   [](double x, double y, double z) noexcept { glTranslated(x, y, z); });
    s.set_function("gl_rotate",
                   [](double angle, double x, double y, double z) noexcept
                   { glRotated(angle, x, y, z); });
    s.set_function("gl_scale",
                   [](double x, double y, double z) noexcept { glScaled(x, y, z); });

    // ─── Input ───────────────────────────────────────────────────────────────

    s.set_function("is_key_down",
                   [](int vk) noexcept -> bool { return win32::is_key_down(vk); });
    s.set_function("get_cursor_pos", []() noexcept { return win32::cursor_pos(); });
    s.set_function("set_cursor_pos", [](int x, int y) noexcept { SetCursorPos(x, y); });
    s.set_function("show_cursor", [](bool v) noexcept { ShowCursor(v ? TRUE : FALSE); });
    s.set_function("get_window_rect",
                   []() noexcept { return win32::window_rect(g_ctx.window); });

    s.set_function(
        "send_chars",
        [](const std::string& text)
        {
            if (!g_ctx.window)
            {
                return;
            }
            for (char c : text)
            {
                PostMessageA(g_ctx.window, WM_CHAR, static_cast<WPARAM>(c), 0);
            }
        });

    // ─── Matrix operations ───────────────────────────────────────────────────

    auto load_matrix = []<typename GlFn>(sol::table t, GlFn gl_fn)
    {
        static constexpr int k_matrix_size{ 16 };
        if (std::cmp_less(t.size(), k_matrix_size))
        {
            return;
        }

        auto m{ detail::to_array_from_range<GLdouble, k_matrix_size>(
            std::views::iota(1, k_matrix_size + 1) |
            std::views::transform([&](int i) -> GLdouble
                                  { return t[i].template get<double>(); })) };

        gl_fn(m.data());
    };

    s.set_function("gl_mult_matrix_d",
                   [=](sol::table t) { load_matrix(std::move(t), glMultMatrixd); });
    s.set_function("gl_load_matrix_d",
                   [=](sol::table t) { load_matrix(std::move(t), glLoadMatrixd); });

    s.set_function("gl_apply_lookat",
                   [](double ex,
                      double ey,
                      double ez,
                      double cx,
                      double cy,
                      double cz,
                      double ux,
                      double uy,
                      double uz) noexcept
                   {
                       auto view{ glm::lookAt(glm::dvec3{ ex, ey, ez },
                                              glm::dvec3{ cx, cy, cz },
                                              glm::dvec3{ ux, uy, uz }) };
                       glMultMatrixd(glm::value_ptr(view));
                   });

    // ─── Logging ─────────────────────────────────────────────────────────────

    s.set_function("log_info",
                   [](const std::string& m) { spdlog::info("[lua] {}", m); });
    s.set_function("log_warn",
                   [](const std::string& m) { spdlog::warn("[lua] {}", m); });
    s.set_function("log_error",
                   [](const std::string& m) { spdlog::error("[lua] {}", m); });

    s.set_function("get_log_dir", []() noexcept -> std::string { return "logs"; });
}

} // namespace sdk::lua
