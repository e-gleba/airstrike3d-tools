// src/proxy/sdk/lua/bindings/constants.cpp
// Game and OpenGL constants exposed to Lua.
// Pure C++ — no sol2 types.

#include <windows.h>
#include <GL/gl.h>

namespace sdk::lua::bindings::constants
{

// ── Virtual keys ─────────────────────────────────────────────────────────────

// Modifiers
std::int32_t vk_shift()   noexcept { return VK_SHIFT; }
std::int32_t vk_control() noexcept { return VK_CONTROL; }
std::int32_t vk_space()   noexcept { return VK_SPACE; }

// Navigation
std::int32_t vk_insert()  noexcept { return VK_INSERT; }
std::int32_t vk_escape()  noexcept { return VK_ESCAPE; }
std::int32_t vk_tab()     noexcept { return VK_TAB; }
std::int32_t vk_return()  noexcept { return VK_RETURN; }
std::int32_t vk_back()    noexcept { return VK_BACK; }
std::int32_t vk_delete()  noexcept { return VK_DELETE; }
std::int32_t vk_home()    noexcept { return VK_HOME; }
std::int32_t vk_end()     noexcept { return VK_END; }
std::int32_t vk_prior()   noexcept { return VK_PRIOR; }
std::int32_t vk_next()    noexcept { return VK_NEXT; }

// Arrow keys
std::int32_t vk_left()    noexcept { return VK_LEFT; }
std::int32_t vk_right()   noexcept { return VK_RIGHT; }
std::int32_t vk_up()      noexcept { return VK_UP; }
std::int32_t vk_down()    noexcept { return VK_DOWN; }

// F-keys
std::int32_t vk_f1()      noexcept { return VK_F1; }
std::int32_t vk_f2()      noexcept { return VK_F2; }
std::int32_t vk_f3()      noexcept { return VK_F3; }
std::int32_t vk_f4()      noexcept { return VK_F4; }
std::int32_t vk_f5()      noexcept { return VK_F5; }
std::int32_t vk_f6()      noexcept { return VK_F6; }
std::int32_t vk_f7()      noexcept { return VK_F7; }
std::int32_t vk_f8()      noexcept { return VK_F8; }
std::int32_t vk_f9()      noexcept { return VK_F9; }
std::int32_t vk_f10()     noexcept { return VK_F10; }
std::int32_t vk_f11()     noexcept { return VK_F11; }
std::int32_t vk_f12()     noexcept { return VK_F12; }

// Mouse buttons
std::int32_t vk_lbutton() noexcept { return VK_LBUTTON; }
std::int32_t vk_rbutton() noexcept { return VK_RBUTTON; }
std::int32_t vk_mbutton() noexcept { return VK_MBUTTON; }

// Letter keys
std::int32_t vk_w()       noexcept { return 'W'; }
std::int32_t vk_a()       noexcept { return 'A'; }
std::int32_t vk_s()       noexcept { return 'S'; }
std::int32_t vk_d()       noexcept { return 'D'; }
std::int32_t vk_q()       noexcept { return 'Q'; }
std::int32_t vk_e()       noexcept { return 'E'; }
std::int32_t vk_c()       noexcept { return 'C'; }
std::int32_t vk_r()       noexcept { return 'R'; }
std::int32_t vk_z()       noexcept { return 'Z'; }
std::int32_t vk_x()       noexcept { return 'X'; }
std::int32_t vk_v()       noexcept { return 'V'; }

// ── OpenGL constants ─────────────────────────────────────────────────────────

// Matrix mode
std::int32_t gl_modelview()  noexcept { return GL_MODELVIEW; }
std::int32_t gl_projection() noexcept { return GL_PROJECTION; }
std::int32_t gl_texture()    noexcept { return GL_TEXTURE; }

// State caps
std::int32_t gl_depth_test()  noexcept { return GL_DEPTH_TEST; }
std::int32_t gl_blend()       noexcept { return GL_BLEND; }
std::int32_t gl_alpha_test()  noexcept { return GL_ALPHA_TEST; }
std::int32_t gl_cull_face()   noexcept { return GL_CULL_FACE; }
std::int32_t gl_lighting()    noexcept { return GL_LIGHTING; }
std::int32_t gl_fog()         noexcept { return GL_FOG; }
std::int32_t gl_texture_2d()  noexcept { return GL_TEXTURE_2D; }

// Face selection
std::int32_t gl_front()          noexcept { return GL_FRONT; }
std::int32_t gl_back()           noexcept { return GL_BACK; }
std::int32_t gl_front_and_back() noexcept { return GL_FRONT_AND_BACK; }

// Blend factors
std::int32_t gl_src_alpha()             noexcept { return GL_SRC_ALPHA; }
std::int32_t gl_one_minus_src_alpha()   noexcept { return GL_ONE_MINUS_SRC_ALPHA; }
std::int32_t gl_one()                   noexcept { return GL_ONE; }
std::int32_t gl_zero()                  noexcept { return GL_ZERO; }

// Primitive types (for glBegin)
std::int32_t gl_lines()           noexcept { return GL_LINES; }
std::int32_t gl_line_strip()      noexcept { return GL_LINE_STRIP; }
std::int32_t gl_line_loop()       noexcept { return GL_LINE_LOOP; }
std::int32_t gl_triangles()       noexcept { return GL_TRIANGLES; }
std::int32_t gl_triangle_strip()  noexcept { return GL_TRIANGLE_STRIP; }
std::int32_t gl_triangle_fan()    noexcept { return GL_TRIANGLE_FAN; }
std::int32_t gl_quads()           noexcept { return GL_QUADS; }
std::int32_t gl_points()          noexcept { return GL_POINTS; }
std::int32_t gl_polygon()         noexcept { return GL_POLYGON; }

// Polygon mode (for glPolygonMode)
std::int32_t gl_line()            noexcept { return GL_LINE; }
std::int32_t gl_fill()            noexcept { return GL_FILL; }

// State masks
std::int32_t gl_all_attrib_bits() noexcept { return GL_ALL_ATTRIB_BITS; }

} // namespace sdk::lua::bindings::constants
