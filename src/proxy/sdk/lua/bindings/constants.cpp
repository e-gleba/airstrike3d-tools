// src/proxy/sdk/lua/bindings/constants.cpp
// Game and OpenGL constants exposed to Lua.
// Pure C++ — no sol2 types.

#include <windows.h>
#include <GL/gl.h>

namespace sdk::lua::bindings
{

// Virtual key codes
int vk_shift() noexcept { return VK_SHIFT; }
int vk_control() noexcept { return VK_CONTROL; }
int vk_space() noexcept { return VK_SPACE; }
int vk_insert() noexcept { return VK_INSERT; }
int vk_escape() noexcept { return VK_ESCAPE; }
int vk_tab() noexcept { return VK_TAB; }
int vk_return() noexcept { return VK_RETURN; }
int vk_back() noexcept { return VK_BACK; }
int vk_delete() noexcept { return VK_DELETE; }
int vk_home() noexcept { return VK_HOME; }
int vk_end() noexcept { return VK_END; }
int vk_prior() noexcept { return VK_PRIOR; }
int vk_next() noexcept { return VK_NEXT; }
int vk_left() noexcept { return VK_LEFT; }
int vk_right() noexcept { return VK_RIGHT; }
int vk_up() noexcept { return VK_UP; }
int vk_down() noexcept { return VK_DOWN; }

// OpenGL constants
int gl_modelview() noexcept { return GL_MODELVIEW; }
int gl_projection() noexcept { return GL_PROJECTION; }
int gl_texture() noexcept { return GL_TEXTURE; }
int gl_depth_test() noexcept { return GL_DEPTH_TEST; }
int gl_blend() noexcept { return GL_BLEND; }
int gl_alpha_test() noexcept { return GL_ALPHA_TEST; }
int gl_cull_face() noexcept { return GL_CULL_FACE; }
int gl_lighting() noexcept { return GL_LIGHTING; }
int gl_fog() noexcept { return GL_FOG; }
int gl_front() noexcept { return GL_FRONT; }
int gl_back() noexcept { return GL_BACK; }
int gl_front_and_back() noexcept { return GL_FRONT_AND_BACK; }

// Blend factors
int gl_src_alpha() noexcept { return GL_SRC_ALPHA; }
int gl_one_minus_src_alpha() noexcept { return GL_ONE_MINUS_SRC_ALPHA; }
int gl_one() noexcept { return GL_ONE; }
int gl_zero() noexcept { return GL_ZERO; }

// Primitive types
int gl_lines() noexcept { return GL_LINES; }
int gl_line_strip() noexcept { return GL_LINE_STRIP; }
int gl_line_loop() noexcept { return GL_LINE_LOOP; }
int gl_triangles() noexcept { return GL_TRIANGLES; }
int gl_triangle_strip() noexcept { return GL_TRIANGLE_STRIP; }
int gl_triangle_fan() noexcept { return GL_TRIANGLE_FAN; }
int gl_quads() noexcept { return GL_QUADS; }
int gl_points() noexcept { return GL_POINTS; }
int gl_polygon() noexcept { return GL_POLYGON; }

} // namespace sdk::lua::bindings
