// src/proxy/sdk/scripting/bindings/sdk.cpp
// SDK functions exposed to scripting.
// Implementation uses Windows/GL types internally, but public API is clean.

#include "sdk.hpp"

#include "sdk/core/context.hpp"
#include "sdk/core/logging.hpp"
#include "sdk/util/win32.hpp"

#include <GL/gl.h>
#include <GL/glu.h>
#include <windows.h>

#include <format>
#include <string_view>

namespace sdk::scripting::bindings::sdk
{

// ── OpenGL wrappers ─────────────────────────────────────────────────────────

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

void gl_mult_matrix_d(const std::array<double, 16>& m) noexcept
{
    glMultMatrixd(m.data());
}

void gl_apply_lookat(double ex, double ey, double ez,
                     double cx, double cy, double cz,
                     double ux, double uy, double uz) noexcept
{
    gluLookAt(ex, ey, ez, cx, cy, cz, ux, uy, uz);
}

// ── Input ───────────────────────────────────────────────────────────────────

bool is_key_down(int vk) noexcept
{
    return win32::is_key_down(vk);
}

point2d get_cursor_pos() noexcept
{
    auto [x, y] = win32::cursor_pos();
    return { static_cast<int>(x), static_cast<int>(y) };
}

void set_cursor_pos(int x, int y) noexcept
{
    SetCursorPos(x, y);
}

void show_cursor(bool v) noexcept
{
    ShowCursor(v ? TRUE : FALSE);
}

void send_chars(const std::string& chars)
{
    for (char c : chars)
    {
        INPUT input{};
        input.type = INPUT_KEYBOARD;
        input.ki.wVk = 0;
        input.ki.wScan = static_cast<WORD>(c);
        input.ki.dwFlags = KEYEVENTF_UNICODE;
        SendInput(1, &input, sizeof(INPUT));
        input.ki.dwFlags |= KEYEVENTF_KEYUP;
        SendInput(1, &input, sizeof(INPUT));
    }
}

// ── Window ──────────────────────────────────────────────────────────────────

rect get_window_rect() noexcept
{
    auto [left, top, right, bottom] = win32::window_rect(g_ctx.window);
    return { left, top, right, bottom };
}

// ── Logging ─────────────────────────────────────────────────────────────────

void log_info(const std::string& m)
{
    ::sdk::log_info(std::format("[script] {}", m));
}

void log_warn(const std::string& m)
{
    ::sdk::log_warn(std::format("[script] {}", m));
}

void log_error(const std::string& m)
{
    ::sdk::log_error(std::format("[script] {}", m));
}

std::string get_log_dir() noexcept
{
    return "logs";
}

} // namespace sdk::scripting::bindings::sdk
