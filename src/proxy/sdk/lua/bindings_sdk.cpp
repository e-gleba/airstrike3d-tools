
#include "sdk/core/context.hpp"
#include "sdk/util/win32.hpp"
#include <glad/glad.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <array>
#include <ranges>
#include <string>
#include <tuple>
#include <utility>

namespace sdk::lua
{

namespace detail
{

inline constexpr auto callback_descriptors = std::tuple{
    std::pair{ "on_frame", &decltype(g_ctx.cb)::on_frame },
    std::pair{ "on_overlay", &decltype(g_ctx.cb)::on_overlay },
    std::pair{ "on_gl_identity", &decltype(g_ctx.cb)::on_gl_identity },
    std::pair{ "on_glu_lookat", &decltype(g_ctx.cb)::on_glu_lookat },
    std::pair{ "on_key_down", &decltype(g_ctx.cb)::on_key_down },
    std::pair{ "on_load", &decltype(g_ctx.cb)::on_load },
    std::pair{ "on_unload", &decltype(g_ctx.cb)::on_unload },
};

template <std::size_t... Is>
void register_callbacks(sol::table& s, std::index_sequence<Is...>)
{
    (s.set_function(std::get<Is>(callback_descriptors).first,
                    [](sol::protected_function f)
                    {
                        (g_ctx.cb.*std::get<Is>(callback_descriptors).second)
                            .add(std::move(f));
                    }),
     ...);
}

inline void register_callbacks(sol::table& s)
{
    register_callbacks(
        s,
        std::make_index_sequence<
            std::tuple_size_v<decltype(callback_descriptors)>>{});
}

template <typename T, std::size_t N, std::ranges::input_range R>
constexpr auto to_array_from_range(R&& rng) -> std::array<T, N>
{
    std::array<T, N> result{};
    std::ranges::copy(std::forward<R>(rng), result.begin());
    return result;
}

} // namespace detail

// ═══════════════════════════════════════════════════════════════════════
void register_sdk_bindings(sol::state& sol_state)
{
    auto s = sol_state.create_named_table("sdk");

    // ── Callbacks ──
    detail::register_callbacks(s);

    // ── Raw GL state control ──
    // Plain lambdas: glad.h defines gl* names as macros expanding to runtime
    // variables (glad_gl*), so they cannot be used as non-type template args.
    s.set_function("gl_enable", [](int cap) { glEnable(static_cast<GLenum>(cap)); });
    s.set_function("gl_disable", [](int cap) { glDisable(static_cast<GLenum>(cap)); });
    s.set_function("gl_depth_mask",
                   [](bool flag) { glDepthMask(flag ? GL_TRUE : GL_FALSE); });
    s.set_function("gl_blend_func",
                   [](int sfactor, int dfactor)
                   { glBlendFunc(static_cast<GLenum>(sfactor), static_cast<GLenum>(dfactor)); });
    s.set_function("gl_line_width", [](float w) { glLineWidth(w); });
    s.set_function("gl_point_size", [](float sz) { glPointSize(sz); });
    s.set_function("gl_color4f",
                   [](float r, float g, float b, float a)
                   { glColor4f(r, g, b, a); });
    s.set_function("gl_color3f",
                   [](float r, float g, float b) { glColor3f(r, g, b); });
    s.set_function("gl_polygon_mode",
                   [](int face, int mode)
                   { glPolygonMode(static_cast<GLenum>(face), static_cast<GLenum>(mode)); });
    s.set_function("gl_push_attrib",
                   [](int mask)
                   { glPushAttrib(static_cast<GLbitfield>(mask)); });
    s.set_function("gl_pop_attrib", [] { glPopAttrib(); });
    s.set_function("gl_push_matrix", [] { glPushMatrix(); });
    s.set_function("gl_pop_matrix", [] { glPopMatrix(); });
    s.set_function("gl_begin", [](int mode) { glBegin(static_cast<GLenum>(mode)); });
    s.set_function("gl_end", [] { glEnd(); });
    s.set_function("gl_vertex3f",
                   [](float x, float y, float z) { glVertex3f(x, y, z); });
    s.set_function("gl_vertex2f", [](float x, float y) { glVertex2f(x, y); });
    s.set_function("gl_translate",
                   [](double x, double y, double z) { glTranslated(x, y, z); });
    s.set_function("gl_rotate",
                   [](double angle, double x, double y, double z)
                   { glRotated(angle, x, y, z); });
    s.set_function("gl_scale",
                   [](double x, double y, double z) { glScaled(x, y, z); });

    // ── Input ──
    s.set_function("is_key_down",
                   [](int vk) -> bool { return win32::is_key_down(vk); });
    s.set_function("get_cursor_pos", [] { return win32::cursor_pos(); });
    s.set_function("set_cursor_pos", [](int x, int y) { SetCursorPos(x, y); });
    s.set_function("show_cursor", [](bool v) { ShowCursor(v ? TRUE : FALSE); });
    s.set_function("get_window_rect",
                   [] { return win32::window_rect(g_ctx.window); });

    // ── Window messages ──
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

    // ── OpenGL matrix helpers ──
    auto load_matrix = []<typename GlFn>(sol::table t, GlFn gl_fn)
    {
        static constexpr auto k_matrix_size = 16;
        if (std::cmp_less(t.size(), k_matrix_size))
        {
            return;
        }

        auto m = detail::to_array_from_range<GLdouble, k_matrix_size>(
            std::views::iota(1, k_matrix_size + 1) |
            std::views::transform([&](int i) -> GLdouble
                                  { return t[i].template get<double>(); }));

        gl_fn(m.data());
    };

    s.set_function("gl_mult_matrix_d",
                   [=](sol::table t)
                   { load_matrix(std::move(t), glMultMatrixd); });
    s.set_function("gl_load_matrix_d",
                   [=](sol::table t)
                   { load_matrix(std::move(t), glLoadMatrixd); });

    s.set_function("gl_apply_lookat",
                   [](double ex,
                      double ey,
                      double ez,
                      double cx,
                      double cy,
                      double cz,
                      double ux,
                      double uy,
                      double uz)
                   {
                       auto view = glm::lookAt(glm::dvec3{ ex, ey, ez },
                                               glm::dvec3{ cx, cy, cz },
                                               glm::dvec3{ ux, uy, uz });
                       glMultMatrixd(glm::value_ptr(view));
                   });

    // ── Logging ──
    s.set_function("log_info",
                   [](const std::string& m) { spdlog::info("[lua] {}", m); });
    s.set_function("log_warn",
                   [](const std::string& m) { spdlog::warn("[lua] {}", m); });
    s.set_function("log_error",
                   [](const std::string& m) { spdlog::error("[lua] {}", m); });

    s.set_function("get_log_dir", [] -> std::string { return "logs"; });
}

} // namespace sdk::lua
