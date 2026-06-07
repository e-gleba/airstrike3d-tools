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
#include <tuple>
#include <utility>

namespace sdk::lua
{

namespace detail
{

// ─── Descriptor table: event → callback_list add method ──────────────────────
//
// Each entry: { event enum, pointer-to-member of callback_list::add_* method }.
// The add_* methods accept std::function with the correct signature.
// Fold expression expands all registrations in a single expression.

inline constexpr auto callback_descriptors = std::tuple{
    std::pair{ event::on_frame,       &callback_list::add_on_frame },
    std::pair{ event::on_overlay,     &callback_list::add_on_overlay },
    std::pair{ event::on_gl_identity, &callback_list::add_on_gl_identity },
    std::pair{ event::on_glu_lookat,  &callback_list::add_on_glu_lookat },
    std::pair{ event::on_key_down,    &callback_list::add_on_key_down },
    std::pair{ event::on_load,        &callback_list::add_on_load },
    std::pair{ event::on_unload,      &callback_list::add_on_unload },
};

/// Bind a single event: wraps sol::protected_function into std::function with error checking,
/// then forwards to the appropriate callback_list::add_* method via pointer-to-member.
template <typename AddPmf>
void bind_event(sol::table& s, event ev, AddPmf add_method)
{
    s.set_function(
        std::string{ to_string_view(ev) },
        [add_method](sol::protected_function fn)
        {
            // Extract the std::function parameter type from the add_* method signature.
            // AddPmf is void(callback_list::*)(std::function<Sig>)
            // We deduce Sig via the lambda below.
            using pmf_traits = decltype([]<typename C, typename Sig>(void (C::*)(Sig))
                                        -> Sig {});
            using fn_type = decltype(pmf_traits{}(add_method));

            auto wrapper = fn_type{ [fn = std::move(fn)](auto&&... args) mutable
            {
                auto result = fn(std::forward<decltype(args)>(args)...);
                if constexpr (std::is_void_v<decltype(result)>)
                {
                    return;
                }
                else
                {
                    if (!result.valid())
                    {
                        sol::error err = result;
                        spdlog::error("[sdk] lua callback error: {}", err.what());
                    }
                    return sol::optional<bool>{ result }.value_or(false);
                }
            } };

            (g_ctx.callbacks.*add_method)(std::move(wrapper));
        });
}

/// Expand descriptor table and register all events via fold expression.
template <std::size_t... Is>
void register_callbacks(sol::table& s, std::index_sequence<Is...>)
{
    (bind_event(s,
                std::get<Is>(callback_descriptors).first,
                std::get<Is>(callback_descriptors).second),
     ...);
}

inline void register_callbacks(sol::table& s)
{
    register_callbacks(
        s, std::make_index_sequence<std::tuple_size_v<decltype(callback_descriptors)>>{});
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
