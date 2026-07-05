/// @file types.hpp
/// @brief Public SDK types — no implementation details exposed.

#pragma once

#include <cstdint>
#include <functional>
#include <vector>

namespace sdk
{

/// Graphics API detected at runtime.
enum class render_api : uint8_t
{
    unknown,
    opengl,
    directx
};

/// Key codes (Windows virtual key codes for now).
using key_code = int;

/// OpenGL matrix mode (GLenum).
using matrix_mode = unsigned int;

/// Callback signatures — type-erased, no backend leakage.
namespace callback
{
template <typename... Args>
using function = std::function<void(Args...)>;

/// Consuming callback returns bool (true = consumed, stop propagation).
template <typename... Args>
using consuming = std::function<bool(Args...)>;
} // namespace callback

/// List of callbacks with thread-safe registration/invocation.
/// Implementation hidden in detail/callback_list_impl.hpp.
template <typename... Args>
class callback_list;

template <typename... Args>
class consuming_callback_list;

} // namespace sdk
