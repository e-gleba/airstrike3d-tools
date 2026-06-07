// sdk/render/directx8_renderer.cpp — DirectX8 renderer implementation
//
// ALL DirectX8 code isolated here. Public headers remain pure C++23.

#include "sdk/render/directx8_renderer.hpp"
#include "sdk/math/glm_adapter.hpp"

#include <d3d8.h>

#include <spdlog/spdlog.h>
#include <vector>
#include <stack>

namespace sdk::render
{

// ─── directx8_renderer::impl ─────────────────────────────────────────────

struct directx8_renderer::impl
{
    bool initialized = false;
    IDirect3DDevice8* device = nullptr;

    // State tracking
    int current_matrix_mode = gl::k_modelview;
    std::stack<D3DMATRIX> matrix_stack;

    // Convert sdk::math::mat4 to D3DMATRIX
    static auto to_d3d_matrix(math::mat4 const& m) -> D3DMATRIX
    {
        D3DMATRIX result;
        for (int i = 0; i < 16; ++i)
        {
            result.m[i / 4][i % 4] = static_cast<float>(m.m[i]);
        }
        return result;
    }

    // Apply current transformation matrix
    void apply_matrix(math::mat4 const& m)
    {
        if (!device)
        {
            return;
        }

        auto d3d_mat = to_d3d_matrix(m);
        switch (current_matrix_mode)
        {
            case gl::k_modelview:
                device->SetTransform(D3DTS_VIEW, &d3d_mat);
                break;
            case gl::k_projection:
                device->SetTransform(D3DTS_PROJECTION, &d3d_mat);
                break;
            case gl::k_texture:
                device->SetTransform(D3DTS_TEXTURE0, &d3d_mat);
                break;
        }
    }
};

// ─── directx8_renderer implementation ────────────────────────────────────

directx8_renderer::directx8_renderer() : pimpl_(std::make_unique<impl>()) {}

directx8_renderer::~directx8_renderer()
{
    if (pimpl_ && pimpl_->initialized)
    {
        shutdown();
    }
}

void directx8_renderer::init(device_context* ctx)
{
    if (pimpl_->initialized)
    {
        return;
    }

    pimpl_->device = static_cast<IDirect3DDevice8*>(ctx);
    if (!pimpl_->device)
    {
        spdlog::error("[directx8] null device context");
        return;
    }

    pimpl_->initialized = true;
    spdlog::info("[directx8] renderer initialized");
}

void directx8_renderer::shutdown()
{
    if (!pimpl_->initialized)
    {
        return;
    }

    // Device is owned by game, don't release here
    pimpl_->device = nullptr;
    pimpl_->initialized = false;
    spdlog::info("[directx8] renderer shutdown");
}

auto directx8_renderer::is_initialized() const noexcept -> bool
{
    return pimpl_->initialized;
}

void directx8_renderer::begin_frame() { /* No-op */ }
void directx8_renderer::end_frame() { /* No-op */ }

void directx8_renderer::enable(int state)
{
    if (!pimpl_->device) return;

    switch (state)
    {
        case gl::k_depth_test:
            pimpl_->device->SetRenderState(D3DRS_ZENABLE, TRUE);
            break;
        case gl::k_blend:
            pimpl_->device->SetRenderState(D3DRS_ALPHABLENDENABLE, TRUE);
            break;
        case gl::k_cull_face:
            pimpl_->device->SetRenderState(D3DRS_CULLMODE, D3DCULL_CCW);
            break;
    }
}

void directx8_renderer::disable(int state)
{
    if (!pimpl_->device) return;

    switch (state)
    {
        case gl::k_depth_test:
            pimpl_->device->SetRenderState(D3DRS_ZENABLE, FALSE);
            break;
        case gl::k_blend:
            pimpl_->device->SetRenderState(D3DRS_ALPHABLENDENABLE, FALSE);
            break;
        case gl::k_cull_face:
            pimpl_->device->SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE);
            break;
    }
}

void directx8_renderer::set_blend_func(int src_factor, int dst_factor)
{
    if (!pimpl_->device) return;

    // Map OpenGL blend factors to D3D
    auto map_factor = [](int gl_factor) -> D3DBLEND {
        switch (gl_factor)
        {
            case gl::k_zero: return D3DBLEND_ZERO;
            case gl::k_one: return D3DBLEND_ONE;
            case gl::k_src_alpha: return D3DBLEND_SRCALPHA;
            case gl::k_one_minus_src_alpha: return D3DBLEND_INVSRCALPHA;
            default: return D3DBLEND_ONE;
        }
    };

    pimpl_->device->SetRenderState(D3DRS_SRCBLEND, map_factor(src_factor));
    pimpl_->device->SetRenderState(D3DRS_DESTBLEND, map_factor(dst_factor));
}

void directx8_renderer::set_depth_mask(bool enabled)
{
    if (!pimpl_->device) return;
    pimpl_->device->SetRenderState(D3DRS_ZWRITEENABLE, enabled ? TRUE : FALSE);
}

void directx8_renderer::set_matrix_mode(int mode)
{
    pimpl_->current_matrix_mode = mode;
}

void directx8_renderer::load_identity()
{
    pimpl_->apply_matrix(math::mat4::identity());
}

void directx8_renderer::load_matrix(double const* matrix)
{
    math::mat4 m;
    for (int i = 0; i < 16; ++i)
    {
        m.m[i] = matrix[i];
    }
    pimpl_->apply_matrix(m);
}

void directx8_renderer::mult_matrix(double const* matrix)
{
    // Get current matrix, multiply, apply
    // Simplified: just load for now
    load_matrix(matrix);
}

void directx8_renderer::push_matrix()
{
    if (!pimpl_->device) return;

    D3DMATRIX current;
    D3DTRANSFORMSTATETYPE type = (pimpl_->current_matrix_mode == gl::k_modelview) ? D3DTS_VIEW : D3DTS_PROJECTION;
    pimpl_->device->GetTransform(type, &current);
    pimpl_->matrix_stack.push(current);
}

void directx8_renderer::pop_matrix()
{
    if (!pimpl_->device || pimpl_->matrix_stack.empty()) return;

    D3DMATRIX top = pimpl_->matrix_stack.top();
    pimpl_->matrix_stack.pop();

    D3DTRANSFORMSTATETYPE type = (pimpl_->current_matrix_mode == gl::k_modelview) ? D3DTS_VIEW : D3DTS_PROJECTION;
    pimpl_->device->SetTransform(type, &top);
}

void directx8_renderer::look_at(math::vec3 const& eye, math::vec3 const& center, math::vec3 const& up)
{
    auto view = math::look_at(eye, center, up);
    pimpl_->apply_matrix(view);
}

void directx8_renderer::begin(int /*primitive_type*/)
{
    // DirectX8 uses vertex buffers, not immediate mode
    // This is a simplification - real implementation would use DrawPrimitiveUP
    spdlog::warn("[directx8] begin() not fully implemented");
}

void directx8_renderer::end()
{
    // See begin()
}

void directx8_renderer::set_color(float r, float g, float b, float a)
{
    // DirectX8 uses vertex colors or material
    // Simplified: store for next vertex
    (void)r; (void)g; (void)b; (void)a;
}

void directx8_renderer::vertex(float x, float y, float z)
{
    // See begin()
    (void)x; (void)y; (void)z;
}

void directx8_renderer::vertex(math::vec3 const& v)
{
    vertex(static_cast<float>(v.x), static_cast<float>(v.y), static_cast<float>(v.z));
}

void directx8_renderer::set_line_width(float width)
{
    // DirectX8 doesn't support line width directly
    (void)width;
}

void directx8_renderer::set_point_size(float size)
{
    if (!pimpl_->device) return;
    pimpl_->device->SetRenderState(D3DRS_POINTSIZE, *reinterpret_cast<DWORD*>(&size));
}

void directx8_renderer::translate(double x, double y, double z)
{
    // Apply translation to current matrix
    // Simplified: would need to get current, multiply, set
    (void)x; (void)y; (void)z;
}

void directx8_renderer::rotate(double angle, double x, double y, double z)
{
    (void)angle; (void)x; (void)y; (void)z;
}

void directx8_renderer::scale(double x, double y, double z)
{
    (void)x; (void)y; (void)z;
}

void directx8_renderer::push_attrib(unsigned int /*mask*/)
{
    // DirectX8 doesn't have attribute stack
    // Would need to manually save/restore state
}

void directx8_renderer::pop_attrib()
{
    // See push_attrib()
}

void directx8_renderer::set_polygon_mode(int face, int mode)
{
    if (!pimpl_->device) return;

    if (mode == gl::k_line)
    {
        pimpl_->device->SetRenderState(D3DRS_FILLMODE, D3DFILL_WIREFRAME);
    }
    else if (mode == gl::k_point)
    {
        pimpl_->device->SetRenderState(D3DRS_FILLMODE, D3DFILL_POINT);
    }
    else
    {
        pimpl_->device->SetRenderState(D3DRS_FILLMODE, D3DFILL_SOLID);
    }
}

// ─── Factory ──────────────────────────────────────────────────────────────

auto create_directx8_renderer() -> std::unique_ptr<renderer>
{
    return std::make_unique<directx8_renderer>();
}

} // namespace sdk::render
