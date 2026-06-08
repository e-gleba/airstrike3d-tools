/// @file detail/lua_state.cpp
/// @brief LuaState member definitions — sol2 backend.

#include "sdk/lua/lua_state.hpp"
#include "sdk/lua/detail/impl.hpp"

#include <spdlog/spdlog.h>

#include <utility>

namespace sdk::lua
{

// ── impl ─────────────────────────────────────────────────────────────────────

LuaState::impl::impl()
{
    // Nothing — libraries are opened explicitly via open_library().
}

LuaState::impl::~impl() = default;

// ── LuaState ─────────────────────────────────────────────────────────────────

LuaState::LuaState()  : impl_{std::make_unique<impl>()} {}
LuaState::~LuaState() = default;

LuaState::LuaState(LuaState&&) noexcept            = default;
LuaState& LuaState::operator=(LuaState&&) noexcept = default;

void LuaState::open_library(lib l)
{
    std::lock_guard lk{impl_->mutex};

    const auto sol_lib = [](lib l) -> sol::lib
    {
        switch (l)
        {
            case lib::base:    return sol::lib::base;
            case lib::math:    return sol::lib::math;
            case lib::string:  return sol::lib::string;
            case lib::table:   return sol::lib::table;
            case lib::io:      return sol::lib::io;
            case lib::os:      return sol::lib::os;
            case lib::package: return sol::lib::package;
        }
        return sol::lib::base; // unreachable
    }(l);

    impl_->state.open_libraries(sol_lib);
}

void LuaState::open_libraries(std::initializer_list<lib> libs)
{
    for (auto l : libs) open_library(l);
}

exec_result LuaState::exec_file(const std::filesystem::path& path)
{
    std::lock_guard lk{impl_->mutex};

    auto result = impl_->state.safe_script_file(
        path.string(), sol::script_pass_on_error);

    if (!result.valid())
    {
        sol::error err{result};
        return {.ok = false, .error = err.what()};
    }
    return {.ok = true};
}

exec_result LuaState::exec_string(std::string_view code)
{
    std::lock_guard lk{impl_->mutex};

    auto result = impl_->state.safe_script(
        std::string{code}, sol::script_pass_on_error);

    if (!result.valid())
    {
        sol::error err{result};
        return {.ok = false, .error = err.what()};
    }
    return {.ok = true};
}

void LuaState::add_function(std::string name, std::function<void()> fn)
{
    std::lock_guard lk{impl_->mutex};
    impl_->state[name] = std::move(fn);
}

void LuaState::create_table(std::string_view name)
{
    std::lock_guard lk{impl_->mutex};
    impl_->state.create_named_table(std::string{name});
}

bool LuaState::valid() const noexcept
{
    return impl_ != nullptr;
}

void LuaState::reset()
{
    std::lock_guard lk{impl_->mutex};
    impl_->state = {};
}

} // namespace sdk::lua
