#pragma once
#include <windows.h>

namespace sdk::overlay
{
void init(HDC dc) noexcept;
void render() noexcept;
void shutdown() noexcept;
} // namespace sdk::overlay
