/// @file lua_engine.hpp
/// @brief Public interface for the Lua scripting engine.
///
/// This header provides a clean C++23 API with no exposure to the underlying
/// Lua binding library (sol2). All implementation details are hidden in the
/// private implementation to enable future backend swaps without breaking
/// client code.
///
/// @note Thread Safety: The engine is thread-safe for concurrent read operations.
///       Script loading and unloading must be externally synchronized.
///
/// @example
/// @code
/// sdk::lua::engine eng;
/// eng.load_directory("plugins");
/// eng.invoke_callback("on_frame");
/// @endcode

#pragma once

#include <filesystem>
#include <functional>
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace sdk::lua
{

/// @brief Callback function type for Lua script events.
/// @note Uses std::function for type erasure, allowing any callable.
using callback_fn = std::function<void()>;

/// @brief Result of a script execution.
/// @details Provides structured error information when script execution fails.
struct [[nodiscard]] script_result
{
    bool success{false};
    std::string error_message;
    std::string script_path;
};

/// @brief Configuration for the Lua engine.
/// @details Immutable configuration passed at construction time.
struct engine_config
{
    std::filesystem::path plugin_directory{"plugins"};
    bool auto_create_plugin_dir{true};
    bool enable_standard_libs{true};
};

/// @brief RAII wrapper for the Lua scripting engine.
/// @details Manages the lifetime of the Lua state and provides a clean interface
///          for script loading, callback registration, and script invocation.
///
/// @invariant The engine maintains a valid Lua state throughout its lifetime.
/// @invariant All callbacks are cleared when the engine is destroyed.
///
/// @note Move-only type. Copying is disabled to prevent state duplication issues.
class engine final
{
public:
    /// @brief Construct engine with default configuration.
    engine();

    /// @brief Construct engine with custom configuration.
    /// @param config Engine configuration parameters.
    explicit engine(engine_config config);

    /// @brief Destructor. Cleans up Lua state and all registered callbacks.
    ~engine();

    /// @brief Move constructor.
    engine(engine&&) noexcept;

    /// @brief Move assignment operator.
    engine& operator=(engine&&) noexcept;

    /// @brief Copy constructor (deleted).
    engine(const engine&) = delete;

    /// @brief Copy assignment operator (deleted).
    engine& operator=(const engine&) = delete;

    /// @brief Load all Lua scripts from the configured plugin directory.
    /// @return Vector of results, one per script. Empty if no scripts found.
    /// @throws std::runtime_error if plugin directory cannot be created.
    /// @note Scripts are loaded in alphabetical order for deterministic behavior.
    [[nodiscard]] std::vector<script_result> load_plugins();

    /// @brief Load a single Lua script file.
    /// @param path Path to the Lua script file.
    /// @return Result indicating success or failure with error details.
    [[nodiscard]] script_result load_script(const std::filesystem::path& path);

    /// @brief Unload all plugins and clear all registered callbacks.
    /// @details Invokes the "on_unload" callback before clearing state.
    void unload_plugins();

    /// @brief Register a C++ callback for a named Lua event.
    /// @param event_name Name of the event (e.g., "on_frame", "on_key_down").
    /// @param callback Function to invoke when the event fires.
    /// @note Callbacks are invoked in registration order.
    void register_callback(std::string_view event_name, callback_fn callback);

    /// @brief Invoke all registered callbacks for a named event.
    /// @param event_name Name of the event to invoke.
    /// @return Number of callbacks successfully invoked.
    std::size_t invoke_callback(std::string_view event_name);

    /// @brief Invoke callbacks and return true if any consumed the event.
    /// @param event_name Name of the event to invoke.
    /// @return true if any callback returned true (event consumed).
    /// @details Useful for input handling where first consumer wins.
    [[nodiscard]] bool invoke_callback_consuming(std::string_view event_name);

    /// @brief Clear all registered callbacks for a specific event.
    /// @param event_name Name of the event to clear.
    void clear_callbacks(std::string_view event_name);

    /// @brief Clear all registered callbacks for all events.
    void clear_all_callbacks();

    /// @brief Check if any callbacks are registered for an event.
    /// @param event_name Name of the event to check.
    /// @return true if at least one callback is registered.
    [[nodiscard]] bool has_callbacks(std::string_view event_name) const;

    /// @brief Get the number of registered callbacks for an event.
    /// @param event_name Name of the event to query.
    /// @return Number of registered callbacks.
    [[nodiscard]] std::size_t callback_count(std::string_view event_name) const;

private:
    struct impl;
    std::unique_ptr<impl> pimpl_;
};

} // namespace sdk::lua
