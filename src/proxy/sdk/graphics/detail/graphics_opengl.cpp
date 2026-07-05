#include "sdk/graphics/graphics.hpp"

#include "sdk/core/context.hpp"
#include "sdk/core/detail/context_state.hpp"

#include <GL/gl.h>
#include <windows.h>

namespace sdk::graphics
{

void enable(std::int32_t cap)
{
    glEnable(static_cast<GLenum>(cap));
}

void disable(std::int32_t cap)
{
    glDisable(static_cast<GLenum>(cap));
}

void depth_mask(bool flag)
{
    glDepthMask(flag ? GL_TRUE : GL_FALSE);
}

void blend_func(std::int32_t sfactor, std::int32_t dfactor)
{
    glBlendFunc(static_cast<GLenum>(sfactor), static_cast<GLenum>(dfactor));
}

void line_width(float w)
{
    glLineWidth(w);
}

void point_size(float sz)
{
    glPointSize(sz);
}

void color4f(float r, float g, float b, float a)
{
    glColor4f(r, g, b, a);
}

void color3f(float r, float g, float b)
{
    glColor3f(r, g, b);
}

void polygon_mode(std::int32_t face, std::int32_t mode)
{
    glPolygonMode(static_cast<GLenum>(face), static_cast<GLenum>(mode));
}

void push_attrib(std::int32_t mask)
{
    glPushAttrib(static_cast<GLbitfield>(mask));
}

void pop_attrib()
{
    glPopAttrib();
}

void push_matrix()
{
    glPushMatrix();
}

void pop_matrix()
{
    glPopMatrix();
}

void begin(std::int32_t mode)
{
    glBegin(static_cast<GLenum>(mode));
}

void end()
{
    glEnd();
}

void vertex3f(float x, float y, float z)
{
    glVertex3f(x, y, z);
}

void vertex2f(float x, float y)
{
    glVertex2f(x, y);
}

void translate(double x, double y, double z)
{
    glTranslated(x, y, z);
}

void rotate(double angle, double x, double y, double z)
{
    glRotated(angle, x, y, z);
}

void scale(double x, double y, double z)
{
    glScaled(x, y, z);
}

void mult_matrix(const double* m16)
{
    if (m16 != nullptr)
    {
        glMultMatrixd(m16);
    }
}

namespace
{

using glu_look_at_fn = void(APIENTRY*)(GLdouble, GLdouble, GLdouble,
                                        GLdouble, GLdouble, GLdouble,
                                        GLdouble, GLdouble, GLdouble);

} // namespace

void apply_lookat(double ex, double ey, double ez,
                  double cx, double cy, double cz,
                  double ux, double uy, double uz)
{
    const auto orig = detail::call_orig<glu_look_at_fn>(detail::g_state.hooks.glu_look_at);
    if (orig != nullptr)
    {
        orig(ex, ey, ez, cx, cy, cz, ux, uy, uz);
    }
}

namespace constants
{

std::int32_t vk_shift()   { return VK_SHIFT; }
std::int32_t vk_control() { return VK_CONTROL; }
std::int32_t vk_space()   { return VK_SPACE; }
std::int32_t vk_insert()  { return VK_INSERT; }
std::int32_t vk_escape()  { return VK_ESCAPE; }
std::int32_t vk_tab()     { return VK_TAB; }
std::int32_t vk_return()  { return VK_RETURN; }
std::int32_t vk_back()    { return VK_BACK; }
std::int32_t vk_delete()  { return VK_DELETE; }
std::int32_t vk_home()    { return VK_HOME; }
std::int32_t vk_end()     { return VK_END; }
std::int32_t vk_prior()   { return VK_PRIOR; }
std::int32_t vk_next()    { return VK_NEXT; }
std::int32_t vk_left()    { return VK_LEFT; }
std::int32_t vk_right()   { return VK_RIGHT; }
std::int32_t vk_up()      { return VK_UP; }
std::int32_t vk_down()    { return VK_DOWN; }
std::int32_t vk_f1()      { return VK_F1; }
std::int32_t vk_f2()      { return VK_F2; }
std::int32_t vk_f3()      { return VK_F3; }
std::int32_t vk_f4()      { return VK_F4; }
std::int32_t vk_f5()      { return VK_F5; }
std::int32_t vk_f6()      { return VK_F6; }
std::int32_t vk_f7()      { return VK_F7; }
std::int32_t vk_f8()      { return VK_F8; }
std::int32_t vk_f9()      { return VK_F9; }
std::int32_t vk_f10()     { return VK_F10; }
std::int32_t vk_f11()     { return VK_F11; }
std::int32_t vk_f12()     { return VK_F12; }
std::int32_t vk_lbutton() { return VK_LBUTTON; }
std::int32_t vk_rbutton() { return VK_RBUTTON; }
std::int32_t vk_mbutton() { return VK_MBUTTON; }
std::int32_t vk_w()       { return 'W'; }
std::int32_t vk_a()       { return 'A'; }
std::int32_t vk_s()       { return 'S'; }
std::int32_t vk_d()       { return 'D'; }
std::int32_t vk_q()       { return 'Q'; }
std::int32_t vk_e()       { return 'E'; }
std::int32_t vk_c()       { return 'C'; }
std::int32_t vk_r()       { return 'R'; }
std::int32_t vk_z()       { return 'Z'; }
std::int32_t vk_x()       { return 'X'; }
std::int32_t vk_v()       { return 'V'; }

std::int32_t gl_modelview()  { return GL_MODELVIEW; }
std::int32_t gl_projection() { return GL_PROJECTION; }
std::int32_t gl_texture()    { return GL_TEXTURE; }
std::int32_t gl_depth_test() { return GL_DEPTH_TEST; }
std::int32_t gl_blend()      { return GL_BLEND; }
std::int32_t gl_alpha_test() { return GL_ALPHA_TEST; }
std::int32_t gl_cull_face()  { return GL_CULL_FACE; }
std::int32_t gl_lighting()   { return GL_LIGHTING; }
std::int32_t gl_fog()        { return GL_FOG; }
std::int32_t gl_texture_2d() { return GL_TEXTURE_2D; }
std::int32_t gl_front()      { return GL_FRONT; }
std::int32_t gl_back()       { return GL_BACK; }
std::int32_t gl_front_and_back() { return GL_FRONT_AND_BACK; }
std::int32_t gl_src_alpha() { return GL_SRC_ALPHA; }
std::int32_t gl_one_minus_src_alpha() { return GL_ONE_MINUS_SRC_ALPHA; }
std::int32_t gl_one()  { return GL_ONE; }
std::int32_t gl_zero() { return GL_ZERO; }
std::int32_t gl_lines()          { return GL_LINES; }
std::int32_t gl_line_strip()     { return GL_LINE_STRIP; }
std::int32_t gl_line_loop()      { return GL_LINE_LOOP; }
std::int32_t gl_triangles()      { return GL_TRIANGLES; }
std::int32_t gl_triangle_strip() { return GL_TRIANGLE_STRIP; }
std::int32_t gl_triangle_fan()   { return GL_TRIANGLE_FAN; }
std::int32_t gl_quads()          { return GL_QUADS; }
std::int32_t gl_points()         { return GL_POINTS; }
std::int32_t gl_polygon()        { return GL_POLYGON; }
std::int32_t gl_line()           { return GL_LINE; }
std::int32_t gl_fill()           { return GL_FILL; }
std::int32_t gl_all_attrib_bits() { return GL_ALL_ATTRIB_BITS; }

} // namespace constants

} // namespace sdk::graphics
