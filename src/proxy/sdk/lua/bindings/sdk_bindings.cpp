#include "sdk/lua/bindings/bindings_fwd.hpp"

#include "sdk/core/context.hpp"
#include "sdk/lua/detail/callback.hpp"
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

inline constexpr auto callback_descriptors = std::tuple{
    std::pair{ "on_frame", &callback_registry::on_frame },
    std::pair{ "on_overlay", &callback_registry::on_overlay },
    std::pair{ "on_gl_identity", &callback_registry::on_gl_identity },
    std::pair{ "on_glu_lookat", &callback_registry::on_glu_lookat },
    std::pair{ "on_key_down", &callback_registry::on_key_down },
    std::pair{ "on_load", &callback_registry::on_load },
    std::pair{ "on_unload", &callback_registry::on_unload },
};

template <std::size_t... Is>
void register_callbacks(sol::table& s,
                        callback_registry& cbs,
                        std::index_sequence<Is...>)
{
    (s.set_function(std::get<Is>(callback_descriptors).first,
                    [&cbs](sol::protected_function f)
                    { (cbs.*std::get<Is>(callback_descriptors).second).add(std::move(f)); }),
     ...);
}

inline void register_callbacks(sol::table& s, callback_registry& cbs)
{
    register_callbacks(s,
                       cbs,
                       std::make_index_sequence<
                           std::tuple_size_v<decltype(callback_descriptors)>>{});
}

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

void register_sdk(sol::state& sol_state, detail::callback_registry& callbacks)
{
    auto s{ sol_state.create_named_table("sdk") };

    detail::register_callbacks(s, callbacks);

    s.set_function("gl_enable", detail::gl_wrap<glEnable, int>);
    s.set_function("gl_disable", detail::gl_wrap<glDisable, int>);
    s.set_function("gl_depth_mask",
                   [](bool flag) noexcept
                   { glDepthMask(flag ? GL_TRUE : GL_FALSE); });
    s.set_function("gl_blend_func", detail::gl_wrap<glBlendFunc, int, int>);
    s.set_function("gl_line_width", [](float w) noexcept { glLineWidth(w); });
    s.set_function("gl_point_size", [](float sz) noexcept { glPointSize(sz); });
    s.set_function("gl_color4f",
                   [](float r, float g, float b, float a) noexcept
                   { glColor4f(r, g, b, a); });
    s.set_function("gl_color3f",
                   [](float r, float g, float b) noexcept
                   { glColor3f(r, g, b); });
    s.set_function("gl_polygon_mode", detail::gl_wrap<glPolygonMode, int, int>);
    s.set_function("gl_push_attrib",
                   [](int mask) noexcept
                   { glPushAttrib(static_cast<GLbitfield>(mask)); });
    s.set_function("gl_pop_attrib", []() noexcept { glPopAttrib(); });
    s.set_function("gl_push_matrix", []() noexcept { glPushMatrix(); });
    s.set_function("gl_pop_matrix", []() noexcept { glPopMatrix(); });
    s.set_function("gl_begin", detail::gl_wrap<glBegin, int>);
    s.set_function("gl_end", []() noexcept { glEnd(); });
    s.set_function("gl_vertex3f",
                   [](float x, float y, float z) noexcept
                   { glVertex3f(x, y, z); });
    s.set_function("gl_vertex2f",
                   [](float x, float y) noexcept { glVertex2f(x, y); });
    s.set_function("gl_translate",
                   [](double x, double y, double z) noexcept
                   { glTranslated(x, y, z); });
    s.set_function("gl_rotate",
                   [](double angle, double x, double y, double z) noexcept
                   { glRotated(angle, x, y, z); });
    s.set_function("gl_scale",
                   [](double x, double y, double z) noexcept
                   { glScaled(x, y, z); });

    s.set_function("is_key_down",
                   [](int vk) noexcept -> bool
                   { return win32::is_key_down(vk); });
    s.set_function("get_cursor_pos",
                   []() noexcept { return win32::cursor_pos(); });
    s.set_function("set_cursor_pos",
                   [](int x, int y) noexcept { SetCursorPos(x, y); });
    s.set_function("show_cursor",
                   [](bool v) noexcept { ShowCursor(v ? TRUE : FALSE); });
    s.set_function("get_window_rect",
                   []() noexcept { return win32::window_rect(g_ctx.window); });

    s.set_function("send_chars",
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
                      double uz) noexcept
                   {
                       auto view{ glm::lookAt(glm::dvec3{ ex, ey, ez },
                                              glm::dvec3{ cx, cy, cz },
                                              glm::dvec3{ ux, uy, uz }) };
                       glMultMatrixd(glm::value_ptr(view));
                   });

    s.set_function("log_info",
                   [](const std::string& m) { spdlog::info("[lua] {}", m); });
    s.set_function("log_warn",
                   [](const std::string& m) { spdlog::warn("[lua] {}", m); });
    s.set_function("log_error",
                   [](const std::string& m) { spdlog::error("[lua] {}", m); });

    s.set_function("get_log_dir",
                   []() noexcept -> std::string { return "logs"; });
}

} // namespace sdk::lua::bindings
