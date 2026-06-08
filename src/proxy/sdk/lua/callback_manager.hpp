/// @file callback_manager.hpp
/// @brief Type-erased, thread-safe callback storage for Lua ↔ C++ events.
///
/// callback_manager stores `std::function` objects — **not** sol2 types.
/// The conversion from a backend-specific callable (e.g.
/// `sol::protected_function`) to `std::function` happens in the
/// registration adapter (`detail/register_bindings.cpp`).
///
/// This design ensures the callback subsystem can survive a complete
/// backend swap without any public-interface changes.

#pragma once

#include <cstddef>
#include <functional>
#include <memory>
#include <mutex>
#include <utility>

namespace sdk::lua
{

class callback_manager
{
public:
    /// Signature of a stored callback.
    ///
    /// All Lua → C++ callbacks are normalised to this type by the
    /// backend adapter.  Arguments are passed as a generic pack so
    /// that a single list can serve heterogeneous event signatures.
    using slot_fn = std::function<void()>;

    /// Construct with an external mutex (shared with the LuaState).
    explicit callback_manager(std::recursive_mutex& mtx) noexcept;
    ~callback_manager();

    callback_manager(callback_manager&&) noexcept;
    callback_manager& operator=(callback_manager&&) noexcept;

    callback_manager(const callback_manager&)            = delete;
    callback_manager& operator=(const callback_manager&) = delete;

    /// Register a callable.
    void add(slot_fn fn);

    /// Invoke every stored callable with no arguments.
    void invoke();

    /// Invoke every stored callable; return `true` as soon as one
    /// returns `true` (consuming semantics — useful for input events).
    ///
    /// The slot_fn for consuming events is expected to encode the
    /// return value via a side channel; see the detail adapter.
    [[nodiscard]] bool invoke_consuming();

    /// Remove all stored callables.
    void clear();

    /// Whether the list is empty.
    [[nodiscard]] bool empty() const;

private:
    struct impl;
    std::unique_ptr<impl> impl_;
};

} // namespace sdk::lua
