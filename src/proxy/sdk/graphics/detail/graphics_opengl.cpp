#include "sdk/graphics/graphics.hpp"

#include "sdk/graphics/detail/opengl_state.hpp"

#include <GL/gl.h>
#include <windows.h>

namespace sdk::graphics
{

void enable(std::int32_t cap) noexcept
{
    glEnable(static_cast<GLenum>(cap));
}

void disable(std::int32_t cap) noexcept
{
    glDisable(static_cast<GLenum>(cap));
}

void depth_mask(bool flag) noexcept
{
    glDepthMask(flag ? GL_TRUE : GL_FALSE);
}

void blend_func(std::int32_t sfactor, std::int32_t dfactor) noexcept
{
    glBlendFunc(static_cast<GLenum>(sfactor), static_cast<GLenum>(dfactor));
}

void line_width(float w) noexcept
{
    glLineWidth(w);
}

void point_size(float sz) noexcept
{
    glPointSize(sz);
}

void color4f(float r, float g, float b, float a) noexcept
{
    glColor4f(r, g, b, a);
}

void color3f(float r, float g, float b) noexcept
{
    glColor3f(r, g, b);
}

void polygon_mode(std::int32_t face, std::int32_t mode) noexcept
{
    glPolygonMode(static_cast<GLenum>(face), static_cast<GLenum>(mode));
}

void push_attrib(std::int32_t mask) noexcept
{
    glPushAttrib(static_cast<GLbitfield>(mask));
}

void pop_attrib() noexcept
{
    glPopAttrib();
}

void push_matrix() noexcept
{
    glPushMatrix();
}

void pop_matrix() noexcept
{
    glPopMatrix();
}

void begin(std::int32_t mode) noexcept
{
    glBegin(static_cast<GLenum>(mode));
}

void end() noexcept
{
    glEnd();
}

void vertex3f(float x, float y, float z) noexcept
{
    glVertex3f(x, y, z);
}

void vertex2f(float x, float y) noexcept
{
    glVertex2f(x, y);
}

void translate(double x, double y, double z) noexcept
{
    glTranslated(x, y, z);
}

void rotate(double angle, double x, double y, double z) noexcept
{
    glRotated(angle, x, y, z);
}

void scale(double x, double y, double z) noexcept
{
    glScaled(x, y, z);
}

void mult_matrix(std::span<const double, 16> matrix)
{
    glMultMatrixd(matrix.data());
}

namespace
{

using glu_look_at_fn = void(APIENTRY*)(GLdouble,
                                       GLdouble,
                                       GLdouble,
                                       GLdouble,
                                       GLdouble,
                                       GLdouble,
                                       GLdouble,
                                       GLdouble,
                                       GLdouble);

} // namespace

void apply_lookat(double ex,
                  double ey,
                  double ez,
                  double cx,
                  double cy,
                  double cz,
                  double ux,
                  double uy,
                  double uz) noexcept
{
    const auto orig = sdk::gl::detail::call_orig<glu_look_at_fn>(
        sdk::gl::detail::g_hooks.glu_look_at);
    if (orig != nullptr)
    {
        orig(ex, ey, ez, cx, cy, cz, ux, uy, uz);
    }
}

} // namespace sdk::graphics
