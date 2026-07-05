/// @file bindings/sdk.hpp
/// @brief Pure C++ SDK functions for Lua binding (no sol2 types).

#pragma once

#include <glm/glm.hpp>
#include <windows.h>
#include <string>

namespace sdk::lua::bindings::sdk
{

// OpenGL wrappers
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

// Input
[[nodiscard]] bool is_key_down(int vk) noexcept;
[[nodiscard]] glm::dvec2 get_cursor_pos() noexcept;
void set_cursor_pos(int x, int y) noexcept;
void show_cursor(bool v) noexcept;

// Window
[[nodiscard]] RECT get_window_rect() noexcept;

// Logging
void log_info(const std::string& m);
void log_warn(const std::string& m);
void log_error(const std::string& m);
[[nodiscard]] std::string get_log_dir() noexcept;

} // namespace sdk::lua::bindings::sdk
