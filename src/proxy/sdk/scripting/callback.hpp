/// @file callback.hpp
/// @brief Type-erased, thread-safe callback lists for scripting events.

#pragma once

#include "sdk/core/contract.hpp"

#include <algorithm>
#include <functional>
#include <mutex>
#include <utility>
#include <vector>

namespace sdk::scripting
{

/// Thread-safe list of `void(Args...)` callbacks.
template <typename... Args>
class callback_list final
{
public:
    using slot_fn = std::function<void(Args...)>;

    explicit callback_list(std::recursive_mutex& m) noexcept : mtx_{ m } {}

    /// @throws std::invalid_argument if @p fn is empty.
    void add(slot_fn fn)
    {
        require(static_cast<bool>(fn), "callback_list::add: callback must not be empty");
        std::lock_guard lk{ mtx_ };
        fns_.push_back(std::move(fn));
    }

    /// Exceptions thrown by a slot propagate to the caller.
    void invoke(Args... args)
    {
        std::lock_guard lk{ mtx_ };
        std::ranges::for_each(fns_, [&](auto& fn) { fn(args...); });
    }

    void clear()
    {
        std::lock_guard lk{ mtx_ };
        fns_.clear();
    }

    [[nodiscard]] bool empty() const
    {
        std::lock_guard lk{ mtx_ };
        return fns_.empty();
    }

    [[nodiscard]] std::size_t size() const
    {
        std::lock_guard lk{ mtx_ };
        return fns_.size();
    }

private:
    std::recursive_mutex& mtx_;
    std::vector<slot_fn>  fns_;
};

/// Thread-safe list of `bool(Args...)` callbacks with consuming semantics.
/// `invoke()` returns `true` as soon as any slot returns `true`.
template <typename... Args>
class consuming_callback_list final
{
public:
    using slot_fn = std::function<bool(Args...)>;

    explicit consuming_callback_list(std::recursive_mutex& m) noexcept : mtx_{ m } {}

    /// @throws std::invalid_argument if @p fn is empty.
    void add(slot_fn fn)
    {
        require(static_cast<bool>(fn), "consuming_callback_list::add: callback must not be empty");
        std::lock_guard lk{ mtx_ };
        fns_.push_back(std::move(fn));
    }

    [[nodiscard]] bool invoke(Args... args)
    {
        std::lock_guard lk{ mtx_ };
        return std::ranges::any_of(fns_, [&](auto& fn) { return fn(args...); });
    }

    void clear()
    {
        std::lock_guard lk{ mtx_ };
        fns_.clear();
    }

    [[nodiscard]] bool empty() const
    {
        std::lock_guard lk{ mtx_ };
        return fns_.empty();
    }

    [[nodiscard]] std::size_t size() const
    {
        std::lock_guard lk{ mtx_ };
        return fns_.size();
    }

private:
    std::recursive_mutex& mtx_;
    std::vector<slot_fn>  fns_;
};

} // namespace sdk::scripting
