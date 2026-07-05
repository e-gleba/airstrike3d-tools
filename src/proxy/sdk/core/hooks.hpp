/// @file hooks.hpp
/// @brief Hook installation and teardown entry points.

#pragma once

namespace sdk
{

/// @throws std::runtime_error if a required export cannot be resolved or hooked.
void install_hooks();

void uninstall_hooks();

} // namespace sdk
