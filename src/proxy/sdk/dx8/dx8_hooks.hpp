#pragma once
#include <d3d8.h>
#include <windows.h>

#include <cstdint>

namespace sdk::dx8
{

// ─── Direct3DCreate8 hook ────────────────────────────────────────────────────

using d3d8_create_fn = IDirect3D8* (WINAPI*)(UINT);

IDirect3D8* WINAPI hk_direct3d_create8(UINT sdk_version);

// ─── IDirect3DDevice8 vtable hooks ───────────────────────────────────────────

// Vtable indices for IDirect3DDevice8
namespace vtable_idx
{
    inline constexpr auto k_reset                   = 14u;
    inline constexpr auto k_present                 = 15u;
    inline constexpr auto k_begin_scene             = 33u;
    inline constexpr auto k_end_scene               = 34u;
    inline constexpr auto k_set_transform           = 36u;
    inline constexpr auto k_draw_indexed_primitive  = 73u;
} // namespace vtable_idx

// ─── Hooked device methods ───────────────────────────────────────────────────

HRESULT STDMETHODCALLTYPE hk_present(
    IDirect3DDevice8* dev,
    CONST RECT*       src_rect,
    CONST RECT*       dst_rect,
    HWND              wnd_override,
    CONST RGNDATA*    dirty_region);

HRESULT STDMETHODCALLTYPE hk_begin_scene(IDirect3DDevice8* dev);

HRESULT STDMETHODCALLTYPE hk_end_scene(IDirect3DDevice8* dev);

HRESULT STDMETHODCALLTYPE hk_set_transform(
    IDirect3DDevice8*       dev,
    D3DTRANSFORMSTATETYPE   state,
    CONST D3DMATRIX*        matrix);

HRESULT STDMETHODCALLTYPE hk_draw_indexed_primitive(
    IDirect3DDevice8*   dev,
    D3DPRIMITIVETYPE    prim_type,
    UINT                min_vertex_index,
    UINT                num_vertices,
    UINT                start_index,
    UINT                prim_count);

HRESULT STDMETHODCALLTYPE hk_reset(
    IDirect3DDevice8*       dev,
    D3DPRESENT_PARAMETERS*  params);

// ─── Vtable management ──────────────────────────────────────────────────────

/// Installs vtable hooks on a newly created IDirect3DDevice8.
/// Saves original function pointers and replaces target vtable slots.
void install_device_hooks(IDirect3DDevice8* dev);

/// Restores the original vtable and releases saved state.
void remove_device_hooks();

/// Returns true if device hooks are currently active.
[[nodiscard]] auto device_hooked() noexcept -> bool;

} // namespace sdk::dx8
