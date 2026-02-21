#pragma once
#include <concepts>
#include <mutex>
#include <sol/sol.hpp>
#include <spdlog/spdlog.h>
#include <vector>

namespace sdk
{

// A thread-safe, mutex-guarded list of sol::protected_function callbacks.
class callback_list final
{
public:
    explicit callback_list(std::recursive_mutex& mtx)
        : mtx(mtx)
    {
    }

    void add(sol::protected_function fn)
    {
        std::lock_guard lk(mtx);
        fns.push_back(std::move(fn));
    }

    // Invoke all callbacks with args, log errors. Returns nothing.
    template <typename... Args> void invoke(Args&&... args)
    {
        std::lock_guard lk(mtx);
        for (auto& fn : fns)
        {
            if (auto r = fn(std::forward<Args>(args)...); !r.valid())
            {
                sol::error err = r;
                spdlog::error("[sdk] lua callback error: {}", err.what());
            }
        }
    }

    // Invoke all callbacks; if any returns true, short-circuit and return true.
    template <typename... Args>
    [[nodiscard]] bool invoke_consuming(Args&&... args)
    {
        std::lock_guard lk(mtx);
        for (auto& fn : fns)
        {
            if (auto r = fn(std::forward<Args>(args)...); r.valid())
            {
                if (sol::optional<bool> b = r; b.has_value() && *b)
                {
                    return true;
                }
            }
            else
            {
                sol::error err = r;
                spdlog::error("[sdk] lua callback error: {}", err.what());
            }
        }
        return false;
    }

    void clear()
    {
        std::lock_guard lk(mtx);
        fns.clear();
    }

    [[nodiscard]] bool empty()
    {
        std::lock_guard lk(mtx);
        return fns.empty();
    }

private:
    std::recursive_mutex&                mtx;
    std::vector<sol::protected_function> fns;
};

} // namespace sdk