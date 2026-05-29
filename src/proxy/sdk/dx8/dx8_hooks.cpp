#include "dx8_hooks.hpp"

#include "sdk/core/context.hpp"

#include <array>
#include <cstring>
#include <memory>
#include <safetyhook.hpp>
#include <spdlog/spdlog.h>

namespace sdk::dx8
{

// ─── Vtable hook state ───────────────────────────────────────────────────────

namespace
{

// Original vtable pointer (before patching)
void** g_device_vtable = nullptr;

// Copy of the vtable we can safely modify
std::unique_ptr<void*[]> g_vtable_copy;

// Saved original function pointers
void* g_orig_present                = nullptr;
void* g_orig_begin_scene            = nullptr;
void* g_orig_end_scene              = nullptr;
void* g_orig_set_transform          = nullptr;
void* g_orig_draw_indexed_primitive = nullptr;
void* g_orig_reset                  = nullptr;

// The device we've hooked
IDirect3DDevice8* g_hooked_device = nullptr;

// ─── Type aliases for original function pointers ─────────────────────────

using present_fn = HRESULT(STDMETHODCALLTYPE*)(
    IDirect3DDevice8*, CONST RECT*, CONST RECT*, HWND, CONST RGNDATA*);

using begin_scene_fn = HRESULT(STDMETHODCALLTYPE*)(IDirect3DDevice8*);

using end_scene_fn = HRESULT(STDMETHODCALLTYPE*)(IDirect3DDevice8*);

using set_transform_fn = HRESULT(STDMETHODCALLTYPE*)(
    IDirect3DDevice8*, D3DTRANSFORMSTATETYPE, CONST D3DMATRIX*);

using draw_indexed_primitive_fn = HRESULT(STDMETHODCALLTYPE*)(
    IDirect3DDevice8*, D3DPRIMITIVETYPE, UINT, UINT, UINT, UINT);

using reset_fn = HRESULT(STDMETHODCALLTYPE*)(
    IDirect3DDevice8*, D3DPRESENT_PARAMETERS*);

// ─── Vtable helpers ──────────────────────────────────────────────────────

/// Replaces a vtable slot and returns the original pointer.
inline auto patch_vtable(void** vtable, uint32_t idx, void* detour) -> void*
{
    void* orig      = vtable[idx];
    vtable[idx]     = detour;
    return orig;
}

/// Returns a safe upper bound for the vtable entry count.
/// IDirect3DDevice8 has ~98 entries; 128 is a safe conservative cap.
[[nodiscard]] constexpr auto count_vtable_entries() noexcept -> uint32_t
{
    return 128;
}

} // namespace

// ─── Public API ──────────────────────────────────────────────────────────────

void install_device_hooks(IDirect3DDevice8* dev)
{
    if (g_hooked_device != nullptr)
    {
        spdlog::warn("[dx8] device already hooked, skipping");
        return;
    }

    g_device_vtable = *reinterpret_cast<void***>(dev);
    const auto entry_count = count_vtable_entries();

    // Copy the entire vtable so we can modify it safely
    g_vtable_copy = std::make_unique<void*[]>(entry_count);
    std::memcpy(g_vtable_copy.get(),
                g_device_vtable,
                entry_count * sizeof(void*));

    // Patch target vtable slots
    g_orig_present = patch_vtable(
        g_vtable_copy.get(),
        vtable_idx::k_present,
        reinterpret_cast<void*>(hk_present));

    g_orig_begin_scene = patch_vtable(
        g_vtable_copy.get(),
        vtable_idx::k_begin_scene,
        reinterpret_cast<void*>(hk_begin_scene));

    g_orig_end_scene = patch_vtable(
        g_vtable_copy.get(),
        vtable_idx::k_end_scene,
        reinterpret_cast<void*>(hk_end_scene));

    g_orig_set_transform = patch_vtable(
        g_vtable_copy.get(),
        vtable_idx::k_set_transform,
        reinterpret_cast<void*>(hk_set_transform));

    g_orig_draw_indexed_primitive = patch_vtable(
        g_vtable_copy.get(),
        vtable_idx::k_draw_indexed_primitive,
        reinterpret_cast<void*>(hk_draw_indexed_primitive));

    g_orig_reset = patch_vtable(
        g_vtable_copy.get(),
        vtable_idx::k_reset,
        reinterpret_cast<void*>(hk_reset));

    // Replace the device's vtable pointer with our patched copy
    *reinterpret_cast<void***>(dev) = g_vtable_copy.get();

    g_hooked_device = dev;
    dev->AddRef(); // Hold a reference

    spdlog::info("[dx8] device vtable hooked — {} entries, {} patched",
                 entry_count, 6);
}

void remove_device_hooks()
{
    if (g_hooked_device == nullptr) { return; }

    // Restore original vtable pointer
    *reinterpret_cast<void***>(g_hooked_device) = g_device_vtable;

    g_hooked_device->Release();
    g_hooked_device = nullptr;

    g_vtable_copy.reset();
    g_device_vtable              = nullptr;
    g_orig_present                = nullptr;
    g_orig_begin_scene            = nullptr;
    g_orig_end_scene              = nullptr;
    g_orig_set_transform          = nullptr;
    g_orig_draw_indexed_primitive = nullptr;
    g_orig_reset                  = nullptr;

    spdlog::info("[dx8] device hooks removed");
}

auto device_hooked() noexcept -> bool
{
    return g_hooked_device != nullptr;
}

// ─── Direct3DCreate8 hook ────────────────────────────────────────────────────

// Forward declaration for CreateDevice vtable hook on IDirect3D8
namespace
{
    using d3d8_create_device_fn = HRESULT(STDMETHODCALLTYPE*)(
        IDirect3D8*,
        UINT,
        D3DDEVTYPE,
        HWND,
        DWORD,
        D3DPRESENT_PARAMETERS*,
        IDirect3DDevice8**);

    // Vtable index for IDirect3D8::CreateDevice
    inline constexpr auto k_d3d8_create_device_idx = 15u;

    void**   g_d3d8_vtable     = nullptr;
    void*    g_orig_create_device = nullptr;
    std::unique_ptr<void*[]> g_d3d8_vtable_copy;

    HRESULT STDMETHODCALLTYPE hk_create_device(
        IDirect3D8*            d3d,
        UINT                   adapter,
        D3DDEVTYPE             device_type,
        HWND                   focus_window,
        DWORD                  behavior_flags,
        D3DPRESENT_PARAMETERS* params,
        IDirect3DDevice8**     out_device)
    {
        auto orig = reinterpret_cast<d3d8_create_device_fn>(g_orig_create_device);

        HRESULT hr = orig(d3d, adapter, device_type, focus_window,
                          behavior_flags, params, out_device);

        if (SUCCEEDED(hr) && (*out_device != nullptr))
        {
            spdlog::info("[dx8] device created — installing vtable hooks");
            install_device_hooks(*out_device);

            g_ctx.detected_api.store(render_api::directx,
                                     std::memory_order::release);
        }

        return hr;
    }
} // namespace

IDirect3D8* WINAPI hk_direct3d_create8(UINT sdk_version)
{
    using fn_t = d3d8_create_fn;

    auto orig = call_orig<fn_t>(g_ctx.hooks.d3d8_create);
    IDirect3D8* d3d = orig(sdk_version);

    if ((d3d != nullptr) && (g_d3d8_vtable == nullptr))
    {
        g_d3d8_vtable = *reinterpret_cast<void***>(d3d);
        constexpr auto entry_count = 32u; // IDirect3D8 has ~32 vtable entries

        g_d3d8_vtable_copy = std::make_unique<void*[]>(entry_count);
        std::memcpy(g_d3d8_vtable_copy.get(),
                    g_d3d8_vtable,
                    entry_count * sizeof(void*));

        g_orig_create_device = patch_vtable(
            g_d3d8_vtable_copy.get(),
            k_d3d8_create_device_idx,
            reinterpret_cast<void*>(hk_create_device));

        *reinterpret_cast<void***>(d3d) = g_d3d8_vtable_copy.get();

        spdlog::info("[dx8] IDirect3D8 vtable hooked — CreateDevice intercepted");
    }

    return d3d;
}

// ─── Hooked device methods ───────────────────────────────────────────────────

HRESULT STDMETHODCALLTYPE hk_present(
    IDirect3DDevice8* dev,
    CONST RECT*       src_rect,
    CONST RECT*       dst_rect,
    HWND              wnd_override,
    CONST RGNDATA*    dirty_region)
{
    // Frame boundary — invoke Lua on_frame callbacks
    g_ctx.cb.on_frame.invoke();

    auto orig = reinterpret_cast<present_fn>(g_orig_present);
    return orig(dev, src_rect, dst_rect, wnd_override, dirty_region);
}

HRESULT STDMETHODCALLTYPE hk_begin_scene(IDirect3DDevice8* dev)
{
    auto orig = reinterpret_cast<begin_scene_fn>(g_orig_begin_scene);
    return orig(dev);
}

HRESULT STDMETHODCALLTYPE hk_end_scene(IDirect3DDevice8* dev)
{
    // Pre-present callback opportunity — invoke overlay rendering here
    g_ctx.cb.on_overlay.invoke();

    auto orig = reinterpret_cast<end_scene_fn>(g_orig_end_scene);
    return orig(dev);
}

HRESULT STDMETHODCALLTYPE hk_set_transform(
    IDirect3DDevice8*       dev,
    D3DTRANSFORMSTATETYPE   state,
    CONST D3DMATRIX*        matrix)
{
    // Track view/projection transforms for camera manipulation
    if (state == D3DTS_VIEW)
    {
        // View matrix set — equivalent to gluLookAt callback
        g_ctx.cb.on_glu_lookat.invoke();
    }

    auto orig = reinterpret_cast<set_transform_fn>(g_orig_set_transform);
    return orig(dev, state, matrix);
}

HRESULT STDMETHODCALLTYPE hk_draw_indexed_primitive(
    IDirect3DDevice8*   dev,
    D3DPRIMITIVETYPE    prim_type,
    UINT                min_vertex_index,
    UINT                num_vertices,
    UINT                start_index,
    UINT                prim_count)
{
    auto orig = reinterpret_cast<draw_indexed_primitive_fn>(
        g_orig_draw_indexed_primitive);
    return orig(dev, prim_type, min_vertex_index,
                num_vertices, start_index, prim_count);
}

HRESULT STDMETHODCALLTYPE hk_reset(
    IDirect3DDevice8*       dev,
    D3DPRESENT_PARAMETERS*  params)
{
    spdlog::info("[dx8] device reset — removing hooks temporarily");
    remove_device_hooks();

    auto orig = reinterpret_cast<reset_fn>(g_orig_reset);
    HRESULT hr = orig(dev, params);

    if (SUCCEEDED(hr))
    {
        spdlog::info("[dx8] device reset complete — re-installing hooks");
        install_device_hooks(dev);
    }

    return hr;
}

} // namespace sdk::dx8
