/// @file callback.hpp
/// @brief Type-erased, thread-safe callback lists for Lua ↔ C++ events.
///
/// Stores `std::function` — **no sol2 types** leak through this interface.
/// The conversion from `sol::protected_function` to `std::function` happens
/// inside `detail/lua_engine.cpp`.
///
/// Two list types:
///   - `callback_list<Args...>` — fire-and-forget, invokes all slots
///   - `consuming_callback_list<Args...>` — short-circuits on first `true`

#pragma once

#include <algorithm>
#include <functional>
#include <mutex>
#include <ranges>
#include <utility>
#include <vector>

namespace sdk::lua
{

/// Thread-safe list of `void(Args...)` callbacks.
template <typename... Args>
class callback_list final
{
public:
    using slot_fn = std::function<void(Args...)>;

    explicit callback_list(std::recursive_mutex& m) noexcept : mtx_{m} {}

    void add(slot_fn fn)
    {
        std::lock_guard lk{mtx_};
        fns_.push_back(std::move(fn));
    }

    void invoke(Args... args)
    {
        std::lock_guard lk{mtx_};
        std::ranges::for_each(fns_, [&](auto& fn) { fn(args...); });
    }

    void clear() noexcept
    {
        std::lock_guard lk{mtx_};
        fns_.clear();
    }

    [[nodiscard]] bool empty() const noexcept
    {
        std::lock_guard lk{mtx_};
        return fns_.empty();
    }

    [[nodiscard]] std::size_t size() const noexcept
    {
        std::lock_guard lk{mtx_};
        return fns_.size();
    }

private:
    std::recursive_mutex&     mtx_;
    std::vector<slot_fn>      fns_;
};

/// Thread-safe list of `bool(Args...)` callbacks with consuming semantics.
/// `invoke()` returns `true` as soon as any slot returns `true`.
template <typename... Args>
class consuming_callback_list final
{
public:
    using slot_fn = std::function<bool(Args...)>;

    explicit consuming_callback_list(std::recursive_mutex& m) noexcept : mtx_{m} {}

    void add(slot_fn fn)
    {
        std::lock_guard lk{mtx_};
        fns_.push_back(std::move(fn));
    }

    [[nodiscard]] bool invoke(Args... args)
    {
        std::lock_guard lk{mtx_};
        return std::ranges::any_of(fns_, [&](auto& fn) { return fn(args...); });
    }

    void clear() noexcept
    {
        std::lock_guard lk{mtx_};
        fns_.clear();
    }

    [[nodiscard]] bool empty() const noexcept
    {
        std::lock_guard lk{mtx_};
        return fns_.empty();
    }

    [[nodiscard]] std::size_t size() const noexcept
    {
        std::lock_guard lk{mtx_};
        return fns_.size();
    }

private:
    std::recursive_mutex&     mtx_;
    std::vector<slot_fn>      fns_;
};

} // namespace sdk::lua
