#pragma once
#include <d3d8.h>

struct ImDrawData;

/// Minimal DirectX 8 backend for ImGui.
/// Bundled directly in the SDK since ocornut/imgui does not ship a D3D8 backend.
/// Zero external dependencies beyond d3d8.h (header-only types).

bool ImGui_ImplDX8_Init(IDirect3DDevice8* device);
void ImGui_ImplDX8_Shutdown();
void ImGui_ImplDX8_NewFrame();
void ImGui_ImplDX8_RenderDrawData(ImDrawData* draw_data);
void ImGui_ImplDX8_InvalidateDeviceObjects();
