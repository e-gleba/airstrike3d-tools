#include "debug_ui.hpp"

#include "camera.hpp"
#include "engine.hpp"
#include "shader.hpp"

#include <imgui.h>

namespace as3
{

void debug_ui::draw(engine& eng)
{
    auto* cam_sys    = eng.camera_system();
    auto& registry   = eng.registry();
    auto  cam_entity = cam_sys->get_active_camera(registry);

    if (cam_entity != entt::null)
    {
        auto& cam = registry.get<CameraComponent>(cam_entity);

        ImGui::Begin("Debug");
        ImGui::Text("pos: (%.2f, %.2f, %.2f)",
                    cam.position.x,
                    cam.position.y,
                    cam.position.z);
        ImGui::Text("yaw: %.1f | pitch: %.1f", cam.yaw, cam.pitch);
        ImGui::SliderFloat("speed", &cam.speed, 1.0f, 20.0f);
        ImGui::Text("fps: %.1f", ImGui::GetIO().Framerate);

        if (eng.mouse_captured())
        {
            ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f),
                               "mouse: captured (ESC to release)");
        }
        else
        {
            ImGui::TextColored(ImVec4(1.0f, 0.0f, 0.0f, 1.0f),
                               "mouse: free (ESC to capture)");
        }

        ImGui::Separator();
        auto* shaders    = eng.shaders();
        bool  hot_reload = shaders->hot_reload_enabled();
        if (ImGui::Checkbox("Shader Hot-Reload", &hot_reload))
        {
            shaders->enable_hot_reload(hot_reload);
        }
        ImGui::End();
    }
}

} // namespace as3

