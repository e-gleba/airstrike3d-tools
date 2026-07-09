#include "sdk/core/context.hpp"
#include "sdk/core/logging.hpp"
#include "sdk/graphics/detail/camera_math.hpp"
#include "sdk/graphics/detail/opengl_state.hpp"
#include "sdk/graphics/graphics.hpp"
#include "sdk/graphics/rendering.hpp"

#include <GL/gl.h>
#include <exception>
#include <format>
#include <type_traits>
#include <utility>

namespace sdk::gl::detail
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

bool g_scene_state_pushed{};
bool g_world_lines_drawn{};
bool g_internal_gl{};

class internal_gl_scope final
{
public:
    internal_gl_scope() noexcept
        : previous_{ std::exchange(g_internal_gl, true) }
    {
    }

    ~internal_gl_scope() { g_internal_gl = previous_; }

    internal_gl_scope(const internal_gl_scope&)            = delete;
    internal_gl_scope& operator=(const internal_gl_scope&) = delete;

private:
    bool previous_;
};

void reset_modelview()
{
    if (const auto load_identity =
            call_orig<gl_load_identity_fn>(g_hooks.gl_load_identity))
    {
        load_identity();
        return;
    }

    internal_gl_scope scope;
    glLoadIdentity();
}

void apply_camera()
{
    if (!graphics::camera_enabled())
    {
        return;
    }

    try
    {
        // Replace modelview. gluLookAt multiplies, so reset first.
        reset_modelview();

        const auto pose    = graphics::get_camera_pose();
        const auto forward = graphics::detail::basis_from_pose(pose).forward;
        graphics::apply_lookat(pose.position.x,
                               pose.position.y,
                               pose.position.z,
                               pose.position.x + forward.x,
                               pose.position.y + forward.y,
                               pose.position.z + forward.z,
                               0.0,
                               1.0,
                               0.0);
    }
    catch (const std::exception& error)
    {
        sdk::log_error(
            std::format("OpenGL camera override failed: {}", error.what()));
    }
}

void apply_visual_mode()
{
    const auto settings = graphics::get_visual_settings();
    if (settings.mode == graphics::visual_mode::disabled ||
        g_scene_state_pushed)
    {
        return;
    }

    glPushAttrib(GL_ALL_ATTRIB_BITS);
    g_scene_state_pushed = true;

    switch (settings.mode)
    {
        case graphics::visual_mode::xray:
            glDisable(GL_DEPTH_TEST);
            glDepthMask(GL_FALSE);
            break;
        case graphics::visual_mode::wireframe:
            glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
            break;
        case graphics::visual_mode::ghost:
            glDisable(GL_DEPTH_TEST);
            glDepthMask(GL_FALSE);
            glEnable(GL_BLEND);
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
            glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
            break;
        case graphics::visual_mode::depth_bias:
            glEnable(GL_POLYGON_OFFSET_FILL);
            glPolygonOffset(1.0F, settings.depth_bias);
            break;
        case graphics::visual_mode::disabled:
            break;
    }
}

void draw_world_lines()
{
    if (g_world_lines_drawn)
    {
        return;
    }

    const auto batch = graphics::detail::world_lines_snapshot();
    if (batch->vertices.empty())
    {
        return;
    }

    glPushAttrib(GL_ALL_ATTRIB_BITS);
    glDisable(GL_TEXTURE_2D);
    glDisable(GL_LIGHTING);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    if (!batch->settings.depth_test)
    {
        glDisable(GL_DEPTH_TEST);
    }
    glBegin(GL_LINES);
    for (const auto& vertex : batch->vertices)
    {
        const auto color = vertex.argb;
        glColor4ub(static_cast<GLubyte>((color >> 16U) & 0xFFU),
                   static_cast<GLubyte>((color >> 8U) & 0xFFU),
                   static_cast<GLubyte>(color & 0xFFU),
                   static_cast<GLubyte>((color >> 24U) & 0xFFU));
        glVertex3f(vertex.x, vertex.y, vertex.z);
    }
    glEnd();
    glPopAttrib();
    g_world_lines_drawn = true;
}

} // namespace sdk::gl::detail

namespace sdk::gl
{

void APIENTRY hk_gl_matrix_mode(GLenum mode)
{
    g_ctx.current_matrix_mode.store(static_cast<matrix_mode>(mode));
    detail::call_if_hooked<detail::gl_matrix_mode_fn>(
        detail::g_hooks.gl_matrix_mode, mode);
}

void APIENTRY hk_gl_load_identity()
{
    detail::call_if_hooked<detail::gl_load_identity_fn>(
        detail::g_hooks.gl_load_identity);

    if (detail::g_internal_gl)
    {
        return;
    }

    if (g_ctx.current_matrix_mode.load() ==
        static_cast<matrix_mode>(GL_MODELVIEW))
    {
        detail::apply_camera();
        detail::apply_visual_mode();
        detail::draw_world_lines();
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
    try
    {
        if (const auto observed = graphics::detail::pose_from_look_at(
                { ex, ey, ez }, { cx, cy, cz }))
        {
            graphics::detail::observe_camera(*observed);
        }
    }
    catch (const std::exception& error)
    {
        sdk::log_error(
            std::format("OpenGL camera observe failed: {}", error.what()));
    }

    if (graphics::camera_enabled())
    {
        detail::apply_camera();
        return;
    }

    const auto consumed =
        g_ctx.cb.on_glu_lookat.invoke(ex, ey, ez, cx, cy, cz, ux, uy, uz);

    if (!consumed)
    {
        detail::call_if_hooked<detail::glu_look_at_fn>(
            detail::g_hooks.glu_look_at, ex, ey, ez, cx, cy, cz, ux, uy, uz);
    }
}

void finish_frame() noexcept
{
    if (detail::g_scene_state_pushed)
    {
        glPopAttrib();
        detail::g_scene_state_pushed = false;
    }
    detail::g_world_lines_drawn = false;
}

} // namespace sdk::gl
