/// @file overlay.hpp
/// @brief Overlay lifecycle (backend-agnostic public interface).

#pragma once

#include <cstdint>

namespace sdk::overlay
{

void init(std::uintptr_t native_device_context);
void render();
void shutdown() noexcept;

} // namespace sdk::overlay
