/// @file detail/binding_api.hpp
/// @brief Internal adapter API used by `detail/register_bindings.cpp`
///        to register backend-agnostic binding schemas with the Lua state.
///
/// This header is **private** — it is included only from the single
/// registration translation unit.  It exposes `sol::table` so that
/// the adapter can call sol2 directly, but this never leaks into
/// `bindings/` or any public header.

#pragma once

#include <sol/sol.hpp>

#include <functional>
#include <string_view>

namespace sdk::lua::detail
{

/// Lightweight handle to a Lua table, used by binding schemas to
/// register functions without owning the table.
struct table_handle
{
    sol::table tbl;

    /// Add a function to this table.
    template <typename Fn>
    void add_function(std::string_view name, Fn&& fn)
    {
        tbl.set_function(std::string{name}, std::forward<Fn>(fn));
    }

    /// Create a nested table and return a handle to it.
    [[nodiscard]] table_handle create_table(std::string_view name)
    {
        return {tbl.create_named(std::string{name})};
    }
};

/// Obtain a handle to a named global table in the given state.
[[nodiscard]] inline table_handle get_table(sol::state& s, std::string_view name)
{
    return {s.create_named_table(std::string{name})};
}

} // namespace sdk::lua::detail
