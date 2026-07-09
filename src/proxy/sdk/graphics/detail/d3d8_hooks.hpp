/// @file d3d8_hooks.hpp
/// @brief Direct3D 8 hook lifecycle (internal).

#pragma once

namespace sdk::d3d8
{

[[nodiscard]] bool install_hooks();
void               uninstall_hooks() noexcept;

} // namespace sdk::d3d8
