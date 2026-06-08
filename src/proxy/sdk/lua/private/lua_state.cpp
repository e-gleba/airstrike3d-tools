/// @file lua_state.cpp
/// @brief Implementation of the private Lua state wrapper.

#include "lua_state.hpp"

#include <spdlog/spdlog.h>

namespace sdk::lua::detail
{

lua_state::lua_state(const bool enable_std_libs)
    : state_{std::make_unique<sol::state>()}
{
    if (enable_std_libs)
    {
        state_->open_libraries(sol::lib::base,
                               sol::lib::math,
                               sol::lib::string,
                               sol::lib::table,
                               sol::lib::io,
                               sol::lib::os,
                               sol::lib::package);
    }
}

bool lua_state::execute_file(const std::string& path)
{
    last_error_.clear();

    try
    {
        auto result{state_->safe_script_file(path, sol::script_pass_on_error)};

        if (!result.valid())
        {
            sol::error err{result};
            last_error_ = err.what();
            spdlog::error("[lua] failed to execute '{}': {}", path, last_error_);
            return false;
        }

        return true;
    }
    catch (const sol::error& e)
    {
        last_error_ = e.what();
        spdlog::error("[lua] exception executing '{}': {}", path, last_error_);
        return false;
    }
    catch (const std::exception& e)
    {
        last_error_ = e.what();
        spdlog::error("[lua] unexpected exception executing '{}': {}", path, last_error_);
        return false;
    }
}

bool lua_state::execute_string(const std::string& code)
{
    last_error_.clear();

    try
    {
        auto result{state_->safe_script(code, sol::script_pass_on_error)};

        if (!result.valid())
        {
            sol::error err{result};
            last_error_ = err.what();
            spdlog::error("[lua] failed to execute string: {}", last_error_);
            return false;
        }

        return true;
    }
    catch (const sol::error& e)
    {
        last_error_ = e.what();
        spdlog::error("[lua] exception executing string: {}", last_error_);
        return false;
    }
    catch (const std::exception& e)
    {
        last_error_ = e.what();
        spdlog::error("[lua] unexpected exception executing string: {}", last_error_);
        return false;
    }
}

sol::state& lua_state::native_handle() noexcept
{
    return *state_;
}

const sol::state& lua_state::native_handle() const noexcept
{
    return *state_;
}

const std::string& lua_state::last_error() const noexcept
{
    return last_error_;
}

} // namespace sdk::lua::detail
