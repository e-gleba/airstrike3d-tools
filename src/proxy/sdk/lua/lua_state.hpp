/// @file lua_state.hpp
/// @brief RAII wrapper for a Lua execution context.
///
/// LuaState owns the interpreter and every resource associated with it.
/// When the object is destroyed the state is torn down cleanly — no
/// raw `lua_State*`, no `sol::state`, no manual lifetime management.
///
/// The implementation uses the pimpl idiom so that **no backend-specific
/// types appear in this header**.  Swapping sol2 for another binding
/// library requires changes only inside `detail/`.

#pragma once

#include <cstddef>
#include <functional>
#include <memory>
#include <string>
#include <string_view>

namespace sdk::lua
{

/// Outcome of a script execution request.
struct exec_result
{
    bool        ok{};
    std::string error;

    [[nodiscard]] explicit operator bool() const noexcept { return ok; }
};

/// Standard libraries that can be selectively opened.
enum class lib
{
    base,
    math,
    string,
    table,
    io,
    os,
    package,
};

// ─────────────────────────────────────────────────────────────────────────────

/// RAII Lua interpreter.
///
/// Thread-safety: a single internal mutex serialises every operation.
/// Callers that need to hold the lock across multiple calls should use
/// the callback_manager (which shares the same mutex) instead of going
/// through the public API.
class LuaState
{
public:
    LuaState();
    ~LuaState();

    LuaState(LuaState&&) noexcept;
    LuaState& operator=(LuaState&&) noexcept;

    LuaState(const LuaState&)            = delete;
    LuaState& operator=(const LuaState&) = delete;

    /// Open one standard library.
    void open_library(lib l);

    /// Open a set of standard libraries.
    void open_libraries(std::initializer_list<lib> libs);

    /// Execute a Lua source file.
    [[nodiscard]] exec_result exec_file(const std::filesystem::path& path);

    /// Execute a Lua source string.
    [[nodiscard]] exec_result exec_string(std::string_view code);

    /// Register a C++ function that Lua scripts can invoke.
    ///
    /// @param name  Global name visible from Lua (e.g. `"on_frame"`).
    /// @param fn    Callable — arguments and return values are converted
    ///              automatically by the backend.
    void add_function(std::string name, std::function<void()> fn);

    /// Create (or retrieve) a named global table and return a handle
    /// that can be used to populate it.
    ///
    /// The handle type is opaque — see `detail/table_handle.hpp`.
    /// For public use, prefer the `register_bindings` entry point
    /// which hides the handle entirely.
    void create_table(std::string_view name);

    /// Whether the interpreter has been initialised and is ready.
    [[nodiscard]] bool valid() const noexcept;

    /// Reset the interpreter, releasing all scripts and registered
    /// functions.  The object remains usable — a new state is created
    /// on the next operation.
    void reset();

private:
    struct impl;
    std::unique_ptr<impl> impl_;

    // callback_manager needs access to the shared mutex and
    // backend-specific handle to convert sol::protected_function.
    friend class callback_manager;
};

} // namespace sdk::lua
