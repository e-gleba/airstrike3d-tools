/// @file engine.hpp
/// @brief Public scripting engine interface — no backend types exposed.
///
/// This header contains **no Lua/sol2 types** — all backend-specific
/// implementation is hidden behind the pimpl idiom in detail/.

#pragma once

#include <filesystem>
#include <memory>
#include <string>

namespace sdk::scripting
{

/// RAII scripting interpreter.
///
/// Owns the interpreter state and manages its lifetime. When destroyed,
/// the engine is torn down cleanly.
///
/// Thread-safety: callback lists in g_ctx.cb use their own mutex.
/// The interpreter state itself should only be accessed from one thread at a time.
class Engine
{
public:
    /// Initialize the scripting interpreter.
    /// Opens standard libraries, registers C++ bindings, and sets up
    /// callback hooks (hook_frame, hook_overlay, hook_key_down, etc.).
    ///
    /// @throws std::runtime_error if initialization fails
    Engine();
    
    /// Shut down the scripting interpreter.
    /// Clears all callbacks and releases resources.
    ~Engine();

    // Non-copyable, movable
    Engine(const Engine&) = delete;
    Engine& operator=(const Engine&) = delete;
    Engine(Engine&&) noexcept;
    Engine& operator=(Engine&&) noexcept;

    /// Load all scripts from the plugins/ directory.
    /// Calls on_load callback after loading.
    ///
    /// @throws std::runtime_error if loading fails
    void load_plugins();

    /// Unload all plugins.
    /// Calls on_unload callback and clears all registered callbacks.
    void unload_plugins();

    /// Execute a script string.
    ///
    /// @param code Script code to execute
    /// @throws std::runtime_error if execution fails
    void execute(std::string_view code);

    /// Execute a script file.
    ///
    /// @param path Path to script file
    /// @throws std::runtime_error if file not found or execution fails
    void execute_file(const std::filesystem::path& path);

private:
    struct impl;
    std::unique_ptr<impl> pimpl;
};

} // namespace sdk::scripting
