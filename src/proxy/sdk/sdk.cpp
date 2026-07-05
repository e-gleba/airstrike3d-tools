// src/proxy/sdk/sdk.cpp
// Main SDK implementation.

#include "sdk.hpp"

#include "core/context.hpp"

namespace sdk
{

context& get_context() noexcept
{
    return g_ctx;
}

} // namespace sdk
