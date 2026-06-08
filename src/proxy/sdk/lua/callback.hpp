/// @file callback.hpp
/// @brief Type-erased callback storage for Lua ↔ C++ events.
///
/// Provides two callback list types:
/// - `callback_list` for void() callbacks (on_frame, on_overlay, etc.)
/// - `consuming_callbacks` for bool() callbacks (on_key_down)
///
/// Both store `std::function` — no sol2 types leak into the public API.
/// The Lua engine adapter converts `sol::protected_function` to
/// `std::function` before storing.

#pragma once

#include <functional>
#include <memory>

namespace sdk::lua
{

/// Thread-safe list of void() callbacks.
class callback_list
{
public:
    using fn_type = std::function<void()>;

    callback_list();
    ~callback_list();

    callback_list(callback_list&&) noexcept;
    callback_list& operator=(callback_list&&) noexcept;

    callback_list(const callback_list&)            = delete;
    callback_list& operator=(const callback_list&) = delete;

    /// Add a callback to the list.
    void add(fn_type fn);

    /// Invoke all callbacks in order.
    void invoke();

    /// Remove all callbacks.
    void clear();

    /// Check if the list is empty.
    [[nodiscard]] bool empty() const;

private:
    struct impl;
    std::unique_ptr<impl> pimpl;
};

/// Thread-safe list of bool() callbacks with consuming semantics.
/// Returns true from invoke() if any callback returned true.
class consuming_callbacks
{
public:
    using fn_type = std::function<bool()>;

    consuming_callbacks();
    ~consuming_callbacks();

    consuming_callbacks(consuming_callbacks&&) noexcept;
    consuming_callbacks& operator=(consuming_callbacks&&) noexcept;

    consuming_callbacks(const consuming_callbacks&)            = delete;
    consuming_callbacks& operator=(const consuming_callbacks&) = delete;

    /// Add a callback to the list.
    void add(fn_type fn);

    /// Invoke callbacks until one returns true, then stop.
    /// Returns true if any callback consumed the event.
    [[nodiscard]] bool invoke();

    /// Remove all callbacks.
    void clear();

    /// Check if the list is empty.
    [[nodiscard]] bool empty() const;

private:
    struct impl;
    std::unique_ptr<impl> pimpl;
};

} // namespace sdk::lua
