#pragma once

#include <string_view>

namespace euengine::platform
{

enum class os : unsigned char
{
    windows,
    linux_os,
    macos,
    unknown
};

[[nodiscard]] consteval os current_os() noexcept
{
#if defined(_WIN32) || defined(_WIN64)
    return os::windows;
#elif defined(__linux__)
    return os::linux_os;
#elif defined(__APPLE__) && defined(__MACH__)
    return os::macos;
#else
    return os::unknown;
#endif
}

[[nodiscard]] consteval bool is_windows() noexcept
{
    return current_os() == os::windows;
}

[[nodiscard]] consteval bool is_linux() noexcept
{
    return current_os() == os::linux_os;
}

[[nodiscard]] consteval bool is_macos() noexcept
{
    return current_os() == os::macos;
}

[[nodiscard]] consteval bool is_posix() noexcept
{
    return is_linux() || is_macos();
}

[[nodiscard]] consteval std::string_view shared_lib_extension() noexcept
{
    if constexpr (is_windows())
    {
        return ".dll";
    }
    else if constexpr (is_macos())
    {
        return ".dylib";
    }
    else
    {
        return ".so";
    }
}

[[nodiscard]] consteval std::string_view shared_lib_prefix() noexcept
{
    if constexpr (is_windows())
    {
        return "";
    }
    else
    {
        return "lib";
    }
}

[[nodiscard]] consteval std::string_view game_library_name() noexcept
{
    if constexpr (is_windows())
    {
        return "game.dll";
    }
    else if constexpr (is_macos())
    {
        return "libgame.dylib";
    }
    else
    {
        return "libgame.so";
    }
}

[[nodiscard]] consteval bool is_debug_build() noexcept
{
#if defined(NDEBUG)
    return false;
#else
    return true;
#endif
}

} // namespace euengine::platform
