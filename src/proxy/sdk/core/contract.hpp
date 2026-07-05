/// @file contract.hpp
/// @brief Precondition and postcondition checks (throws on violation).

#pragma once

#include <stdexcept>
#include <string>
#include <string_view>

namespace sdk
{

/// @throws std::invalid_argument when @p condition is false.
[[noreturn]] inline void contract_fail(std::string_view message)
{
    throw std::invalid_argument(std::string{ message });
}

/// @throws std::runtime_error when @p condition is false.
[[noreturn]] inline void ensure_fail(std::string_view message)
{
    throw std::runtime_error(std::string{ message });
}

inline void require(bool condition, std::string_view message)
{
    if (!condition)
    {
        contract_fail(message);
    }
}

inline void ensure(bool condition, std::string_view message)
{
    if (!condition)
    {
        ensure_fail(message);
    }
}

} // namespace sdk
