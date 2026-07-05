#pragma once
#include <windows.h>

namespace sdk::overlay
{
void init(HDC dc);
void render();
void shutdown();
} // namespace sdk::overlay
