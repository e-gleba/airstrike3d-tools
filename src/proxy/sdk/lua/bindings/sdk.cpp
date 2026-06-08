// src/proxy/sdk/lua/bindings/sdk.cpp
// SDK functions exposed to Lua.
// Pure C++ — no sol2 types.

#include "sdk/core/context.hpp"
#include "sdk/util/win32.hpp"

#include <GL/gl.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <spdlog/spdlog.h>

namespace sdk::lua::bindings
{

// OpenGL wrappers
void gl_enable(int cap) noexcept
{
    glEnable(static_cast<GLenum>(cap));
}

void gl_disable(int cap) noexcept
{
    glDisable(static_cast<GLenum>(cap));
}

void gl_depth_mask(bool flag) noexcept
{
    glDepthMask(flag ? GL_TRUE : GL_FALSE);
}

void gl_blend_func(int sfactor, int dfactor) noexcept
{
    glBlendFunc(static_cast<GLenum>(sfactor), static_cast<GLenum>(dfactor));
}

void gl_line_width(float w) noexcept
{
    glLineWidth(w);
}

void gl_point_size(float sz) noexcept
{
    glPointSize(sz);
}

void gl_color4f(float r, float g, float b, float a) noexcept
{
    glColor4f(r, g, b, a);
}

void gl_color3f(float r, float g, float b) noexcept
{
    glColor3f(r, g, b);
}

void gl_polygon_mode(int face, int mode) noexcept
{
    glPolygonMode(static_cast<GLenum>(face), static_cast<GLenum>(mode));
}

void gl_push_attrib(int mask) noexcept
{
    glPushAttrib(static_cast<GLbitfield>(mask));
}

void gl_pop_attrib() noexcept
{
    glPopAttrib();
}

void gl_push_matrix() noexcept
{
    glPushMatrix();
}

void gl_pop_matrix() noexcept
{
    glPopMatrix();
}

void gl_begin(int mode) noexcept
{
    glBegin(static_cast<GLenum>(mode));
}

void gl_end() noexcept
{
    glEnd();
}

void gl_vertex3f(float x, float y, float z) noexcept
{
    glVertex3f(x, y, z);
}

void gl_vertex2f(float x, float y) noexcept
{
    glVertex2f(x, y);
}

void gl_translate(double x, double y, double z) noexcept
{
    glTranslated(x, y, z);
}

void gl_rotate(double angle, double x, double y, double z) noexcept
{
    glRotated(angle, x, y, z);
}

void gl_scale(double x, double y, double z) noexcept
{
    glScaled(x, y, z);
}

// Input
bool is_key_down(int vk) noexcept
{
    return win32::is_key_down(vk);
}

glm::dvec2 get_cursor_pos() noexcept
{
    auto const [x, y] = win32::cursor_pos();
    return { static_cast<double>(x), static_cast<double>(y) };
}

void set_cursor_pos(int x, int y) noexcept
{
    SetCursorPos(x, y);
}

void show_cursor(bool v) noexcept
{
    ShowCursor(v ? TRUE : FALSE);
}

// Window
RECT get_window_rect() noexcept
{
    auto const [left, top, right, bottom] = win32::window_rect(g_ctx.window);
    return RECT{ left, top, right, bottom };
}

// Logging
void log_info(const std::string& m)
{
    spdlog::info("[lua] {}", m);
}

void log_warn(const std::string& m)
{
    spdlog::warn("[lua] {}", m);
}

void log_error(const std::string& m)
{
    spdlog::error("[lua] {}", m);
}

std::string get_log_dir() noexcept
{
    return "logs";
}

} // namespace sdk::lua::bindings
