// Native Direct3D 8 renderer backend for Dear ImGui.
// Adapted from Dear ImGui's MIT-licensed imgui_impl_dx9 backend.
// Copyright (c) 2014-2026 Omar Cornut.

#pragma once

#include <imgui.h>

struct IDirect3DDevice8;

namespace sdk::overlay::detail
{

[[nodiscard]] bool d3d8_init(IDirect3DDevice8* device);
void               d3d8_shutdown();
void               d3d8_new_frame();
void               d3d8_render_draw_data(ImDrawData* draw_data);
[[nodiscard]] bool d3d8_create_device_objects();
void               d3d8_invalidate_device_objects();

} // namespace sdk::overlay::detail
