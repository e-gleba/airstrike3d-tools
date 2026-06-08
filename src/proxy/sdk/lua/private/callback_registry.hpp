/// @file callback_registry.hpp
/// @brief Private type-erased callback registry for Lua events.
///
/// This header is part of the private implementation. It provides a bridge
/// between sol2 protected_function callbacks and type-erased std::function
/// callbacks used by the public API.

#pragma once

#include <mutex>
#include <sol/sol.hpp>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace sdk::lua::detail
{

/// @brief Thread-safe registry for Lua and C++ callbacks.
/// @details Manages both sol2 protected_function callbacks (from Lua scripts)
///          and std::function callbacks (from C++ code) in a unified interface.
///
/// @invariant All operations are thread-safe via internal mutex.
/// @invariant Callbacks are invoked in registration order.
class callback_registry final
{
public:
    /// @brief Callback type for C++ callbacks.
    using cpp_callback = std::function<void()>;

    /// @brief Construct an empty registry.
    callback_registry() = default;

    /// @brief Destructor.
    ~callback_registry() = default;

    // Non-copyable, non-movable (contains mutex)
    callback_registry(const callback_registry&) = delete;
    callback_registry& operator=(const callback_registry&) = delete;
    callback_registry(callback_registry&&) = delete;
    callback_registry& operator=(callback_registry&&) = delete;

    /// @brief Add a Lua callback for an event.
    /// @param event_name Name of the event.
    /// @param callback sol2 protected function to register.
    void add_lua_callback(std::string_view event_name, sol::protected_function callback);

    /// @brief Add a C++ callback for an event.
    /// @param event_name Name of the event.
    /// @param callback std::function to register.
    void add_cpp_callback(std::string_view event_name, cpp_callback callback);

    /// @brief Invoke all callbacks for an event.
    /// @param event_name Name of the event to invoke.
    /// @return Number of callbacks successfully invoked.
    std::size_t invoke(std::string_view event_name);

    /// @brief Invoke callbacks and return true if any consumed the event.
    /// @param event_name Name of the event to invoke.
    /// @return true if any callback returned true.
    [[nodiscard]] bool invoke_consuming(std::string_view event_name);

    /// @brief Clear all callbacks for a specific event.
    /// @param event_name Name of the event to clear.
    void clear(std::string_view event_name);

    /// @brief Clear all callbacks for all events.
    void clear_all();

    /// @brief Check if any callbacks are registered for an event.
    /// @param event_name Name of the event to check.
    /// @return true if at least one callback is registered.
    [[nodiscard]] bool has_callbacks(std::string_view event_name) const;

    /// @brief Get the number of registered callbacks for an event.
    /// @param event_name Name of the event to query.
    /// @return Number of registered callbacks.
    [[nodiscard]] std::size_t count(std::string_view event_name) const;

private:
    /// @brief Internal storage for a single event's callbacks.
    struct event_callbacks
    {
        std::vector<sol::protected_function> lua_callbacks;
        std::vector<cpp_callback> cpp_callbacks;
    };

    mutable std::recursive_mutex mutex_;
    std::unordered_map<std::string, event_callbacks> events_;

    /// @brief Log errors from Lua callback execution.
    static void log_lua_error(const sol::protected_function_result& result);
};

} // namespace sdk::lua::detail
