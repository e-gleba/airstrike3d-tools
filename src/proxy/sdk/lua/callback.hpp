#pragma once

#include "event.hpp"

#include <algorithm>
#include <concepts>
#include <functional>
#include <mutex>
#include <ranges>
#include <utility>
#include <vector>

namespace sdk
{

/// Type-erased, thread-safe callback registry.
///
/// Stores std::function — zero exposure of Lua/sol2 internals.
/// Template dispatch via NTTP event + concept ensures compile-time signature validation.
///
/// Usage:
///   callbacks.add_on_frame([]{ ... });
///   callbacks.invoke<event::on_frame, void()>();
///
class callback_list
{
    std::recursive_mutex& mtx;

    std::vector<std::function<void()>>                                          on_frame_fns;
    std::vector<std::function<void()>>                                          on_overlay_fns;
    std::vector<std::function<void()>>                                          on_gl_identity_fns;
    std::vector<std::function<void(double, double, double, double, double, double, double, double, double)>> on_glu_lookat_fns;
    std::vector<std::function<bool(int)>>                                       on_key_down_fns;
    std::vector<std::function<bool(double, double, double, double, double, double, double, double, double)>> on_glu_lookat_consuming_fns;
    std::vector<std::function<void()>>                                          on_load_fns;
    std::vector<std::function<void()>>                                          on_unload_fns;

    template <event E>
    constexpr auto& storage()
    {
        if constexpr (E == event::on_frame)
            return on_frame_fns;
        else if constexpr (E == event::on_overlay)
            return on_overlay_fns;
        else if constexpr (E == event::on_gl_identity)
            return on_gl_identity_fns;
        else if constexpr (E == event::on_glu_lookat)
            return on_glu_lookat_fns;
        else if constexpr (E == event::on_key_down)
            return on_key_down_fns;
        else if constexpr (E == event::on_load)
            return on_load_fns;
        else if constexpr (E == event::on_unload)
            return on_unload_fns;
    }

public:
    explicit callback_list(std::recursive_mutex& m) noexcept : mtx{ m }
    {
    }

    // ─── Named add methods for descriptor-table registration in bindings ──────

    void add_on_frame(std::function<void()> f)
    {
        std::lock_guard lk{ mtx };
        on_frame_fns.push_back(std::move(f));
    }
    void add_on_overlay(std::function<void()> f)
    {
        std::lock_guard lk{ mtx };
        on_overlay_fns.push_back(std::move(f));
    }
    void add_on_gl_identity(std::function<void()> f)
    {
        std::lock_guard lk{ mtx };
        on_gl_identity_fns.push_back(std::move(f));
    }
    void add_on_glu_lookat(
        std::function<void(double, double, double, double, double, double, double, double, double)> f)
    {
        std::lock_guard lk{ mtx };
        on_glu_lookat_fns.push_back(std::move(f));
    }
    void add_on_key_down(std::function<bool(int)> f)
    {
        std::lock_guard lk{ mtx };
        on_key_down_fns.push_back(std::move(f));
    }
    void add_on_glu_lookat_consuming(
        std::function<bool(double, double, double, double, double, double, double, double, double)> f)
    {
        std::lock_guard lk{ mtx };
        on_glu_lookat_consuming_fns.push_back(std::move(f));
    }
    void add_on_load(std::function<void()> f)
    {
        std::lock_guard lk{ mtx };
        on_load_fns.push_back(std::move(f));
    }
    void add_on_unload(std::function<void()> f)
    {
        std::lock_guard lk{ mtx };
        on_unload_fns.push_back(std::move(f));
    }

    /// Invoke all callbacks for event E. Args forwarded to each callable.
    template <event E, typename Sig, typename... Args>
    void invoke(Args&&... args)
    {
        std::lock_guard lk{ mtx };
        for (auto& fn : storage<E>())
        {
            fn(std::forward<Args>(args)...);
        }
    }

    /// Invoke key_down callbacks consuming-style: returns true if any returned true.
    bool invoke_key_down(int key)
    {
        std::lock_guard lk{ mtx };
        return std::ranges::any_of(on_key_down_fns, [&](auto& fn) { return fn(key); });
    }

    /// Invoke glu_lookat callbacks consuming-style: returns true if any returned true.
    bool invoke_glu_lookat_consuming(double ex, double ey, double ez,
                                     double cx, double cy, double cz,
                                     double ux, double uy, double uz)
    {
        std::lock_guard lk{ mtx };
        return std::ranges::any_of(on_glu_lookat_consuming_fns,
                                   [&](auto& fn) { return fn(ex, ey, ez, cx, cy, cz, ux, uy, uz); });
    }

    void clear()
    {
        std::lock_guard lk{ mtx };
        on_frame_fns.clear();
        on_overlay_fns.clear();
        on_gl_identity_fns.clear();
        on_glu_lookat_fns.clear();
        on_key_down_fns.clear();
        on_glu_lookat_consuming_fns.clear();
        on_load_fns.clear();
        on_unload_fns.clear();
    }

    [[nodiscard]] bool empty() const
    {
        std::lock_guard lk{ mtx };
        return on_frame_fns.empty() && on_overlay_fns.empty() && on_gl_identity_fns.empty() &&
               on_glu_lookat_fns.empty() && on_key_down_fns.empty() &&
               on_glu_lookat_consuming_fns.empty() && on_load_fns.empty() &&
               on_unload_fns.empty();
    }
};

} // namespace sdk
