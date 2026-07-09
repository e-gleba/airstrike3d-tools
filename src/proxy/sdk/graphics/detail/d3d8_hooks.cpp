#include "sdk/graphics/detail/d3d8_hooks.hpp"

#include "sdk/core/context.hpp"
#include "sdk/core/logging.hpp"
#include "sdk/graphics/detail/camera_math.hpp"
#include "sdk/graphics/rendering.hpp"
#include "sdk/overlay/overlay.hpp"

#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <d3d8.h>
#include <exception>
#include <format>
#include <mutex>
#include <ranges>
#include <safetyhook.hpp>
#include <string_view>
#include <utility>
#include <vector>
#include <windows.h>

namespace sdk::d3d8
{

namespace
{

constexpr std::size_t k_create_device_slot             = 15;
constexpr std::size_t k_reset_slot                     = 14;
constexpr std::size_t k_present_slot                   = 15;
constexpr std::size_t k_begin_scene_slot               = 34;
constexpr std::size_t k_end_scene_slot                 = 35;
constexpr std::size_t k_set_transform_slot             = 37;
constexpr std::size_t k_draw_primitive_slot            = 70;
constexpr std::size_t k_draw_indexed_primitive_slot    = 71;
constexpr std::size_t k_draw_primitive_up_slot         = 72;
constexpr std::size_t k_draw_indexed_primitive_up_slot = 73;

using direct3d_create8_fn = IDirect3D8*(WINAPI*)(UINT);
using create_device_fn    = HRESULT(STDMETHODCALLTYPE*)(IDirect3D8*,
                                                     UINT,
                                                     D3DDEVTYPE,
                                                     HWND,
                                                     DWORD,
                                                     D3DPRESENT_PARAMETERS*,
                                                     IDirect3DDevice8**);
using reset_fn            = HRESULT(STDMETHODCALLTYPE*)(IDirect3DDevice8*,
                                             D3DPRESENT_PARAMETERS*);
using present_fn          = HRESULT(STDMETHODCALLTYPE*)(
    IDirect3DDevice8*, const RECT*, const RECT*, HWND, const RGNDATA*);
using scene_fn          = HRESULT(STDMETHODCALLTYPE*)(IDirect3DDevice8*);
using set_transform_fn  = HRESULT(STDMETHODCALLTYPE*)(IDirect3DDevice8*,
                                                     D3DTRANSFORMSTATETYPE,
                                                     const D3DMATRIX*);
using draw_primitive_fn = HRESULT(STDMETHODCALLTYPE*)(IDirect3DDevice8*,
                                                      D3DPRIMITIVETYPE,
                                                      UINT,
                                                      UINT);
using draw_indexed_primitive_fn = HRESULT(STDMETHODCALLTYPE*)(
    IDirect3DDevice8*, D3DPRIMITIVETYPE, UINT, UINT, UINT, UINT);
using draw_primitive_up_fn = HRESULT(STDMETHODCALLTYPE*)(
    IDirect3DDevice8*, D3DPRIMITIVETYPE, UINT, const void*, UINT);
using draw_indexed_primitive_up_fn =
    HRESULT(STDMETHODCALLTYPE*)(IDirect3DDevice8*,
                                D3DPRIMITIVETYPE,
                                UINT,
                                UINT,
                                UINT,
                                const void*,
                                D3DFORMAT,
                                const void*,
                                UINT);

struct hook_state final
{
    safetyhook::InlineHook         create8;
    safetyhook::InlineHook         create_device;
    safetyhook::InlineHook         reset;
    safetyhook::InlineHook         present;
    safetyhook::InlineHook         begin_scene;
    safetyhook::InlineHook         end_scene;
    safetyhook::InlineHook         set_transform;
    safetyhook::InlineHook         draw_primitive;
    safetyhook::InlineHook         draw_indexed_primitive;
    safetyhook::InlineHook         draw_primitive_up;
    safetyhook::InlineHook         draw_indexed_primitive_up;
    std::mutex                     mutex;
    std::mutex                     transform_mutex;
    std::atomic<IDirect3DDevice8*> device{};
    std::atomic<HWND>              window{};
    std::atomic<bool>              scene_active{};
    std::atomic<bool>              frame_callbacks_run{};
    D3DMATRIX                      main_view{};
    D3DMATRIX                      main_projection{};
    D3DVIEWPORT8                   main_viewport{};
    bool                           main_perspective_active{};
    bool                           has_main_scene{};
};

hook_state        g_hooks;
thread_local bool g_internal_render{};

template <typename fn_type>
[[nodiscard]] fn_type original(safetyhook::InlineHook& hook) noexcept
{
    return reinterpret_cast<fn_type>(hook.trampoline().address());
}

[[nodiscard]] void* vtable_entry(void* object, std::size_t slot) noexcept
{
    if (object == nullptr)
    {
        return nullptr;
    }
    return (*reinterpret_cast<void***>(object))[slot];
}

[[nodiscard]] bool install_inline(safetyhook::InlineHook& hook,
                                  void*                   target,
                                  void*                   detour,
                                  std::string_view        name)
{
    if (hook)
    {
        return true;
    }
    if (target == nullptr)
    {
        sdk::log_error(std::format("D3D8 hook target '{}' is null", name));
        return false;
    }

    hook = safetyhook::create_inline(target, detour);
    if (!hook)
    {
        sdk::log_error(std::format("failed to install D3D8 hook '{}'", name));
        return false;
    }
    return true;
}

template <typename function> void guarded(function&& fn) noexcept
{
    try
    {
        std::forward<function>(fn)();
    }
    catch (const std::exception& error)
    {
        sdk::log_error(std::format("D3D8 callback failed: {}", error.what()));
    }
    catch (...)
    {
        sdk::log_error("D3D8 callback failed: unknown exception");
    }
}

[[nodiscard]] D3DMATRIX camera_matrix() noexcept
{
    D3DMATRIX  result{};
    const auto matrix =
        graphics::detail::make_right_handed_view(graphics::get_camera_pose());
    std::memcpy(result.m, matrix.data(), sizeof(result.m));
    return result;
}

[[nodiscard]] bool is_right_handed_perspective(const D3DMATRIX& matrix) noexcept
{
    constexpr float k_epsilon = 0.01F;
    return std::isfinite(matrix._34) && std::isfinite(matrix._44) &&
           matrix._34 < -0.5F && std::abs(matrix._44) < k_epsilon;
}

[[nodiscard]] bool is_primary(IDirect3DDevice8* device) noexcept
{
    return device == g_hooks.device.load(std::memory_order::acquire);
}

class internal_render_scope final
{
public:
    internal_render_scope() noexcept
        : previous_{ std::exchange(g_internal_render, true) }
    {
    }
    ~internal_render_scope() { g_internal_render = previous_; }

    internal_render_scope(const internal_render_scope&)            = delete;
    internal_render_scope& operator=(const internal_render_scope&) = delete;

private:
    bool previous_;
};

class d3d_state_block final
{
public:
    explicit d3d_state_block(IDirect3DDevice8* device) noexcept
        : device_{ device }
    {
        valid_ = SUCCEEDED(device_->CreateStateBlock(D3DSBT_ALL, &token_));
    }

    ~d3d_state_block()
    {
        if (valid_)
        {
            static_cast<void>(device_->ApplyStateBlock(token_));
            static_cast<void>(device_->DeleteStateBlock(token_));
        }
    }

    [[nodiscard]] bool valid() const noexcept { return valid_; }

private:
    IDirect3DDevice8* device_;
    DWORD             token_{};
    bool              valid_{};
};

void draw_world_lines(IDirect3DDevice8* device)
{
    static_assert(sizeof(graphics::line_vertex) == 16);

    const auto batch = graphics::detail::world_lines_snapshot();
    if (batch->vertices.size() < 2)
    {
        return;
    }

    D3DMATRIX    view{};
    D3DMATRIX    projection{};
    D3DVIEWPORT8 viewport{};
    {
        std::lock_guard lock{ g_hooks.transform_mutex };
        if (!g_hooks.has_main_scene)
        {
            return;
        }
        view = graphics::camera_enabled() ? camera_matrix() : g_hooks.main_view;
        projection = g_hooks.main_projection;
        viewport   = g_hooks.main_viewport;
    }

    d3d_state_block state{ device };
    if (!state.valid())
    {
        return;
    }

    D3DMATRIX identity{};
    identity._11 = 1.0F;
    identity._22 = 1.0F;
    identity._33 = 1.0F;
    identity._44 = 1.0F;
    if (FAILED(device->SetTransform(D3DTS_WORLD, &identity)) ||
        FAILED(device->SetTransform(D3DTS_VIEW, &view)) ||
        FAILED(device->SetTransform(D3DTS_PROJECTION, &projection)) ||
        FAILED(device->SetViewport(&viewport)))
    {
        return;
    }

    static_cast<void>(device->SetPixelShader(0));
    static_cast<void>(device->SetVertexShader(D3DFVF_XYZ | D3DFVF_DIFFUSE));
    static_cast<void>(device->SetTexture(0, nullptr));
    static_cast<void>(device->SetRenderState(D3DRS_LIGHTING, FALSE));
    static_cast<void>(device->SetRenderState(D3DRS_FOGENABLE, FALSE));
    static_cast<void>(device->SetRenderState(D3DRS_ALPHATESTENABLE, FALSE));
    static_cast<void>(device->SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE));
    static_cast<void>(device->SetRenderState(
        D3DRS_ZENABLE, batch->settings.depth_test ? TRUE : FALSE));
    static_cast<void>(device->SetRenderState(D3DRS_ZWRITEENABLE, FALSE));
    static_cast<void>(device->SetRenderState(D3DRS_ALPHABLENDENABLE, TRUE));
    static_cast<void>(
        device->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_SRCALPHA));
    static_cast<void>(
        device->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA));
    static_cast<void>(
        device->SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_SELECTARG1));
    static_cast<void>(
        device->SetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_DIFFUSE));
    static_cast<void>(
        device->SetTextureStageState(0, D3DTSS_ALPHAOP, D3DTOP_SELECTARG1));
    static_cast<void>(
        device->SetTextureStageState(0, D3DTSS_ALPHAARG1, D3DTA_DIFFUSE));
    static_cast<void>(
        device->SetTextureStageState(1, D3DTSS_COLOROP, D3DTOP_DISABLE));
    static_cast<void>(
        device->SetTextureStageState(1, D3DTSS_ALPHAOP, D3DTOP_DISABLE));
    static_cast<void>(
        device->DrawPrimitiveUP(D3DPT_LINELIST,
                                static_cast<UINT>(batch->vertices.size() / 2),
                                batch->vertices.data(),
                                sizeof(graphics::line_vertex)));
}

template <typename draw_call>
[[nodiscard]] HRESULT with_visual_override(IDirect3DDevice8* device,
                                           draw_call&&       call) noexcept
{
    if (g_internal_render)
    {
        return std::forward<draw_call>(call)();
    }

    const auto settings = graphics::get_visual_settings();
    if (settings.mode == graphics::visual_mode::disabled)
    {
        return std::forward<draw_call>(call)();
    }

    struct saved_state final
    {
        D3DRENDERSTATETYPE type{};
        DWORD              value{};
    };
    struct saved_stage_state final
    {
        D3DTEXTURESTAGESTATETYPE type{};
        DWORD                    value{};
    };

    internal_render_scope            scope;
    std::array<saved_state, 7>       saved{};
    std::size_t                      saved_count{};
    std::array<saved_stage_state, 4> saved_stage{};
    std::size_t                      saved_stage_count{};
    const auto set_temporary = [&](D3DRENDERSTATETYPE type, DWORD value)
    {
        DWORD original_value{};
        if (SUCCEEDED(device->GetRenderState(type, &original_value)))
        {
            saved[saved_count++] = { type, original_value };
            static_cast<void>(device->SetRenderState(type, value));
        }
    };
    const auto set_temporary_stage =
        [&](D3DTEXTURESTAGESTATETYPE type, DWORD value)
    {
        DWORD original_value{};
        if (SUCCEEDED(device->GetTextureStageState(0, type, &original_value)))
        {
            saved_stage[saved_stage_count++] = { type, original_value };
            static_cast<void>(device->SetTextureStageState(0, type, value));
        }
    };
    const auto set_temporary_color = [&](DWORD factor)
    {
        set_temporary(D3DRS_TEXTUREFACTOR, factor);
        set_temporary_stage(D3DTSS_COLOROP, D3DTOP_SELECTARG1);
        set_temporary_stage(D3DTSS_COLORARG1, D3DTA_TFACTOR);
        set_temporary_stage(D3DTSS_ALPHAOP, D3DTOP_SELECTARG1);
        set_temporary_stage(D3DTSS_ALPHAARG1, D3DTA_TFACTOR);
    };

    switch (settings.mode)
    {
        case graphics::visual_mode::xray:
            set_temporary(D3DRS_ZENABLE, FALSE);
            set_temporary(D3DRS_ZWRITEENABLE, FALSE);
            break;
        case graphics::visual_mode::wireframe:
            set_temporary(D3DRS_FILLMODE, D3DFILL_WIREFRAME);
            set_temporary_color(0xFF000000U | (settings.argb & 0x00FFFFFFU));
            break;
        case graphics::visual_mode::ghost:
            set_temporary(D3DRS_ZENABLE, FALSE);
            set_temporary(D3DRS_ZWRITEENABLE, FALSE);
            set_temporary(D3DRS_FILLMODE, D3DFILL_WIREFRAME);
            set_temporary(D3DRS_ALPHABLENDENABLE, TRUE);
            set_temporary(D3DRS_SRCBLEND, D3DBLEND_SRCALPHA);
            set_temporary(D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA);
            set_temporary_color(
                (static_cast<DWORD>(std::lround(settings.alpha * 255.0F))
                 << 24U) |
                (settings.argb & 0x00FFFFFFU));
            break;
        case graphics::visual_mode::depth_bias:
        {
            const auto bias = static_cast<DWORD>(std::clamp(
                std::lround(std::abs(settings.depth_bias) * 16.0F), 0L, 16L));
            set_temporary(D3DRS_ZBIAS, bias);
            break;
        }
        case graphics::visual_mode::disabled:
            break;
    }

    const auto result = std::forward<draw_call>(call)();
    while (saved_stage_count > 0)
    {
        --saved_stage_count;
        static_cast<void>(
            device->SetTextureStageState(0,
                                         saved_stage[saved_stage_count].type,
                                         saved_stage[saved_stage_count].value));
    }
    while (saved_count > 0)
    {
        --saved_count;
        static_cast<void>(device->SetRenderState(saved[saved_count].type,
                                                 saved[saved_count].value));
    }
    return result;
}

HRESULT STDMETHODCALLTYPE hk_reset(IDirect3DDevice8*      device,
                                   D3DPRESENT_PARAMETERS* parameters)
{
    if (device == g_hooks.device.load(std::memory_order::acquire))
    {
        guarded([] { overlay::invalidate_device_objects(); });
    }
    return original<reset_fn>(g_hooks.reset)(device, parameters);
}

HRESULT STDMETHODCALLTYPE hk_begin_scene(IDirect3DDevice8* device)
{
    const auto result = original<scene_fn>(g_hooks.begin_scene)(device);
    if (SUCCEEDED(result) && !g_internal_render && is_primary(device))
    {
        guarded(
            [device]
            {
                g_hooks.scene_active.store(true, std::memory_order_release);
                overlay::init_direct3d8(
                    reinterpret_cast<std::uintptr_t>(device),
                    reinterpret_cast<std::uintptr_t>(
                        g_hooks.window.load(std::memory_order_acquire)));
                if (!g_hooks.frame_callbacks_run.exchange(
                        true, std::memory_order_acq_rel))
                {
                    g_ctx.cb.on_frame.invoke();
                }
            });
    }
    return result;
}

HRESULT STDMETHODCALLTYPE hk_end_scene(IDirect3DDevice8* device)
{
    if (!g_internal_render && is_primary(device))
    {
        guarded(
            [device]
            {
                if (device == g_hooks.device.load(std::memory_order::acquire) &&
                    g_ctx.imgui_initialized.load(std::memory_order::acquire))
                {
                    internal_render_scope scope;
                    draw_world_lines(device);
                    if (g_ctx.show_ui.load(std::memory_order::relaxed))
                    {
                        overlay::render();
                    }
                }
            });
        g_hooks.scene_active.store(false, std::memory_order_release);
    }
    return original<scene_fn>(g_hooks.end_scene)(device);
}

HRESULT STDMETHODCALLTYPE hk_present(IDirect3DDevice8* device,
                                     const RECT*       source,
                                     const RECT*       destination,
                                     HWND              override_window,
                                     const RGNDATA*    dirty_region)
{
    const auto result = original<present_fn>(g_hooks.present)(
        device, source, destination, override_window, dirty_region);
    if (is_primary(device))
    {
        g_hooks.frame_callbacks_run.store(false, std::memory_order_release);
    }
    return result;
}

HRESULT STDMETHODCALLTYPE hk_set_transform(IDirect3DDevice8*     device,
                                           D3DTRANSFORMSTATETYPE state,
                                           const D3DMATRIX*      matrix)
{
    try
    {
        if (g_internal_render || !is_primary(device) || matrix == nullptr)
        {
            return original<set_transform_fn>(g_hooks.set_transform)(
                device, state, matrix);
        }

        if (state == D3DTS_PROJECTION)
        {
            D3DVIEWPORT8 viewport{};
            const auto   perspective = is_right_handed_perspective(*matrix);
            const auto   has_viewport =
                perspective && SUCCEEDED(device->GetViewport(&viewport));
            std::lock_guard lock{ g_hooks.transform_mutex };
            g_hooks.main_perspective_active = perspective;
            if (perspective)
            {
                g_hooks.main_projection = *matrix;
                if (has_viewport)
                {
                    g_hooks.main_viewport = viewport;
                }
            }
        }
        else if (state == D3DTS_VIEW)
        {
            bool main_perspective{};
            {
                std::lock_guard lock{ g_hooks.transform_mutex };
                main_perspective = g_hooks.main_perspective_active;
                if (main_perspective)
                {
                    g_hooks.main_view      = *matrix;
                    g_hooks.has_main_scene = true;
                }
            }

            if (main_perspective)
            {
                const auto matrix_values =
                    std::span<const float, 16>{ &matrix->m[0][0], 16 };
                if (const auto pose =
                        graphics::detail::decompose_right_handed_view(
                            matrix_values))
                {
                    graphics::detail::observe_camera(*pose);
                }

                if (graphics::camera_enabled())
                {
                    const auto replacement = camera_matrix();
                    return original<set_transform_fn>(g_hooks.set_transform)(
                        device, state, &replacement);
                }
            }
        }
    }
    catch (...)
    {
        sdk::log_error("Direct3D 8 camera override failed");
    }
    return original<set_transform_fn>(g_hooks.set_transform)(
        device, state, matrix);
}

HRESULT STDMETHODCALLTYPE hk_draw_primitive(IDirect3DDevice8* device,
                                            D3DPRIMITIVETYPE  type,
                                            UINT              start,
                                            UINT              count)
{
    if (!is_primary(device))
    {
        return original<draw_primitive_fn>(g_hooks.draw_primitive)(
            device, type, start, count);
    }
    return with_visual_override(device,
                                [&]
                                {
                                    return original<draw_primitive_fn>(
                                        g_hooks.draw_primitive)(
                                        device, type, start, count);
                                });
}

HRESULT STDMETHODCALLTYPE hk_draw_indexed_primitive(IDirect3DDevice8* device,
                                                    D3DPRIMITIVETYPE  type,
                                                    UINT              min_index,
                                                    UINT vertex_count,
                                                    UINT start_index,
                                                    UINT primitive_count)
{
    if (!is_primary(device))
    {
        return original<draw_indexed_primitive_fn>(
            g_hooks.draw_indexed_primitive)(device,
                                            type,
                                            min_index,
                                            vertex_count,
                                            start_index,
                                            primitive_count);
    }
    return with_visual_override(device,
                                [&]
                                {
                                    return original<draw_indexed_primitive_fn>(
                                        g_hooks.draw_indexed_primitive)(
                                        device,
                                        type,
                                        min_index,
                                        vertex_count,
                                        start_index,
                                        primitive_count);
                                });
}

HRESULT STDMETHODCALLTYPE hk_draw_primitive_up(IDirect3DDevice8* device,
                                               D3DPRIMITIVETYPE  type,
                                               UINT        primitive_count,
                                               const void* data,
                                               UINT        stride)
{
    if (!is_primary(device))
    {
        return original<draw_primitive_up_fn>(g_hooks.draw_primitive_up)(
            device, type, primitive_count, data, stride);
    }
    return with_visual_override(
        device,
        [&]
        {
            return original<draw_primitive_up_fn>(g_hooks.draw_primitive_up)(
                device, type, primitive_count, data, stride);
        });
}

HRESULT STDMETHODCALLTYPE hk_draw_indexed_primitive_up(IDirect3DDevice8* device,
                                                       D3DPRIMITIVETYPE  type,
                                                       UINT min_vertex,
                                                       UINT vertex_count,
                                                       UINT primitive_count,
                                                       const void* index_data,
                                                       D3DFORMAT   index_format,
                                                       const void* data,
                                                       UINT        stride)
{
    if (!is_primary(device))
    {
        return original<draw_indexed_primitive_up_fn>(
            g_hooks.draw_indexed_primitive_up)(device,
                                               type,
                                               min_vertex,
                                               vertex_count,
                                               primitive_count,
                                               index_data,
                                               index_format,
                                               data,
                                               stride);
    }
    return with_visual_override(
        device,
        [&]
        {
            return original<draw_indexed_primitive_up_fn>(
                g_hooks.draw_indexed_primitive_up)(device,
                                                   type,
                                                   min_vertex,
                                                   vertex_count,
                                                   primitive_count,
                                                   index_data,
                                                   index_format,
                                                   data,
                                                   stride);
        });
}

[[nodiscard]] bool install_device_hooks(IDirect3DDevice8* device)
{
    struct definition final
    {
        safetyhook::InlineHook* hook;
        std::size_t             slot;
        void*                   detour;
        std::string_view        name;
    };

    const auto definitions = std::array{
        definition{ &g_hooks.reset,
                    k_reset_slot,
                    reinterpret_cast<void*>(hk_reset),
                    "IDirect3DDevice8::Reset" },
        definition{ &g_hooks.present,
                    k_present_slot,
                    reinterpret_cast<void*>(hk_present),
                    "IDirect3DDevice8::Present" },
        definition{ &g_hooks.begin_scene,
                    k_begin_scene_slot,
                    reinterpret_cast<void*>(hk_begin_scene),
                    "IDirect3DDevice8::BeginScene" },
        definition{ &g_hooks.end_scene,
                    k_end_scene_slot,
                    reinterpret_cast<void*>(hk_end_scene),
                    "IDirect3DDevice8::EndScene" },
        definition{ &g_hooks.set_transform,
                    k_set_transform_slot,
                    reinterpret_cast<void*>(hk_set_transform),
                    "IDirect3DDevice8::SetTransform" },
        definition{ &g_hooks.draw_primitive,
                    k_draw_primitive_slot,
                    reinterpret_cast<void*>(hk_draw_primitive),
                    "IDirect3DDevice8::DrawPrimitive" },
        definition{ &g_hooks.draw_indexed_primitive,
                    k_draw_indexed_primitive_slot,
                    reinterpret_cast<void*>(hk_draw_indexed_primitive),
                    "IDirect3DDevice8::DrawIndexedPrimitive" },
        definition{ &g_hooks.draw_primitive_up,
                    k_draw_primitive_up_slot,
                    reinterpret_cast<void*>(hk_draw_primitive_up),
                    "IDirect3DDevice8::DrawPrimitiveUP" },
        definition{ &g_hooks.draw_indexed_primitive_up,
                    k_draw_indexed_primitive_up_slot,
                    reinterpret_cast<void*>(hk_draw_indexed_primitive_up),
                    "IDirect3DDevice8::DrawIndexedPrimitiveUP" },
    };

    return std::ranges::all_of(definitions,
                               [device](const auto& definition)
                               {
                                   return install_inline(
                                       *definition.hook,
                                       vtable_entry(device, definition.slot),
                                       definition.detour,
                                       definition.name);
                               });
}

HRESULT STDMETHODCALLTYPE hk_create_device(IDirect3D8* direct3d,
                                           UINT        adapter,
                                           D3DDEVTYPE  device_type,
                                           HWND        focus_window,
                                           DWORD       behavior_flags,
                                           D3DPRESENT_PARAMETERS* parameters,
                                           IDirect3DDevice8**     output)
{
    const auto result =
        original<create_device_fn>(g_hooks.create_device)(direct3d,
                                                          adapter,
                                                          device_type,
                                                          focus_window,
                                                          behavior_flags,
                                                          parameters,
                                                          output);
    if (FAILED(result) || output == nullptr || *output == nullptr)
    {
        return result;
    }

    bool claimed{};
    try
    {
        auto expected = render_api::unknown;
        claimed       = g_ctx.detected_api.compare_exchange_strong(
            expected, render_api::direct3d8, std::memory_order_acq_rel);
        if (!claimed && expected != render_api::direct3d8)
        {
            sdk::log_info(
                "Direct3D 8 device ignored because another renderer is active");
            return result;
        }

        std::lock_guard lock{ g_hooks.mutex };
        auto* const     current_device =
            g_hooks.device.load(std::memory_order_acquire);
        if (current_device != nullptr && current_device != *output)
        {
            sdk::log_warn("additional Direct3D 8 device ignored; primary "
                          "device retained");
            return result;
        }

        if (!install_device_hooks(*output))
        {
            sdk::log_error(
                "Direct3D 8 device created, but hooks are incomplete");
            if (claimed)
            {
                auto active = render_api::direct3d8;
                static_cast<void>(g_ctx.detected_api.compare_exchange_strong(
                    active, render_api::unknown, std::memory_order_acq_rel));
            }
            return result;
        }

        g_hooks.device.store(*output, std::memory_order_release);
        const auto window =
            parameters != nullptr && parameters->hDeviceWindow != nullptr
                ? parameters->hDeviceWindow
                : focus_window;
        g_hooks.window.store(window, std::memory_order_release);
        graphics::detail::set_active_backend(render_api::direct3d8);
        g_ctx.overlay_available.store(true, std::memory_order_release);
        sdk::log_info(
            "Direct3D 8 renderer confirmed; overlay and plugins active");
    }
    catch (const std::exception& error)
    {
        if (claimed)
        {
            g_ctx.detected_api.store(render_api::unknown,
                                     std::memory_order_release);
        }
        sdk::log_error(std::format("Direct3D 8 device hook setup failed: {}",
                                   error.what()));
    }
    catch (...)
    {
        if (claimed)
        {
            g_ctx.detected_api.store(render_api::unknown,
                                     std::memory_order_release);
        }
        sdk::log_error("Direct3D 8 device hook setup failed");
    }
    return result;
}

IDirect3D8* WINAPI hk_direct3d_create8(UINT sdk_version)
{
    auto* direct3d =
        original<direct3d_create8_fn>(g_hooks.create8)(sdk_version);
    if (direct3d == nullptr)
    {
        return nullptr;
    }

    try
    {
        std::lock_guard lock{ g_hooks.mutex };
        static_cast<void>(
            install_inline(g_hooks.create_device,
                           vtable_entry(direct3d, k_create_device_slot),
                           reinterpret_cast<void*>(hk_create_device),
                           "IDirect3D8::CreateDevice"));
    }
    catch (...)
    {
        sdk::log_error("Direct3D 8 CreateDevice hook setup failed");
    }
    return direct3d;
}

} // namespace

bool install_hooks()
{
    std::lock_guard lock{ g_hooks.mutex };
    if (g_hooks.create8)
    {
        return true;
    }

    const auto module = GetModuleHandleW(L"d3d8.dll");
    if (module == nullptr)
    {
        return false;
    }

    const auto target =
        reinterpret_cast<void*>(GetProcAddress(module, "Direct3DCreate8"));
    const auto installed =
        install_inline(g_hooks.create8,
                       target,
                       reinterpret_cast<void*>(hk_direct3d_create8),
                       "Direct3DCreate8");
    if (installed)
    {
        sdk::log_info("Direct3D 8 bootstrap hook installed");
    }
    return installed;
}

void uninstall_hooks() noexcept
{
    std::lock_guard lock{ g_hooks.mutex };
    g_hooks.draw_indexed_primitive_up.reset();
    g_hooks.draw_primitive_up.reset();
    g_hooks.draw_indexed_primitive.reset();
    g_hooks.draw_primitive.reset();
    g_hooks.set_transform.reset();
    g_hooks.end_scene.reset();
    g_hooks.begin_scene.reset();
    g_hooks.present.reset();
    g_hooks.reset.reset();
    g_hooks.create_device.reset();
    g_hooks.create8.reset();
    g_hooks.device.store(nullptr, std::memory_order_release);
    g_hooks.window.store(nullptr, std::memory_order_release);
    g_hooks.scene_active.store(false, std::memory_order_release);
    g_hooks.frame_callbacks_run.store(false, std::memory_order_release);
}

} // namespace sdk::d3d8
