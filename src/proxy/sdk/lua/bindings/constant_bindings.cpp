#include "sdk/lua/bindings/bindings_fwd.hpp"

#include <GL/gl.h>
#include <format>
#include <ranges>
#include <sol/sol.hpp>
#include <windows.h>

namespace sdk::lua::bindings
{

void register_constants(sol::state& sol_state)
{
    // Helper to bulk-register name/value pairs into a table
    constexpr auto bind_constants = [](sol::table& tbl, auto&&... pairs)
    { ((tbl[pairs.first] = pairs.second), ...); };

    // -- Virtual keys --
    auto vk = sol_state.create_named_table("VK");

    bind_constants(vk,
                   std::pair{ "SHIFT", VK_SHIFT },
                   std::pair{ "CONTROL", VK_CONTROL },
                   std::pair{ "SPACE", VK_SPACE },
                   std::pair{ "INSERT", VK_INSERT },
                   std::pair{ "LBUTTON", VK_LBUTTON },
                   std::pair{ "RBUTTON", VK_RBUTTON },
                   std::pair{ "ESCAPE", VK_ESCAPE },
                   std::pair{ "TAB", VK_TAB },
                   std::pair{ "RETURN", VK_RETURN },
                   std::pair{ "BACK", VK_BACK },
                   std::pair{ "DELETE", VK_DELETE },
                   std::pair{ "HOME", VK_HOME },
                   std::pair{ "END", VK_END },
                   std::pair{ "PRIOR", VK_PRIOR },
                   std::pair{ "NEXT", VK_NEXT },
                   std::pair{ "LEFT", VK_LEFT },
                   std::pair{ "RIGHT", VK_RIGHT },
                   std::pair{ "UP", VK_UP },
                   std::pair{ "DOWN", VK_DOWN },
                   std::pair{ "MENU", VK_MENU },
                   std::pair{ "CAPITAL", VK_CAPITAL },
                   std::pair{ "MBUTTON", VK_MBUTTON },
                   std::pair{ "XBUTTON1", VK_XBUTTON1 },
                   std::pair{ "XBUTTON2", VK_XBUTTON2 },
                   std::pair{ "NUMPAD0", VK_NUMPAD0 },
                   std::pair{ "NUMPAD1", VK_NUMPAD1 },
                   std::pair{ "NUMPAD2", VK_NUMPAD2 },
                   std::pair{ "NUMPAD3", VK_NUMPAD3 },
                   std::pair{ "NUMPAD4", VK_NUMPAD4 },
                   std::pair{ "NUMPAD5", VK_NUMPAD5 },
                   std::pair{ "NUMPAD6", VK_NUMPAD6 },
                   std::pair{ "NUMPAD7", VK_NUMPAD7 },
                   std::pair{ "NUMPAD8", VK_NUMPAD8 },
                   std::pair{ "NUMPAD9", VK_NUMPAD9 });

    // A-Z
    for (char c : std::views::iota('A') | std::views::take(26))
    {
        vk[std::string(1, c)] = static_cast<int>(c);
    }

    // 0-9
    for (char c : std::views::iota('0') | std::views::take(10))
    {
        vk[std::string(1, c)] = static_cast<int>(c);
    }

    // F1-F24
    for (int i : std::views::iota(1) | std::views::take(24))
    {
        vk[std::format("F{}", i)] = VK_F1 + (i - 1);
    }

    // -- GL constants --
    auto gl = sol_state.create_named_table("GL");

    bind_constants(gl,
                   std::pair{ "MODELVIEW", GL_MODELVIEW },
                   std::pair{ "PROJECTION", GL_PROJECTION },
                   std::pair{ "TEXTURE", GL_TEXTURE },
                   std::pair{ "DEPTH_TEST", GL_DEPTH_TEST },
                   std::pair{ "BLEND", GL_BLEND },
                   std::pair{ "ALPHA_TEST", GL_ALPHA_TEST },
                   std::pair{ "CULL_FACE", GL_CULL_FACE },
                   std::pair{ "LIGHTING", GL_LIGHTING },
                   std::pair{ "FOG", GL_FOG },
                   std::pair{ "FRONT", GL_FRONT },
                   std::pair{ "BACK", GL_BACK },
                   std::pair{ "FRONT_AND_BACK", GL_FRONT_AND_BACK },
                   std::pair{ "SRC_ALPHA", GL_SRC_ALPHA },
                   std::pair{ "ONE_MINUS_SRC_ALPHA", GL_ONE_MINUS_SRC_ALPHA },
                   std::pair{ "ONE", GL_ONE },
                   std::pair{ "ZERO", GL_ZERO },
                   std::pair{ "LINES", GL_LINES },
                   std::pair{ "LINE_STRIP", GL_LINE_STRIP },
                   std::pair{ "LINE_LOOP", GL_LINE_LOOP },
                   std::pair{ "TRIANGLES", GL_TRIANGLES },
                   std::pair{ "TRIANGLE_STRIP", GL_TRIANGLE_STRIP },
                   std::pair{ "TRIANGLE_FAN", GL_TRIANGLE_FAN },
                   std::pair{ "QUADS", GL_QUADS },
                   std::pair{ "POINTS", GL_POINTS },
                   std::pair{ "POLYGON", GL_POLYGON },
                   // Polygon modes
                   std::pair{ "FILL", GL_FILL },
                   std::pair{ "LINE", GL_LINE },
                   std::pair{ "POINT", GL_POINT },
                   // Attrib bits
                   std::pair{ "ALL_ATTRIB_BITS", GL_ALL_ATTRIB_BITS },
                   std::pair{ "ENABLE_BIT", GL_ENABLE_BIT },
                   std::pair{ "DEPTH_BUFFER_BIT", GL_DEPTH_BUFFER_BIT },
                   std::pair{ "COLOR_BUFFER_BIT", GL_COLOR_BUFFER_BIT },
                   std::pair{ "POLYGON_BIT", GL_POLYGON_BIT },
                   std::pair{ "LINE_BIT", GL_LINE_BIT },
                   std::pair{ "CURRENT_BIT", GL_CURRENT_BIT },
                   std::pair{ "LIGHTING_BIT", GL_LIGHTING_BIT },
                   // Additional caps
                   std::pair{ "TEXTURE_2D", GL_TEXTURE_2D },
                   std::pair{ "NORMALIZE", GL_NORMALIZE },
                   std::pair{ "COLOR_MATERIAL", GL_COLOR_MATERIAL },
                   std::pair{ "SCISSOR_TEST", GL_SCISSOR_TEST },
                   std::pair{ "STENCIL_TEST", GL_STENCIL_TEST });
}

} // namespace sdk::lua::bindings
