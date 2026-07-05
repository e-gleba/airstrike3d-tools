#include "sdk/core/context.hpp"
#include "sdk/core/detail/context_state.hpp"

#include <GL/gl.h>
#include <concepts>
#include <type_traits>
#include <utility>

namespace sdk::detail
{

using gl_matrix_mode_fn   = void(APIENTRY*)(GLenum);
using gl_load_identity_fn = void(APIENTRY*)();
using glu_look_at_fn      = void(APIENTRY*)(GLdouble,
                                            GLdouble,
                                            GLdouble,
                                            GLdouble,
                                            GLdouble,
                                            GLdouble,
                                            GLdouble,
                                            GLdouble,
                                            GLdouble);

template <typename Fn, typename Hook, typename... Args>
    requires std::is_pointer_v<Fn> &&
             std::is_function_v<std::remove_pointer_t<Fn>>
inline void call_if_hooked(Hook& hook, Args&&... args)
{
    if (hook)
    {
        call_orig<Fn>(hook)(std::forward<Args>(args)...);
    }
}

} // namespace sdk::detail

namespace sdk::gl
{

void APIENTRY hk_gl_matrix_mode(GLenum mode)
{
    g_ctx.current_matrix_mode.store(static_cast<matrix_mode>(mode));
    detail::call_if_hooked<detail::gl_matrix_mode_fn>(
        detail::g_state.hooks.gl_matrix_mode, mode);
}

void APIENTRY hk_gl_load_identity()
{
    detail::call_if_hooked<detail::gl_load_identity_fn>(
        detail::g_state.hooks.gl_load_identity);

    if (g_ctx.current_matrix_mode.load() == static_cast<matrix_mode>(GL_MODELVIEW))
    {
        g_ctx.cb.on_gl_identity.invoke(g_ctx.current_matrix_mode.load());
    }
}

void APIENTRY hk_glu_look_at(GLdouble ex,
                             GLdouble ey,
                             GLdouble ez,
                             GLdouble cx,
                             GLdouble cy,
                             GLdouble cz,
                             GLdouble ux,
                             GLdouble uy,
                             GLdouble uz)
{
    const auto consumed = g_ctx.cb.on_glu_lookat.invoke(
        ex, ey, ez, cx, cy, cz, ux, uy, uz);

    if (!consumed)
    {
        detail::call_if_hooked<detail::glu_look_at_fn>(
            detail::g_state.hooks.glu_look_at,
            ex, ey, ez, cx, cy, cz, ux, uy, uz);
    }
}

} // namespace sdk::gl
