// src/proxy/sdk/scripting/bindings/constants.cpp
// Game and OpenGL constants exposed to scripting.
// Pure C++ — no backend types.

#include "constants.hpp"

#include <windows.h>
#include <GL/gl.h>
#include <sol/sol.hpp>

namespace sdk::scripting::bindings::constants
{

// ── Virtual keys ─────────────────────────────────────────────────────────────

// Modifiers
int vk_shift()   noexcept { return VK_SHIFT; }
int vk_control() noexcept { return VK_CONTROL; }
int vk_space()   noexcept { return VK_SPACE; }

// Navigation
int vk_insert()  noexcept { return VK_INSERT; }
int vk_escape()  noexcept { return VK_ESCAPE; }
int vk_tab()     noexcept { return VK_TAB; }
int vk_return()  noexcept { return VK_RETURN; }
int vk_back()    noexcept { return VK_BACK; }
int vk_delete()  noexcept { return VK_DELETE; }
int vk_home()    noexcept { return VK_HOME; }
int vk_end()     noexcept { return VK_END; }
int vk_prior()   noexcept { return VK_PRIOR; }
int vk_next()    noexcept { return VK_NEXT; }

// Arrow keys
int vk_left()    noexcept { return VK_LEFT; }
int vk_right()   noexcept { return VK_RIGHT; }
int vk_up()      noexcept { return VK_UP; }
int vk_down()    noexcept { return VK_DOWN; }

// F-keys
int vk_f1()      noexcept { return VK_F1; }
int vk_f2()      noexcept { return VK_F2; }
int vk_f3()      noexcept { return VK_F3; }
int vk_f4()      noexcept { return VK_F4; }
int vk_f5()      noexcept { return VK_F5; }
int vk_f6()      noexcept { return VK_F6; }
int vk_f7()      noexcept { return VK_F7; }
int vk_f8()      noexcept { return VK_F8; }
int vk_f9()      noexcept { return VK_F9; }
int vk_f10()     noexcept { return VK_F10; }
int vk_f11()     noexcept { return VK_F11; }
int vk_f12()     noexcept { return VK_F12; }

// Mouse buttons
int vk_lbutton() noexcept { return VK_LBUTTON; }
int vk_rbutton() noexcept { return VK_RBUTTON; }
int vk_mbutton() noexcept { return VK_MBUTTON; }

// Letter keys
int vk_w()       noexcept { return 'W'; }
int vk_a()       noexcept { return 'A'; }
int vk_s()       noexcept { return 'S'; }
int vk_d()       noexcept { return 'D'; }
int vk_q()       noexcept { return 'Q'; }
int vk_e()       noexcept { return 'E'; }
int vk_c()       noexcept { return 'C'; }
int vk_r()       noexcept { return 'R'; }
int vk_z()       noexcept { return 'Z'; }
int vk_x()       noexcept { return 'X'; }
int vk_v()       noexcept { return 'V'; }

// ── OpenGL constants ─────────────────────────────────────────────────────────

// Matrix mode
int gl_modelview()  noexcept { return GL_MODELVIEW; }
int gl_projection() noexcept { return GL_PROJECTION; }
int gl_texture()    noexcept { return GL_TEXTURE; }

// State caps
int gl_depth_test()  noexcept { return GL_DEPTH_TEST; }
int gl_blend()       noexcept { return GL_BLEND; }
int gl_alpha_test()  noexcept { return GL_ALPHA_TEST; }
int gl_cull_face()   noexcept { return GL_CULL_FACE; }
int gl_lighting()    noexcept { return GL_LIGHTING; }
int gl_fog()         noexcept { return GL_FOG; }
int gl_texture_2d()  noexcept { return GL_TEXTURE_2D; }

// Face selection
int gl_front()          noexcept { return GL_FRONT; }
int gl_back()           noexcept { return GL_BACK; }
int gl_front_and_back() noexcept { return GL_FRONT_AND_BACK; }

// Blend factors
int gl_src_alpha()             noexcept { return GL_SRC_ALPHA; }
int gl_one_minus_src_alpha()   noexcept { return GL_ONE_MINUS_SRC_ALPHA; }
int gl_one()                   noexcept { return GL_ONE; }
int gl_zero()                  noexcept { return GL_ZERO; }

// Primitive types (for glBegin)
int gl_lines()           noexcept { return GL_LINES; }
int gl_line_strip()      noexcept { return GL_LINE_STRIP; }
int gl_line_loop()       noexcept { return GL_LINE_LOOP; }
int gl_triangles()       noexcept { return GL_TRIANGLES; }
int gl_triangle_strip()  noexcept { return GL_TRIANGLE_STRIP; }
int gl_triangle_fan()    noexcept { return GL_TRIANGLE_FAN; }
int gl_quads()           noexcept { return GL_QUADS; }
int gl_points()          noexcept { return GL_POINTS; }
int gl_polygon()         noexcept { return GL_POLYGON; }

// Polygon mode (for glPolygonMode)
int gl_line()            noexcept { return GL_LINE; }
int gl_fill()            noexcept { return GL_FILL; }

// State masks
int gl_all_attrib_bits() noexcept { return GL_ALL_ATTRIB_BITS; }

// ── Registration function for Lua bindings ──────────────────────────────────

void register_constants(sol::state& lua)
{
    auto constants = lua["constants"].get_or_create<sol::table>();
    
    // Virtual keys
    constants["vk_shift"] = &vk_shift;
    constants["vk_control"] = &vk_control;
    constants["vk_space"] = &vk_space;
    constants["vk_insert"] = &vk_insert;
    constants["vk_escape"] = &vk_escape;
    constants["vk_tab"] = &vk_tab;
    constants["vk_return"] = &vk_return;
    constants["vk_back"] = &vk_back;
    constants["vk_delete"] = &vk_delete;
    constants["vk_home"] = &vk_home;
    constants["vk_end"] = &vk_end;
    constants["vk_prior"] = &vk_prior;
    constants["vk_next"] = &vk_next;
    constants["vk_left"] = &vk_left;
    constants["vk_right"] = &vk_right;
    constants["vk_up"] = &vk_up;
    constants["vk_down"] = &vk_down;
    constants["vk_f1"] = &vk_f1;
    constants["vk_f2"] = &vk_f2;
    constants["vk_f3"] = &vk_f3;
    constants["vk_f4"] = &vk_f4;
    constants["vk_f5"] = &vk_f5;
    constants["vk_f6"] = &vk_f6;
    constants["vk_f7"] = &vk_f7;
    constants["vk_f8"] = &vk_f8;
    constants["vk_f9"] = &vk_f9;
    constants["vk_f10"] = &vk_f10;
    constants["vk_f11"] = &vk_f11;
    constants["vk_f12"] = &vk_f12;
    constants["vk_lbutton"] = &vk_lbutton;
    constants["vk_rbutton"] = &vk_rbutton;
    constants["vk_mbutton"] = &vk_mbutton;
    constants["vk_w"] = &vk_w;
    constants["vk_a"] = &vk_a;
    constants["vk_s"] = &vk_s;
    constants["vk_d"] = &vk_d;
    constants["vk_q"] = &vk_q;
    constants["vk_e"] = &vk_e;
    constants["vk_c"] = &vk_c;
    constants["vk_r"] = &vk_r;
    constants["vk_z"] = &vk_z;
    constants["vk_x"] = &vk_x;
    constants["vk_v"] = &vk_v;
    
    // OpenGL constants
    constants["gl_modelview"] = &gl_modelview;
    constants["gl_projection"] = &gl_projection;
    constants["gl_texture"] = &gl_texture;
    constants["gl_depth_test"] = &gl_depth_test;
    constants["gl_blend"] = &gl_blend;
    constants["gl_alpha_test"] = &gl_alpha_test;
    constants["gl_cull_face"] = &gl_cull_face;
    constants["gl_lighting"] = &gl_lighting;
    constants["gl_fog"] = &gl_fog;
    constants["gl_texture_2d"] = &gl_texture_2d;
    constants["gl_front"] = &gl_front;
    constants["gl_back"] = &gl_back;
    constants["gl_front_and_back"] = &gl_front_and_back;
    constants["gl_src_alpha"] = &gl_src_alpha;
    constants["gl_one_minus_src_alpha"] = &gl_one_minus_src_alpha;
    constants["gl_one"] = &gl_one;
    constants["gl_zero"] = &gl_zero;
    constants["gl_lines"] = &gl_lines;
    constants["gl_line_strip"] = &gl_line_strip;
    constants["gl_line_loop"] = &gl_line_loop;
    constants["gl_triangles"] = &gl_triangles;
    constants["gl_triangle_strip"] = &gl_triangle_strip;
    constants["gl_triangle_fan"] = &gl_triangle_fan;
    constants["gl_quads"] = &gl_quads;
    constants["gl_points"] = &gl_points;
    constants["gl_polygon"] = &gl_polygon;
    constants["gl_line"] = &gl_line;
    constants["gl_fill"] = &gl_fill;
    constants["gl_all_attrib_bits"] = &gl_all_attrib_bits;
}

} // namespace sdk::scripting::bindings::constants
