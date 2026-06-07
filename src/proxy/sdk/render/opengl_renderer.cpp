// sdk/render/opengl_renderer.cpp — OpenGL renderer implementation
//
// ALL OpenGL code isolated here. Public headers remain pure C++23.

#include "sdk/render/opengl_renderer.hpp"
#include "sdk/math/glm_adapter.hpp"

#include <GL/gl.h>
#include <GL/glu.h>

#include <spdlog/spdlog.h>

namespace sdk::render
{

// ─── opengl_renderer::impl ───────────────────────────────────────────────

struct opengl_renderer::impl
{
    bool initialized = false;

    void check_error(char const* operation)
    {
        if (GLenum err = glGetError(); err != GL_NO_ERROR)
        {
            spdlog::error("[opengl] {} failed with error: 0x{:04X}", operation, err);
        }
    }
};

// ─── opengl_renderer implementation ──────────────────────────────────────

opengl_renderer::opengl_renderer() : pimpl_(std::make_unique<impl>()) {}

opengl_renderer::~opengl_renderer()
{
    if (pimpl_ && pimpl_->initialized)
    {
        shutdown();
    }
}

void opengl_renderer::init(device_context* /*ctx*/)
{
    if (pimpl_->initialized)
    {
        return;
    }
    pimpl_->initialized = true;
    spdlog::info("[opengl] renderer initialized");
}

void opengl_renderer::shutdown()
{
    if (!pimpl_->initialized)
    {
        return;
    }
    pimpl_->initialized = false;
    spdlog::info("[opengl] renderer shutdown");
}

auto opengl_renderer::is_initialized() const noexcept -> bool
{
    return pimpl_->initialized;
}

void opengl_renderer::begin_frame() { /* No-op for OpenGL */ }
void opengl_renderer::end_frame() { /* No-op for OpenGL */ }

void opengl_renderer::enable(int state)
{
    glEnable(static_cast<GLenum>(state));
}

void opengl_renderer::disable(int state)
{
    glDisable(static_cast<GLenum>(state));
}

void opengl_renderer::set_blend_func(int src_factor, int dst_factor)
{
    glBlendFunc(static_cast<GLenum>(src_factor), static_cast<GLenum>(dst_factor));
}

void opengl_renderer::set_depth_mask(bool enabled)
{
    glDepthMask(enabled ? GL_TRUE : GL_FALSE);
}

void opengl_renderer::set_matrix_mode(int mode)
{
    glMatrixMode(static_cast<GLenum>(mode));
}

void opengl_renderer::load_identity()
{
    glLoadIdentity();
}

void opengl_renderer::load_matrix(double const* matrix)
{
    glLoadMatrixd(matrix);
}

void opengl_renderer::mult_matrix(double const* matrix)
{
    glMultMatrixd(matrix);
}

void opengl_renderer::push_matrix()
{
    glPushMatrix();
}

void opengl_renderer::pop_matrix()
{
    glPopMatrix();
}

void opengl_renderer::look_at(math::vec3 const& eye, math::vec3 const& center, math::vec3 const& up)
{
    gluLookAt(eye.x, eye.y, eye.z, center.x, center.y, center.z, up.x, up.y, up.z);
}

void opengl_renderer::begin(int primitive_type)
{
    glBegin(static_cast<GLenum>(primitive_type));
}

void opengl_renderer::end()
{
    glEnd();
}

void opengl_renderer::set_color(float r, float g, float b, float a)
{
    glColor4f(r, g, b, a);
}

void opengl_renderer::vertex(float x, float y, float z)
{
    glVertex3f(x, y, z);
}

void opengl_renderer::vertex(math::vec3 const& v)
{
    glVertex3d(v.x, v.y, v.z);
}

void opengl_renderer::set_line_width(float width)
{
    glLineWidth(width);
}

void opengl_renderer::set_point_size(float size)
{
    glPointSize(size);
}

void opengl_renderer::translate(double x, double y, double z)
{
    glTranslated(x, y, z);
}

void opengl_renderer::rotate(double angle, double x, double y, double z)
{
    glRotated(angle, x, y, z);
}

void opengl_renderer::scale(double x, double y, double z)
{
    glScaled(x, y, z);
}

void opengl_renderer::push_attrib(unsigned int mask)
{
    glPushAttrib(static_cast<GLbitfield>(mask));
}

void opengl_renderer::pop_attrib()
{
    glPopAttrib();
}

void opengl_renderer::set_polygon_mode(int face, int mode)
{
    glPolygonMode(static_cast<GLenum>(face), static_cast<GLenum>(mode));
}

// ─── Factory ──────────────────────────────────────────────────────────────

auto create_opengl_renderer() -> std::unique_ptr<renderer>
{
    return std::make_unique<opengl_renderer>();
}

} // namespace sdk::render
