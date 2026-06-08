/// @file detail/register_bindings.cpp
/// @brief Single adapter TU that bridges backend-agnostic binding
///        schemas to the sol2 state.
///
/// To replace sol2 with another Lua binding library, rewrite only
/// this file and `detail/lua_state.cpp`.

#include "sdk/lua/detail/register_bindings.hpp"
#include "sdk/lua/detail/binding_api.hpp"
#include "sdk/lua/detail/impl.hpp"
#include "sdk/lua/lua_state.hpp"

// Binding schemas (pure C++ — no sol2 in their headers).
#include "sdk/lua/bindings/constants.hpp"
#include "sdk/lua/bindings/math.hpp"
#include "sdk/lua/bindings/sdk.hpp"
#include "sdk/lua/bindings/ui.hpp"

#include <sol/sol.hpp>

#include <array>
#include <cmath>
#include <cstddef>
#include <format>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <imgui.h>
#include <misc/cpp/imgui_stdlib.h>
#include <ranges>
#include <span>
#include <string>
#include <tuple>
#include <vector>

// Platform headers — required for VK_ / GL_ constants and input helpers.
#include <GL/gl.h>
#include <windows.h>

namespace sdk::lua::detail
{

// ─── helpers ─────────────────────────────────────────────────────────────────

namespace
{

template <auto GlFn, typename... Args>
constexpr auto gl_wrap = [](Args... args) noexcept
{
    GlFn(static_cast<std::conditional_t<std::is_integral_v<Args>, GLenum, Args>>(
        args)...);
};

[[nodiscard]] constexpr auto to_tuple(const glm::dvec3& v) noexcept
    -> std::tuple<double, double, double>
{
    return {v.x, v.y, v.z};
}

[[nodiscard]] constexpr auto vec3(double x, double y, double z) noexcept
    -> glm::dvec3
{
    return {x, y, z};
}

[[nodiscard]] auto mat4_to_table(sol::state& s, const glm::dmat4& mat)
    -> sol::table
{
    auto t{s.create_table(16, 0)};
    auto p{std::span(glm::value_ptr(mat), 16)};
    for (auto i : std::views::iota(0uz, 16uz)) t[i + 1] = p[i];
    return t;
}

} // namespace

// ─── constants ───────────────────────────────────────────────────────────────

void register_constants(table_handle root)
{
    // Virtual keys.
    auto vk = root.create_table("VK");

    constexpr std::pair<const char*, int> k_vk_map[] = {
        {"SHIFT", VK_SHIFT},     {"CONTROL", VK_CONTROL},
        {"SPACE", VK_SPACE},     {"INSERT", VK_INSERT},
        {"LBUTTON", VK_LBUTTON}, {"RBUTTON", VK_RBUTTON},
        {"ESCAPE", VK_ESCAPE},   {"TAB", VK_TAB},
        {"RETURN", VK_RETURN},   {"BACK", VK_BACK},
        {"DELETE", VK_DELETE},   {"HOME", VK_HOME},
        {"END", VK_END},         {"PRIOR", VK_PRIOR},
        {"NEXT", VK_NEXT},       {"LEFT", VK_LEFT},
        {"RIGHT", VK_RIGHT},     {"UP", VK_UP},
        {"DOWN", VK_DOWN},       {"MENU", VK_MENU},
        {"CAPITAL", VK_CAPITAL}, {"MBUTTON", VK_MBUTTON},
        {"XBUTTON1", VK_XBUTTON1}, {"XBUTTON2", VK_XBUTTON2},
    };

    for (auto [name, code] : k_vk_map) vk.add_function(name, [code]{ return code; });

    // A-Z, 0-9, F1-F24.
    for (char c = 'A'; c <= 'Z'; ++c)
        vk.add_function(std::string(1, c), [c]{ return static_cast<int>(c); });
    for (char c = '0'; c <= '9'; ++c)
        vk.add_function(std::string(1, c), [c]{ return static_cast<int>(c); });
    for (int i = 1; i <= 24; ++i)
        vk.add_function(std::format("F{}", i), [i]{ return VK_F1 + (i - 1); });

    // GL constants.
    auto gl = root.create_table("GL");

    constexpr std::pair<const char*, int> k_gl_map[] = {
        {"MODELVIEW", GL_MODELVIEW}, {"PROJECTION", GL_PROJECTION},
        {"TEXTURE", GL_TEXTURE},     {"DEPTH_TEST", GL_DEPTH_TEST},
        {"BLEND", GL_BLEND},         {"ALPHA_TEST", GL_ALPHA_TEST},
        {"CULL_FACE", GL_CULL_FACE}, {"LIGHTING", GL_LIGHTING},
        {"FOG", GL_FOG},             {"FRONT", GL_FRONT},
        {"BACK", GL_BACK},           {"FRONT_AND_BACK", GL_FRONT_AND_BACK},
        {"SRC_ALPHA", GL_SRC_ALPHA},
        {"ONE_MINUS_SRC_ALPHA", GL_ONE_MINUS_SRC_ALPHA},
        {"ONE", GL_ONE},             {"ZERO", GL_ZERO},
        {"LINES", GL_LINES},         {"LINE_STRIP", GL_LINE_STRIP},
        {"LINE_LOOP", GL_LINE_LOOP}, {"TRIANGLES", GL_TRIANGLES},
        {"TRIANGLE_STRIP", GL_TRIANGLE_STRIP},
        {"TRIANGLE_FAN", GL_TRIANGLE_FAN},
        {"QUADS", GL_QUADS},         {"POINTS", GL_POINTS},
        {"POLYGON", GL_POLYGON},     {"FILL", GL_FILL},
        {"LINE", GL_LINE},           {"POINT", GL_POINT},
        {"ALL_ATTRIB_BITS", GL_ALL_ATTRIB_BITS},
        {"ENABLE_BIT", GL_ENABLE_BIT},
        {"DEPTH_BUFFER_BIT", GL_DEPTH_BUFFER_BIT},
        {"COLOR_BUFFER_BIT", GL_COLOR_BUFFER_BIT},
        {"POLYGON_BIT", GL_POLYGON_BIT},
        {"LINE_BIT", GL_LINE_BIT},
        {"CURRENT_BIT", GL_CURRENT_BIT},
        {"LIGHTING_BIT", GL_LIGHTING_BIT},
        {"TEXTURE_2D", GL_TEXTURE_2D},
        {"NORMALIZE", GL_NORMALIZE},
        {"COLOR_MATERIAL", GL_COLOR_MATERIAL},
        {"SCISSOR_TEST", GL_SCISSOR_TEST},
        {"STENCIL_TEST", GL_STENCIL_TEST},
    };

    for (auto [name, val] : k_gl_map) gl.add_function(name, [val]{ return val; });
}

// ─── math ────────────────────────────────────────────────────────────────────

void register_math(table_handle m, sol::state& s)
{
    m.add_function("radians", [](double d) noexcept { return glm::radians(d); });
    m.add_function("cos",     [](double v) noexcept { return std::cos(v); });
    m.add_function("sin",     [](double v) noexcept { return std::sin(v); });
    m.add_function("mod",     [](double v, double d) noexcept { return glm::mod(v, d); });
    m.add_function("clamp",   [](double v, double lo, double hi) noexcept
                   { return glm::clamp(v, lo, hi); });
    m.add_function("normalize",
                   [](double x, double y, double z) noexcept
                   { return to_tuple(glm::normalize(vec3(x, y, z))); });
    m.add_function("cross",
                   [](double ax, double ay, double az,
                      double bx, double by, double bz) noexcept
                   { return to_tuple(glm::cross(vec3(ax, ay, az), vec3(bx, by, bz))); });
    m.add_function("lookat_matrix",
                   [&s](double ex, double ey, double ez,
                        double cx, double cy, double cz,
                        double ux, double uy, double uz) -> sol::table
                   {
                       return mat4_to_table(
                           s, glm::lookAt(vec3(ex, ey, ez),
                                          vec3(cx, cy, cz),
                                          vec3(ux, uy, uz)));
                   });
}

// ─── sdk (callbacks + GL + input) ────────────────────────────────────────────

void register_sdk(table_handle root, sol::state& s);

// ─── ui (imgui) ──────────────────────────────────────────────────────────────

void register_ui(table_handle ui);

// ─── entry point ─────────────────────────────────────────────────────────────

void register_all(LuaState& vm)
{
    auto& p = *vm.impl_;
    std::lock_guard lk{p.mutex};

    auto constants_tbl = get_table(p.state, "VK");  // populated inside
    register_constants(get_table(p.state, "_constants"));
    register_math(get_table(p.state, "gmath"), p.state);
    register_sdk(get_table(p.state, "sdk"), p.state);
    register_ui(get_table(p.state, "ui"));
}

} // namespace sdk::lua::detail
