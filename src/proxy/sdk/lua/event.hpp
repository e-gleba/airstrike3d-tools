#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <string_view>

namespace sdk
{

/// Scripting callback event identifiers.
enum class event : std::uint8_t
{
    on_frame,
    on_overlay,
    on_gl_identity,
    on_glu_lookat,
    on_key_down,
    on_load,
    on_unload,
    count
};

/// Compile-time event name lookup. Zero-cost: returns std::string_view into static data.
[[nodiscard]] constexpr auto to_string_view(event e) noexcept -> std::string_view
{
    constexpr std::array names = {
        std::string_view{ "on_frame" },
        std::string_view{ "on_overlay" },
        std::string_view{ "on_gl_identity" },
        std::string_view{ "on_glu_lookat" },
        std::string_view{ "on_key_down" },
        std::string_view{ "on_load" },
        std::string_view{ "on_unload" },
    };
    return names[static_cast<std::size_t>(e)];
}

} // namespace sdk
