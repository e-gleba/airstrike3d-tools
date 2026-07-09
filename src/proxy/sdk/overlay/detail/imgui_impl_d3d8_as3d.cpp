// Native Direct3D 8 renderer backend for Dear ImGui.
// Adapted from Dear ImGui's MIT-licensed imgui_impl_dx9 backend.
// Copyright (c) 2014-2026 Omar Cornut.

#include "sdk/overlay/detail/imgui_impl_d3d8_as3d.hpp"

#include <d3d8.h>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>

namespace sdk::overlay::detail
{
namespace
{

constexpr int   k_initial_vertex_capacity{ 5'000 };
constexpr int   k_initial_index_capacity{ 10'000 };
constexpr int   k_vertex_growth{ 5'000 };
constexpr int   k_index_growth{ 10'000 };
constexpr DWORD k_vertex_format{ D3DFVF_XYZ | D3DFVF_DIFFUSE | D3DFVF_TEX1 };

template <typename T> void release(T*& object) noexcept
{
    if (object != nullptr)
    {
        object->Release();
        object = nullptr;
    }
}

struct Vertex final
{
    float    position[3]{};
    D3DCOLOR color{};
    float    uv[2]{};
};

struct BackendData final
{
    explicit BackendData(IDirect3DDevice8* const d3d_device) noexcept
        : device{ d3d_device }
    {
        device->AddRef();
    }

    ~BackendData()
    {
        release(vertex_buffer);
        release(index_buffer);
        release(device);
    }

    BackendData(const BackendData&)            = delete;
    BackendData& operator=(const BackendData&) = delete;
    BackendData(BackendData&&)                 = delete;
    BackendData& operator=(BackendData&&)      = delete;

    IDirect3DDevice8*       device{};
    IDirect3DVertexBuffer8* vertex_buffer{};
    IDirect3DIndexBuffer8*  index_buffer{};
    int                     vertex_buffer_size{ k_initial_vertex_capacity };
    int                     index_buffer_size{ k_initial_index_capacity };
};

class StateBlock final
{
public:
    explicit StateBlock(IDirect3DDevice8& device) noexcept
        : device_{ device }
    {
        if (SUCCEEDED(device_.CreateStateBlock(D3DSBT_ALL, &token_)))
        {
            created_  = true;
            captured_ = SUCCEEDED(device_.CaptureStateBlock(token_));
        }
    }

    ~StateBlock()
    {
        if (created_)
        {
            if (captured_)
            {
                device_.ApplyStateBlock(token_);
            }
            device_.DeleteStateBlock(token_);
        }
    }

    StateBlock(const StateBlock&)            = delete;
    StateBlock& operator=(const StateBlock&) = delete;

    [[nodiscard]] bool valid() const noexcept { return captured_; }

private:
    IDirect3DDevice8& device_;
    DWORD             token_{};
    bool              created_{};
    bool              captured_{};
};

[[nodiscard]] BackendData* get_backend_data() noexcept
{
    if (ImGui::GetCurrentContext() == nullptr)
    {
        return nullptr;
    }
    return static_cast<BackendData*>(ImGui::GetIO().BackendRendererUserData);
}

[[nodiscard]] constexpr D3DCOLOR to_d3d_color(const ImU32 color) noexcept
{
#ifdef IMGUI_USE_BGRA_PACKED_COLOR
    return static_cast<D3DCOLOR>(color);
#else
    return (color & 0xFF00FF00U) | ((color & 0x00FF0000U) >> 16U) |
           ((color & 0x000000FFU) << 16U);
#endif
}

[[nodiscard]] D3DMATRIX identity_matrix() noexcept
{
    D3DMATRIX matrix{};
    matrix.m[0][0] = 1.0F;
    matrix.m[1][1] = 1.0F;
    matrix.m[2][2] = 1.0F;
    matrix.m[3][3] = 1.0F;
    return matrix;
}

[[nodiscard]] D3DMATRIX projection_matrix(const float left,
                                          const float right,
                                          const float top,
                                          const float bottom) noexcept
{
    D3DMATRIX matrix{};
    matrix.m[0][0] = 2.0F / (right - left);
    matrix.m[1][1] = 2.0F / (top - bottom);
    matrix.m[2][2] = 0.5F;
    matrix.m[3][0] = (left + right) / (left - right);
    matrix.m[3][1] = (top + bottom) / (bottom - top);
    matrix.m[3][2] = 0.5F;
    matrix.m[3][3] = 1.0F;
    return matrix;
}

void set_projection(IDirect3DDevice8&   device,
                    const ImVec2        display_position,
                    const D3DVIEWPORT8& viewport)
{
    const auto left{ display_position.x + static_cast<float>(viewport.X) +
                     0.5F };
    const auto right{ left + static_cast<float>(viewport.Width) };
    const auto top{ display_position.y + static_cast<float>(viewport.Y) +
                    0.5F };
    const auto bottom{ top + static_cast<float>(viewport.Height) };
    const auto projection{ projection_matrix(left, right, top, bottom) };
    device.SetTransform(D3DTS_PROJECTION, &projection);
}

void setup_render_state(ImDrawData& draw_data)
{
    auto* const backend{ get_backend_data() };
    IM_ASSERT(backend != nullptr);
    auto& device{ *backend->device };

    const D3DVIEWPORT8 viewport{
        .X      = 0,
        .Y      = 0,
        .Width  = static_cast<DWORD>(draw_data.DisplaySize.x),
        .Height = static_cast<DWORD>(draw_data.DisplaySize.y),
        .MinZ   = 0.0F,
        .MaxZ   = 1.0F,
    };
    device.SetViewport(&viewport);

    device.SetPixelShader(0);
    device.SetVertexShader(k_vertex_format);
    device.SetRenderState(D3DRS_FILLMODE, D3DFILL_SOLID);
    device.SetRenderState(D3DRS_SHADEMODE, D3DSHADE_GOURAUD);
    device.SetRenderState(D3DRS_ZWRITEENABLE, FALSE);
    device.SetRenderState(D3DRS_ALPHATESTENABLE, FALSE);
    device.SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE);
    device.SetRenderState(D3DRS_ZENABLE, FALSE);
    device.SetRenderState(D3DRS_ALPHABLENDENABLE, TRUE);
    device.SetRenderState(D3DRS_BLENDOP, D3DBLENDOP_ADD);
    device.SetRenderState(D3DRS_SRCBLEND, D3DBLEND_SRCALPHA);
    device.SetRenderState(D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA);
    device.SetRenderState(D3DRS_FOGENABLE, FALSE);
    device.SetRenderState(D3DRS_SPECULARENABLE, FALSE);
    device.SetRenderState(D3DRS_STENCILENABLE, FALSE);
    device.SetRenderState(D3DRS_CLIPPING, TRUE);
    device.SetRenderState(D3DRS_LIGHTING, FALSE);

    device.SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_MODULATE);
    device.SetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_TEXTURE);
    device.SetTextureStageState(0, D3DTSS_COLORARG2, D3DTA_DIFFUSE);
    device.SetTextureStageState(0, D3DTSS_ALPHAOP, D3DTOP_MODULATE);
    device.SetTextureStageState(0, D3DTSS_ALPHAARG1, D3DTA_TEXTURE);
    device.SetTextureStageState(0, D3DTSS_ALPHAARG2, D3DTA_DIFFUSE);
    device.SetTextureStageState(1, D3DTSS_COLOROP, D3DTOP_DISABLE);
    device.SetTextureStageState(1, D3DTSS_ALPHAOP, D3DTOP_DISABLE);
    device.SetTextureStageState(0, D3DTSS_MINFILTER, D3DTEXF_LINEAR);
    device.SetTextureStageState(0, D3DTSS_MAGFILTER, D3DTEXF_LINEAR);
    device.SetTextureStageState(0, D3DTSS_ADDRESSU, D3DTADDRESS_CLAMP);
    device.SetTextureStageState(0, D3DTSS_ADDRESSV, D3DTADDRESS_CLAMP);

    const auto identity{ identity_matrix() };
    device.SetTransform(D3DTS_WORLD, &identity);
    device.SetTransform(D3DTS_VIEW, &identity);
    set_projection(device, draw_data.DisplayPos, viewport);
}

void draw_callback_reset_render_state(const ImDrawList*, const ImDrawCmd*) {}

void draw_callback_set_sampler_linear(const ImDrawList*, const ImDrawCmd*)
{
    auto* const backend{ get_backend_data() };
    backend->device->SetTextureStageState(0, D3DTSS_MINFILTER, D3DTEXF_LINEAR);
    backend->device->SetTextureStageState(0, D3DTSS_MAGFILTER, D3DTEXF_LINEAR);
}

void draw_callback_set_sampler_nearest(const ImDrawList*, const ImDrawCmd*)
{
    auto* const backend{ get_backend_data() };
    backend->device->SetTextureStageState(0, D3DTSS_MINFILTER, D3DTEXF_POINT);
    backend->device->SetTextureStageState(0, D3DTSS_MAGFILTER, D3DTEXF_POINT);
}

[[nodiscard]] bool calculate_buffer_size(const int         required,
                                         const int         growth,
                                         const std::size_t element_size,
                                         int&              capacity,
                                         UINT&             bytes) noexcept
{
    if (required < 0)
    {
        return false;
    }

    const auto max_int{ std::numeric_limits<int>::max() };
    capacity = required <= max_int - growth ? required + growth : required;
    const auto byte_count{ static_cast<std::size_t>(capacity) * element_size };
    if (byte_count > std::numeric_limits<UINT>::max())
    {
        return false;
    }
    bytes = static_cast<UINT>(byte_count);
    return true;
}

[[nodiscard]] bool ensure_buffers(BackendData&      backend,
                                  const ImDrawData& draw_data)
{
    if (backend.vertex_buffer == nullptr ||
        backend.vertex_buffer_size < draw_data.TotalVtxCount)
    {
        int  capacity{};
        UINT bytes{};
        if (!calculate_buffer_size(draw_data.TotalVtxCount,
                                   k_vertex_growth,
                                   sizeof(Vertex),
                                   capacity,
                                   bytes))
        {
            return false;
        }
        release(backend.vertex_buffer);
        backend.vertex_buffer_size = capacity;
        if (FAILED(backend.device->CreateVertexBuffer(bytes,
                                                      D3DUSAGE_DYNAMIC |
                                                          D3DUSAGE_WRITEONLY,
                                                      k_vertex_format,
                                                      D3DPOOL_DEFAULT,
                                                      &backend.vertex_buffer)))
        {
            return false;
        }
    }

    if (backend.index_buffer == nullptr ||
        backend.index_buffer_size < draw_data.TotalIdxCount)
    {
        int  capacity{};
        UINT bytes{};
        if (!calculate_buffer_size(draw_data.TotalIdxCount,
                                   k_index_growth,
                                   sizeof(ImDrawIdx),
                                   capacity,
                                   bytes))
        {
            return false;
        }
        release(backend.index_buffer);
        backend.index_buffer_size = capacity;
        constexpr D3DFORMAT index_format{ sizeof(ImDrawIdx) ==
                                                  sizeof(std::uint16_t)
                                              ? D3DFMT_INDEX16
                                              : D3DFMT_INDEX32 };
        if (FAILED(backend.device->CreateIndexBuffer(bytes,
                                                     D3DUSAGE_DYNAMIC |
                                                         D3DUSAGE_WRITEONLY,
                                                     index_format,
                                                     D3DPOOL_DEFAULT,
                                                     &backend.index_buffer)))
        {
            return false;
        }
    }
    return true;
}

void copy_texture_region(const bool         use_colors,
                         const ImU32* const source,
                         const int          source_pitch,
                         ImU32* const       destination,
                         const int          destination_pitch,
                         const int          width,
                         const int          height)
{
    for (int y{}; y < height; ++y)
    {
        const auto* const source_row{ reinterpret_cast<const ImU32*>(
            reinterpret_cast<const std::byte*>(source) +
            static_cast<std::ptrdiff_t>(source_pitch) * y) };
        auto* const       destination_row{ reinterpret_cast<ImU32*>(
            reinterpret_cast<std::byte*>(destination) +
            static_cast<std::ptrdiff_t>(destination_pitch) * y) };
#ifdef IMGUI_USE_BGRA_PACKED_COLOR
        static_cast<void>(use_colors);
        std::memcpy(
            destination_row, source_row, static_cast<std::size_t>(width) * 4U);
#else
        if (use_colors)
        {
            for (int x{}; x < width; ++x)
            {
                destination_row[x] = to_d3d_color(source_row[x]);
            }
        }
        else
        {
            std::memcpy(destination_row,
                        source_row,
                        static_cast<std::size_t>(width) * 4U);
        }
#endif
    }
}

void update_texture(ImTextureData& texture_data)
{
    auto* const backend{ get_backend_data() };
    IM_ASSERT(backend != nullptr);

    if (texture_data.Status == ImTextureStatus_WantCreate)
    {
        IM_ASSERT(texture_data.TexID == ImTextureID_Invalid);
        IM_ASSERT(texture_data.BackendUserData == nullptr);
        IM_ASSERT(texture_data.Format == ImTextureFormat_RGBA32);
        if (texture_data.Format != ImTextureFormat_RGBA32 ||
            texture_data.Width <= 0 || texture_data.Height <= 0 ||
            texture_data.Width > std::numeric_limits<int>::max() / 4)
        {
            return;
        }

        IDirect3DTexture8* texture{};
        auto               result{ backend->device->CreateTexture(
            static_cast<UINT>(texture_data.Width),
            static_cast<UINT>(texture_data.Height),
            1,
            D3DUSAGE_DYNAMIC,
            D3DFMT_A8R8G8B8,
            D3DPOOL_DEFAULT,
            &texture) };
        if (FAILED(result))
        {
            result = backend->device->CreateTexture(
                static_cast<UINT>(texture_data.Width),
                static_cast<UINT>(texture_data.Height),
                1,
                0,
                D3DFMT_A8R8G8B8,
                D3DPOOL_MANAGED,
                &texture);
        }
        if (FAILED(result))
        {
            IM_ASSERT(false && "Direct3D 8 backend failed to create a texture");
            return;
        }

        D3DLOCKED_RECT locked{};
        if (FAILED(texture->LockRect(0, &locked, nullptr, 0)) ||
            locked.Pitch <= 0)
        {
            texture->Release();
            IM_ASSERT(false && "Direct3D 8 backend failed to lock a texture");
            return;
        }
        copy_texture_region(texture_data.UseColors,
                            static_cast<const ImU32*>(texture_data.GetPixels()),
                            texture_data.Width * 4,
                            static_cast<ImU32*>(locked.pBits),
                            locked.Pitch,
                            texture_data.Width,
                            texture_data.Height);
        texture->UnlockRect(0);

        texture_data.SetTexID(static_cast<ImTextureID>(
            reinterpret_cast<std::uintptr_t>(texture)));
        texture_data.SetStatus(ImTextureStatus_OK);
    }
    else if (texture_data.Status == ImTextureStatus_WantUpdates)
    {
        auto* const texture{ reinterpret_cast<IDirect3DTexture8*>(
            static_cast<std::uintptr_t>(texture_data.TexID)) };
        if (texture == nullptr ||
            texture_data.Width > std::numeric_limits<int>::max() / 4)
        {
            return;
        }

        const RECT update_rectangle{
            .left   = static_cast<LONG>(texture_data.UpdateRect.x),
            .top    = static_cast<LONG>(texture_data.UpdateRect.y),
            .right  = static_cast<LONG>(texture_data.UpdateRect.x +
                                       texture_data.UpdateRect.w),
            .bottom = static_cast<LONG>(texture_data.UpdateRect.y +
                                        texture_data.UpdateRect.h),
        };
        D3DLOCKED_RECT locked{};
        if (FAILED(texture->LockRect(0, &locked, &update_rectangle, 0)) ||
            locked.Pitch <= 0)
        {
            return;
        }
        for (const ImTextureRect& rectangle : texture_data.Updates)
        {
            auto* const destination{ static_cast<ImU32*>(locked.pBits) +
                                     (rectangle.x - texture_data.UpdateRect.x) +
                                     (rectangle.y - texture_data.UpdateRect.y) *
                                         (locked.Pitch / 4) };
            copy_texture_region(
                texture_data.UseColors,
                static_cast<const ImU32*>(
                    texture_data.GetPixelsAt(rectangle.x, rectangle.y)),
                texture_data.Width * 4,
                destination,
                locked.Pitch,
                rectangle.w,
                rectangle.h);
        }
        texture->UnlockRect(0);
        texture_data.SetStatus(ImTextureStatus_OK);
    }
    else if (texture_data.Status == ImTextureStatus_WantDestroy)
    {
        if (texture_data.TexID != ImTextureID_Invalid)
        {
            auto* const texture{ reinterpret_cast<IDirect3DTexture8*>(
                static_cast<std::uintptr_t>(texture_data.TexID)) };
            if (texture != nullptr)
            {
                texture->Release();
                texture_data.SetTexID(ImTextureID_Invalid);
            }
        }
        texture_data.SetStatus(ImTextureStatus_Destroyed);
    }
}

[[nodiscard]] bool valid_display_size(const ImVec2 display_size) noexcept
{
    const auto max_dimension{ static_cast<float>(
        std::numeric_limits<DWORD>::max()) };
    return std::isfinite(display_size.x) && std::isfinite(display_size.y) &&
           display_size.x > 0.0F && display_size.y > 0.0F &&
           display_size.x <= max_dimension && display_size.y <= max_dimension;
}

[[nodiscard]] bool set_clip_viewport(IDirect3DDevice8& device,
                                     const ImDrawData& draw_data,
                                     const ImDrawCmd&  command)
{
    const float clip_left{ command.ClipRect.x - draw_data.DisplayPos.x };
    const float clip_top{ command.ClipRect.y - draw_data.DisplayPos.y };
    const float clip_right{ command.ClipRect.z - draw_data.DisplayPos.x };
    const float clip_bottom{ command.ClipRect.w - draw_data.DisplayPos.y };
    if (!std::isfinite(clip_left) || !std::isfinite(clip_top) ||
        !std::isfinite(clip_right) || !std::isfinite(clip_bottom))
    {
        return false;
    }

    const float left{ std::floor(
        std::clamp(clip_left, 0.0F, draw_data.DisplaySize.x)) };
    const float top{ std::floor(
        std::clamp(clip_top, 0.0F, draw_data.DisplaySize.y)) };
    const float right{ std::ceil(
        std::clamp(clip_right, 0.0F, draw_data.DisplaySize.x)) };
    const float bottom{ std::ceil(
        std::clamp(clip_bottom, 0.0F, draw_data.DisplaySize.y)) };
    if (right <= left || bottom <= top)
    {
        return false;
    }

    const D3DVIEWPORT8 viewport{
        .X      = static_cast<DWORD>(left),
        .Y      = static_cast<DWORD>(top),
        .Width  = static_cast<DWORD>(right - left),
        .Height = static_cast<DWORD>(bottom - top),
        .MinZ   = 0.0F,
        .MaxZ   = 1.0F,
    };
    if (FAILED(device.SetViewport(&viewport)))
    {
        return false;
    }
    set_projection(device, draw_data.DisplayPos, viewport);
    return true;
}

} // namespace

bool d3d8_init(IDirect3DDevice8* const device)
{
    IMGUI_CHECKVERSION();
    ImGuiIO& io{ ImGui::GetIO() };
    IM_ASSERT(io.BackendRendererUserData == nullptr &&
              "Renderer backend already initialized");
    if (device == nullptr || io.BackendRendererUserData != nullptr)
    {
        return false;
    }

    auto* const backend{ IM_NEW(BackendData)(device) };
    io.BackendRendererUserData = backend;
    io.BackendRendererName     = "imgui_impl_d3d8_as3d";
    io.BackendFlags |= ImGuiBackendFlags_RendererHasVtxOffset;
    io.BackendFlags |= ImGuiBackendFlags_RendererHasTextures;

    ImGuiPlatformIO& platform_io{ ImGui::GetPlatformIO() };
    D3DCAPS8         capabilities{};
    if (SUCCEEDED(device->GetDeviceCaps(&capabilities)))
    {
        const auto max_int{ static_cast<DWORD>(
            std::numeric_limits<int>::max()) };
        platform_io.Renderer_TextureMaxWidth =
            static_cast<int>(std::min(capabilities.MaxTextureWidth, max_int));
        platform_io.Renderer_TextureMaxHeight =
            static_cast<int>(std::min(capabilities.MaxTextureHeight, max_int));
    }
    else
    {
        platform_io.Renderer_TextureMaxWidth  = 2'048;
        platform_io.Renderer_TextureMaxHeight = 2'048;
    }
    platform_io.DrawCallback_ResetRenderState =
        draw_callback_reset_render_state;
    platform_io.DrawCallback_SetSamplerLinear =
        draw_callback_set_sampler_linear;
    platform_io.DrawCallback_SetSamplerNearest =
        draw_callback_set_sampler_nearest;
    return true;
}

void d3d8_shutdown()
{
    auto* const backend{ get_backend_data() };
    IM_ASSERT(backend != nullptr && "Renderer backend is not initialized");
    if (backend == nullptr)
    {
        return;
    }

    d3d8_invalidate_device_objects();
    ImGuiIO& io{ ImGui::GetIO() };
    io.BackendRendererName     = nullptr;
    io.BackendRendererUserData = nullptr;
    io.BackendFlags &= ~(ImGuiBackendFlags_RendererHasVtxOffset |
                         ImGuiBackendFlags_RendererHasTextures);
    ImGui::GetPlatformIO().ClearRendererHandlers();
    IM_DELETE(backend);
}

void d3d8_new_frame()
{
    auto* const backend{ get_backend_data() };
    IM_ASSERT(backend != nullptr && "Renderer backend is not initialized");
    static_cast<void>(backend);
}

void d3d8_render_draw_data(ImDrawData* const draw_data)
{
    auto* const backend{ get_backend_data() };
    if (backend == nullptr || draw_data == nullptr ||
        !valid_display_size(draw_data->DisplaySize))
    {
        return;
    }

    if (draw_data->Textures != nullptr)
    {
        for (ImTextureData* const texture_data : *draw_data->Textures)
        {
            if (texture_data != nullptr &&
                texture_data->Status != ImTextureStatus_OK)
            {
                update_texture(*texture_data);
            }
        }
    }
    if (draw_data->TotalVtxCount <= 0 || draw_data->TotalIdxCount <= 0 ||
        !ensure_buffers(*backend, *draw_data))
    {
        return;
    }

    auto&      device{ *backend->device };
    StateBlock state_block{ device };
    if (!state_block.valid())
    {
        return;
    }

    D3DMATRIX previous_world{};
    D3DMATRIX previous_view{};
    D3DMATRIX previous_projection{};
    if (FAILED(device.GetTransform(D3DTS_WORLD, &previous_world)) ||
        FAILED(device.GetTransform(D3DTS_VIEW, &previous_view)) ||
        FAILED(device.GetTransform(D3DTS_PROJECTION, &previous_projection)))
    {
        return;
    }

    Vertex*    vertices{};
    ImDrawIdx* indices{};
    const auto vertex_bytes{ static_cast<UINT>(
        static_cast<std::size_t>(draw_data->TotalVtxCount) * sizeof(Vertex)) };
    const auto index_bytes{ static_cast<UINT>(
        static_cast<std::size_t>(draw_data->TotalIdxCount) *
        sizeof(ImDrawIdx)) };
    if (FAILED(backend->vertex_buffer->Lock(0,
                                            vertex_bytes,
                                            reinterpret_cast<BYTE**>(&vertices),
                                            D3DLOCK_DISCARD)))
    {
        return;
    }
    if (FAILED(backend->index_buffer->Lock(0,
                                           index_bytes,
                                           reinterpret_cast<BYTE**>(&indices),
                                           D3DLOCK_DISCARD)))
    {
        backend->vertex_buffer->Unlock();
        return;
    }

    for (const ImDrawList* const draw_list : draw_data->CmdLists)
    {
        for (const ImDrawVert& source : draw_list->VtxBuffer)
        {
            vertices->position[0] = source.pos.x;
            vertices->position[1] = source.pos.y;
            vertices->position[2] = 0.0F;
            vertices->color       = to_d3d_color(source.col);
            vertices->uv[0]       = source.uv.x;
            vertices->uv[1]       = source.uv.y;
            ++vertices;
        }
        const auto bytes{ static_cast<std::size_t>(draw_list->IdxBuffer.Size) *
                          sizeof(ImDrawIdx) };
        std::memcpy(indices, draw_list->IdxBuffer.Data, bytes);
        indices += draw_list->IdxBuffer.Size;
    }
    backend->vertex_buffer->Unlock();
    backend->index_buffer->Unlock();

    device.SetStreamSource(
        0, backend->vertex_buffer, static_cast<UINT>(sizeof(Vertex)));
    setup_render_state(*draw_data);

    int global_vertex_offset{};
    int global_index_offset{};
    for (const ImDrawList* const draw_list : draw_data->CmdLists)
    {
        for (const ImDrawCmd& command : draw_list->CmdBuffer)
        {
            if (command.UserCallback != nullptr)
            {
                if (command.UserCallback == draw_callback_reset_render_state)
                {
                    setup_render_state(*draw_data);
                }
                else
                {
                    command.UserCallback(draw_list, &command);
                }
                continue;
            }
            if (!set_clip_viewport(device, *draw_data, command))
            {
                continue;
            }

            auto* const texture{ reinterpret_cast<IDirect3DBaseTexture8*>(
                static_cast<std::uintptr_t>(command.GetTexID())) };
            device.SetTexture(0, texture);

            const auto base_vertex{ static_cast<UINT>(
                global_vertex_offset + static_cast<int>(command.VtxOffset)) };
            const auto start_index{ static_cast<UINT>(
                global_index_offset + static_cast<int>(command.IdxOffset)) };
            const auto vertex_count{ static_cast<UINT>(
                draw_list->VtxBuffer.Size -
                static_cast<int>(command.VtxOffset)) };
            device.SetIndices(backend->index_buffer, base_vertex);
            device.DrawIndexedPrimitive(D3DPT_TRIANGLELIST,
                                        0,
                                        vertex_count,
                                        start_index,
                                        command.ElemCount / 3U);
        }
        global_index_offset += draw_list->IdxBuffer.Size;
        global_vertex_offset += draw_list->VtxBuffer.Size;
    }

    device.SetTransform(D3DTS_WORLD, &previous_world);
    device.SetTransform(D3DTS_VIEW, &previous_view);
    device.SetTransform(D3DTS_PROJECTION, &previous_projection);
}

bool d3d8_create_device_objects()
{
    const auto* const backend{ get_backend_data() };
    return backend != nullptr && backend->device != nullptr;
}

void d3d8_invalidate_device_objects()
{
    auto* const backend{ get_backend_data() };
    if (backend == nullptr || backend->device == nullptr)
    {
        return;
    }

    for (ImTextureData* const texture_data : ImGui::GetPlatformIO().Textures)
    {
        if (texture_data != nullptr && texture_data->RefCount == 1)
        {
            texture_data->SetStatus(ImTextureStatus_WantDestroy);
            update_texture(*texture_data);
        }
    }
    release(backend->vertex_buffer);
    release(backend->index_buffer);
}

} // namespace sdk::overlay::detail
