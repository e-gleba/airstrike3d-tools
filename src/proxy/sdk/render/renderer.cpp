// sdk/render/renderer.cpp — Renderer factory
//
// Creates appropriate backend based on detected render API.

#include "sdk/render/renderer.hpp"

#include <spdlog/spdlog.h>

namespace sdk::render
{

// Forward declarations of backend implementations
class opengl_renderer;
class directx8_renderer;

// ─── Factory ──────────────────────────────────────────────────────────────

auto renderer::create(render_api api) -> std::unique_ptr<renderer>
{
    switch (api)
    {
        case render_api::opengl:
        {
            spdlog::info("[render] creating OpenGL renderer");
            // Forward declare - actual implementation in opengl_renderer.cpp
            extern auto create_opengl_renderer() -> std::unique_ptr<renderer>;
            return create_opengl_renderer();
        }

        case render_api::directx:
        {
            spdlog::info("[render] creating DirectX8 renderer");
            // Forward declare - actual implementation in directx8_renderer.cpp
            extern auto create_directx8_renderer() -> std::unique_ptr<renderer>;
            return create_directx8_renderer();
        }

        case render_api::unknown:
        default:
        {
            spdlog::error("[render] unknown render API");
            return nullptr;
        }
    }
}

} // namespace sdk::render
