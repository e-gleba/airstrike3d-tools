#pragma once

// sdk/render/renderer.hpp — Abstract renderer interface (pure C++23)
//
// Polukhin-style: PIMPL, backend-agnostic.
// Turner-style: RAII, noexcept where possible.
// Stefano-style: Concept-constrained operations.
//
// This interface abstracts away OpenGL and DirectX8, allowing both to coexist.
// The engine detects which API is available and instantiates the appropriate backend.

#include "sdk/render/render_api.hpp"
#include "sdk/math/types.hpp"
#include "sdk/render/types.hpp"

#include <memory>
#include <string_view>

namespace sdk::render
{

// ─── Renderer interface ──────────────────────────────────────────────────

class renderer
{
public:
    virtual ~renderer() = default;

    // ─── Lifecycle ──────────────────────────────────────────────────────

    // Initialize renderer with device context
    virtual void init(device_context* ctx) = 0;

    // Shutdown renderer
    virtual void shutdown() = 0;

    // Check if initialized
    [[nodiscard]] virtual auto is_initialized() const noexcept -> bool = 0;

    // ─── Frame management ───────────────────────────────────────────────

    // Begin frame (call before rendering)
    virtual void begin_frame() = 0;

    // End frame (call after rendering)
    virtual void end_frame() = 0;

    // ─── State management ───────────────────────────────────────────────

    // Enable/disable render states
    virtual void enable(int state) = 0;
    virtual void disable(int state) = 0;

    // Set blend function
    virtual void set_blend_func(int src_factor, int dst_factor) = 0;

    // Set depth mask
    virtual void set_depth_mask(bool enabled) = 0;

    // ─── Matrix operations ──────────────────────────────────────────────

    // Set matrix mode
    virtual void set_matrix_mode(int mode) = 0;

    // Load identity matrix
    virtual void load_identity() = 0;

    // Load matrix from array (16 elements, column-major)
    virtual void load_matrix(double const* matrix) = 0;

    // Multiply current matrix
    virtual void mult_matrix(double const* matrix) = 0;

    // Push/pop matrix stack
    virtual void push_matrix() = 0;
    virtual void pop_matrix() = 0;

    // ─── Camera ─────────────────────────────────────────────────────────

    // Set camera (look-at transformation)
    virtual void look_at(math::vec3 const& eye, math::vec3 const& center, math::vec3 const& up) = 0;

    // ─── Drawing ────────────────────────────────────────────────────────

    // Begin/end primitive
    virtual void begin(int primitive_type) = 0;
    virtual void end() = 0;

    // Set color (RGBA, 0.0-1.0)
    virtual void set_color(float r, float g, float b, float a = 1.0f) = 0;

    // Add vertex
    virtual void vertex(float x, float y, float z) = 0;
    virtual void vertex(math::vec3 const& v) = 0;

    // Set line width / point size
    virtual void set_line_width(float width) = 0;
    virtual void set_point_size(float size) = 0;

    // ─── Transform ──────────────────────────────────────────────────────

    // Apply translation
    virtual void translate(double x, double y, double z) = 0;

    // Apply rotation (angle in degrees)
    virtual void rotate(double angle, double x, double y, double z) = 0;

    // Apply scale
    virtual void scale(double x, double y, double z) = 0;

    // ─── Attribute stack ────────────────────────────────────────────────

    // Push/pop attribute stack
    virtual void push_attrib(unsigned int mask) = 0;
    virtual void pop_attrib() = 0;

    // ─── Polygon mode ───────────────────────────────────────────────────

    // Set polygon mode (fill, line, point)
    virtual void set_polygon_mode(int face, int mode) = 0;

    // ─── Factory ────────────────────────────────────────────────────────

    // Create renderer for detected API
    [[nodiscard]] static auto create(render_api api) -> std::unique_ptr<renderer>;
};

} // namespace sdk::render
