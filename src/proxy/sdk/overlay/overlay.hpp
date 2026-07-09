/// @file overlay.hpp
/// @brief Overlay lifecycle (backend-agnostic public interface).

#pragma once

#include <cstdint>

namespace sdk::overlay
{

void init_opengl(std::uintptr_t native_device_context);
void init_direct3d8(std::uintptr_t native_device, std::uintptr_t native_window);
void render();
void invalidate_device_objects() noexcept;
void shutdown() noexcept;

} // namespace sdk::overlay
