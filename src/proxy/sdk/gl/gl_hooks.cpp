#include "gl_hooks.hpp"
#include "sdk/core/context.hpp"

#include <concepts>
#include <type_traits>
#include <utility>

namespace sdk::gl
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

void APIENTRY hk_gl_matrix_mode(GLenum mode)
{
    g_ctx.current_matrix_mode = mode;
    call_if_hooked<gl_matrix_mode_fn>(g_ctx.hooks.gl_matrix_mode, mode);
}

void APIENTRY hk_gl_load_identity()
{
    call_if_hooked<gl_load_identity_fn>(g_ctx.hooks.gl_load_identity);

    if (g_ctx.current_matrix_mode == GL_MODELVIEW)
    {
        g_ctx.callbacks.invoke<event::on_gl_identity, void()>();
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
    auto consumed{ g_ctx.callbacks.invoke_glu_lookat_consuming(
        ex, ey, ez, cx, cy, cz, ux, uy, uz) };

    if (!consumed)
    {
        call_if_hooked<glu_look_at_fn>(
            g_ctx.hooks.glu_look_at, ex, ey, ez, cx, cy, cz, ux, uy, uz);
    }
}

} // namespace sdk::gl
