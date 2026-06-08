/// @file lua_state.hpp
/// @brief Private RAII wrapper for sol2 Lua state.
///
/// This header is part of the private implementation and must not be included
/// by client code. It encapsulates all sol2-specific types and operations.

#pragma once

#include <memory>
#include <sol/sol.hpp>
#include <string>

namespace sdk::lua::detail
{

/// @brief RAII wrapper for sol::state with error handling.
/// @details Manages the lifetime of a sol2 state object and provides
///          safe script execution with structured error reporting.
///
/// @invariant The wrapped sol::state is always valid.
/// @invariant All library initialization happens at construction.
class lua_state final
{
public:
    /// @brief Construct and initialize Lua state with standard libraries.
    /// @param enable_std_libs Whether to load standard Lua libraries.
    explicit lua_state(bool enable_std_libs = true);

    /// @brief Destructor. Automatically cleans up Lua state.
    ~lua_state() = default;

    /// @brief Move constructor.
    lua_state(lua_state&&) noexcept = default;

    /// @brief Move assignment operator.
    lua_state& operator=(lua_state&&) noexcept = default;

    /// @brief Copy constructor (deleted).
    lua_state(const lua_state&) = delete;

    /// @brief Copy assignment operator (deleted).
    lua_state& operator=(const lua_state&) = delete;

    /// @brief Execute a Lua script file safely.
    /// @param path Path to the Lua script file.
    /// @return true if script executed successfully, false otherwise.
    /// @note Errors are logged but do not throw exceptions.
    [[nodiscard]] bool execute_file(const std::string& path);

    /// @brief Execute a Lua code string safely.
    /// @param code Lua code to execute.
    /// @return true if code executed successfully, false otherwise.
    [[nodiscard]] bool execute_string(const std::string& code);

    /// @brief Get the underlying sol2 state for binding registration.
    /// @return Reference to the wrapped sol::state.
    /// @warning Direct state access bypasses safety checks. Use with caution.
    [[nodiscard]] sol::state& native_handle() noexcept;

    /// @brief Get the underlying sol2 state for binding registration (const).
    /// @return Const reference to the wrapped sol::state.
    [[nodiscard]] const sol::state& native_handle() const noexcept;

    /// @brief Get the last error message from script execution.
    /// @return Last error message, or empty string if no error.
    [[nodiscard]] const std::string& last_error() const noexcept;

private:
    std::unique_ptr<sol::state> state_;
    std::string last_error_;
};

} // namespace sdk::lua::detail
