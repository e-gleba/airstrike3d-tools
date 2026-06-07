#pragma once

// sdk/render/directx8_renderer.hpp — DirectX8 renderer implementation (private)
//
// This header is PRIVATE to the render module.
// Users interact only with the abstract renderer interface.

#include "sdk/render/renderer.hpp"

namespace sdk::render
{

class directx8_renderer final : public renderer
{
public:
    directx8_renderer();
    ~directx8_renderer() override;

    // ─── Lifecycle ──────────────────────────────────────────────────────

    void init(device_context* ctx) override;
    void shutdown() override;
    [[nodiscard]] auto is_initialized() const noexcept -> bool override;

    // ─── Frame management ───────────────────────────────────────────────

    void begin_frame() override;
    void end_frame() override;

    // ─── State management ───────────────────────────────────────────────

    void enable(int state) override;
    void disable(int state) override;
    void set_blend_func(int src_factor, int dst_factor) override;
    void set_depth_mask(bool enabled) override;

    // ─── Matrix operations ──────────────────────────────────────────────

    void set_matrix_mode(int mode) override;
    void load_identity() override;
    void load_matrix(double const* matrix) override;
    void mult_matrix(double const* matrix) override;
    void push_matrix() override;
    void pop_matrix() override;

    // ─── Camera ─────────────────────────────────────────────────────────

    void look_at(math::vec3 const& eye, math::vec3 const& center, math::vec3 const& up) override;

    // ─── Drawing ────────────────────────────────────────────────────────

    void begin(int primitive_type) override;
    void end() override;
    void set_color(float r, float g, float b, float a = 1.0f) override;
    void vertex(float x, float y, float z) override;
    void vertex(math::vec3 const& v) override;
    void set_line_width(float width) override;
    void set_point_size(float size) override;

    // ─── Transform ──────────────────────────────────────────────────────

    void translate(double x, double y, double z) override;
    void rotate(double angle, double x, double y, double z) override;
    void scale(double x, double y, double z) override;

    // ─── Attribute stack ────────────────────────────────────────────────

    void push_attrib(unsigned int mask) override;
    void pop_attrib() override;

    // ─── Polygon mode ───────────────────────────────────────────────────

    void set_polygon_mode(int face, int mode) override;

private:
    struct impl;
    std::unique_ptr<impl> pimpl_;
};

// Factory function (called by renderer::create)
auto create_directx8_renderer() -> std::unique_ptr<renderer>;

} // namespace sdk::render
