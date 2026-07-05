/// @file bindings/sdk.hpp
/// @brief Pure C++ SDK functions for scripting (no backend types).
///
/// All functions use simple types (double, int, std::array) — no glm,
/// no Windows-specific types in the public API.

#pragma once

#include <array>
#include <string>

namespace sdk::scripting::bindings::sdk
{

/// 2D point (screen coordinates).
struct point2d
{
    int x, y;
};

/// Rectangle (window bounds).
struct rect
{
    int left, top, right, bottom;
};

// ── OpenGL wrappers ─────────────────────────────────────────────────────────

void gl_enable(int cap) noexcept;
void gl_disable(int cap) noexcept;
void gl_depth_mask(bool flag) noexcept;
void gl_blend_func(int sfactor, int dfactor) noexcept;
void gl_line_width(float w) noexcept;
void gl_point_size(float sz) noexcept;
void gl_color4f(float r, float g, float b, float a) noexcept;
void gl_color3f(float r, float g, float b) noexcept;
void gl_polygon_mode(int face, int mode) noexcept;
void gl_push_attrib(int mask) noexcept;
void gl_pop_attrib() noexcept;
void gl_push_matrix() noexcept;
void gl_pop_matrix() noexcept;
void gl_begin(int mode) noexcept;
void gl_end() noexcept;
void gl_vertex3f(float x, float y, float z) noexcept;
void gl_vertex2f(float x, float y) noexcept;
void gl_translate(double x, double y, double z) noexcept;
void gl_rotate(double angle, double x, double y, double z) noexcept;
void gl_scale(double x, double y, double z) noexcept;

/// Apply 4x4 matrix (column-major, 16 elements).
void gl_mult_matrix_d(const std::array<double, 16>& m) noexcept;

/// Apply look-at transformation.
void gl_apply_lookat(double ex, double ey, double ez,
                     double cx, double cy, double cz,
                     double ux, double uy, double uz) noexcept;

// ── Input ───────────────────────────────────────────────────────────────────

/// Check if a key is currently pressed.
[[nodiscard]] bool is_key_down(int vk) noexcept;

/// Get current cursor position (screen coordinates).
[[nodiscard]] point2d get_cursor_pos() noexcept;

/// Set cursor position (screen coordinates).
void set_cursor_pos(int x, int y) noexcept;

/// Show or hide the cursor.
void show_cursor(bool v) noexcept;

/// Send character input (simulates keyboard input).
void send_chars(const std::string& chars);

// ── Window ──────────────────────────────────────────────────────────────────

/// Get the main window bounds.
[[nodiscard]] rect get_window_rect() noexcept;

// ── Logging ─────────────────────────────────────────────────────────────────

void log_info(const std::string& m);
void log_warn(const std::string& m);
void log_error(const std::string& m);

/// Get the log directory path.
[[nodiscard]] std::string get_log_dir() noexcept;

} // namespace sdk::scripting::bindings::sdk
