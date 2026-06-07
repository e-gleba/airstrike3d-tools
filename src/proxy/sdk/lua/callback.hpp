#pragma once

#include <sol/sol.hpp>
#include <spdlog/spdlog.h>

#include <mutex>
#include <optional>
#include <ranges>
#include <utility>
#include <vector>

namespace sdk
{

class callback_list final
{
    std::recursive_mutex&                mtx;
    std::vector<sol::protected_function> fns;

    static void log_if_error(sol::protected_function_result const& r)
    {
        if (!r.valid())
        {
            spdlog::error("[sdk] lua callback error: {}",
                          sol::error{ r }.what());
        }
    }

public:
    explicit callback_list(std::recursive_mutex& m) noexcept
        : mtx{ m }
    {
    }

    void add(sol::protected_function fn)
    {
        std::lock_guard lk{ mtx };
        fns.push_back(std::move(fn));
    }

    template <typename... Args> void invoke(Args&&... args)
    {
        std::lock_guard lk{ mtx };
        std::ranges::for_each(
            fns,
            [&](auto& fn) { log_if_error(fn(std::forward<Args>(args)...)); });
    }

    template <typename... Args>
    [[nodiscard]] bool invoke_consuming(Args&&... args)
    {
        std::lock_guard lk{ mtx };
        return std::ranges::any_of(
            fns,
            [&](auto& fn)
            {
                auto r{ fn(std::forward<Args>(args)...) };
                if (!r.valid())
                {
                    log_if_error(r);
                    return false;
                }
                return sol::optional<bool>{ r }.value_or(false);
            });
    }

    void clear()
    {
        std::lock_guard lk{ mtx };
        fns.clear();
    }

    [[nodiscard]] bool empty()
    {
        std::lock_guard lk{ mtx };
        return fns.empty();
    }
};

} // namespace sdk