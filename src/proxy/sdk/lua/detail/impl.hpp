/// @file detail/impl.hpp
/// @brief Backend-specific LuaState implementation (sol2).
///
/// This header is **private** to the `detail/` translation units.
/// It must never be included from a public header or from `bindings/`.

#pragma once

#include <sol/sol.hpp>

#include <memory>
#include <mutex>

namespace sdk::lua
{

/// Concrete pimpl for LuaState.
///
/// Wraps a `sol::state` and the shared recursive mutex that serialises
/// all interpreter access (including callbacks).
struct LuaState::impl
{
    sol::state              state;
    std::recursive_mutex    mutex;

    impl();
    ~impl();
};

/// Concrete pimpl for callback_manager.
///
/// Stores `std::function<void()>` slots; the conversion from
/// `sol::protected_function` is performed by the adapter in
/// `register_bindings.cpp`.
struct callback_manager::impl
{
    std::recursive_mutex&              mutex;
    std::vector<std::function<void()>> slots;

    // Consuming-invoke result channel (set by adapter before invoke).
    std::function<bool()> consuming_slot;

    explicit impl(std::recursive_mutex& m) noexcept : mutex{m} {}
};

} // namespace sdk::lua
