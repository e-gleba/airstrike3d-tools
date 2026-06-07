#pragma once

#include <cstdint>

namespace sdk
{

/// Compile-time event identifier.
/// Zero-cost abstraction: string_view stored inline, no heap allocation.
struct event_key
{
    const char* data;
    std::size_t size;

    consteval event_key(const char* str) noexcept : data{ str }, size{ __builtin_strlen(str) }
    {
    }

    [[nodiscard]] consteval auto operator<=>(const event_key&) const noexcept = default;
};

} // namespace sdk
