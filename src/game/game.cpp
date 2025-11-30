#include "../shared/game_api.hpp"

#include <imgui.h>

#include <algorithm>
#include <cmath>
#include <vector>

namespace
{
entt::entity g_camera = entt::null;
std::vector<as3::mesh_handle> g_meshes;
as3::engine_context* g_ctx = nullptr;
} // namespace

GAME_API bool game_init(as3::engine_context* ctx)
{
    g_ctx = ctx;

    // Create camera
    g_camera = ctx->registry->create();
    ctx->registry->emplace<as3::CameraComponent>(g_camera);

    // Grid on the ground
    g_meshes.push_back(ctx->renderer->create_wireframe_grid(
        20.0f, 20, glm::vec3(0.3f, 0.3f, 0.3f)));

    // Cubes
    g_meshes.push_back(ctx->renderer->create_wireframe_cube(
        glm::vec3(0.0f, 1.0f, 0.0f), 2.0f, glm::vec3(0.0f, 1.0f, 0.0f)));
    g_meshes.push_back(ctx->renderer->create_wireframe_cube(
        glm::vec3(-4.0f, 0.5f, -3.0f), 1.0f, glm::vec3(1.0f, 0.0f, 0.0f)));
    g_meshes.push_back(ctx->renderer->create_wireframe_cube(
        glm::vec3(4.0f, 1.5f, 2.0f), 3.0f, glm::vec3(0.0f, 0.0f, 1.0f)));

    // Spheres
    g_meshes.push_back(ctx->renderer->create_wireframe_sphere(
        glm::vec3(0.0f, 3.0f, -6.0f), 1.5f, glm::vec3(1.0f, 1.0f, 0.0f)));
    g_meshes.push_back(ctx->renderer->create_wireframe_sphere(
        glm::vec3(-3.0f, 2.0f, 2.0f), 1.0f, glm::vec3(0.0f, 1.0f, 1.0f)));

    // Custom mesh - a simple triangle
    std::vector<as3::vertex> tri_verts = {
        { glm::vec3(5.0f, 0.0f, 0.0f), glm::vec3(1.0f, 0.0f, 1.0f) },
        { glm::vec3(6.0f, 0.0f, 0.0f), glm::vec3(1.0f, 0.0f, 1.0f) },
        { glm::vec3(5.5f, 2.0f, 0.0f), glm::vec3(1.0f, 0.0f, 1.0f) },
    };
    std::vector<uint16_t> tri_idx = { 0, 1, 1, 2, 2, 0 };
    g_meshes.push_back(ctx->renderer->create_mesh(tri_verts, tri_idx));

    return true;
}

GAME_API void game_shutdown()
{
    // Destroy meshes we created
    if (g_ctx && g_ctx->renderer)
    {
        for (auto mesh : g_meshes)
        {
            g_ctx->renderer->destroy_mesh(mesh);
        }
    }
    g_meshes.clear();

    // Destroy entities we created
    if (g_ctx && g_ctx->registry && g_camera != entt::null)
    {
        g_ctx->registry->destroy(g_camera);
    }
    g_camera = entt::null;
    g_ctx = nullptr;
}

GAME_API void game_update(as3::engine_context* ctx)
{
    auto view = ctx->registry->view<as3::CameraComponent>();
    for (auto entity : view)
    {
        auto& cam = view.get<as3::CameraComponent>(entity);

        // Mouse look
        if (ctx->input.mouse_captured)
        {
            cam.yaw += ctx->input.mouse_xrel * cam.sensitivity;
            cam.pitch = std::clamp(
                cam.pitch + (-ctx->input.mouse_yrel) * cam.sensitivity,
                -89.0f, 89.0f);

            glm::vec3 dir{};
            dir.x = std::cos(glm::radians(cam.yaw)) * std::cos(glm::radians(cam.pitch));
            dir.y = std::sin(glm::radians(cam.pitch));
            dir.z = std::sin(glm::radians(cam.yaw)) * std::cos(glm::radians(cam.pitch));
            cam.front = glm::normalize(dir);
        }

        // Movement (scancodes: W=26, S=22, A=4, D=7, Space=44, LCtrl=224)
        if (ctx->input.mouse_captured && ctx->input.keyboard)
        {
            const float vel = cam.speed * ctx->delta_time;
            const glm::vec3 right = glm::normalize(glm::cross(cam.front, cam.up));

            if (ctx->input.keyboard[26]) cam.position += cam.front * vel;
            if (ctx->input.keyboard[22]) cam.position -= cam.front * vel;
            if (ctx->input.keyboard[4])  cam.position -= right * vel;
            if (ctx->input.keyboard[7])  cam.position += right * vel;
            if (ctx->input.keyboard[44]) cam.position += cam.up * vel;
            if (ctx->input.keyboard[224]) cam.position -= cam.up * vel;
        }
    }
}

GAME_API void game_render(as3::engine_context* ctx)
{
    // Set camera matrices
    auto view = ctx->registry->view<const as3::CameraComponent>();
    for (auto entity : view)
    {
        const auto& cam = view.get<const as3::CameraComponent>(entity);

        const glm::mat4 view_mat =
            glm::lookAt(cam.position, cam.position + cam.front, cam.up);
        const glm::mat4 proj = glm::perspective(
            glm::radians(cam.fov), ctx->display.aspect, cam.near_plane, cam.far_plane);

        ctx->renderer->set_view_projection(proj * view_mat);
        break;
    }

    // Draw all meshes
    for (auto mesh : g_meshes)
    {
        ctx->renderer->draw(mesh);
    }
}

GAME_API void game_ui(as3::engine_context* ctx)
{
    ImGui::SetCurrentContext(static_cast<ImGuiContext*>(ctx->imgui_ctx));

    auto view = ctx->registry->view<as3::CameraComponent>();
    for (auto entity : view)
    {
        auto& cam = view.get<as3::CameraComponent>(entity);

        ImGui::Begin("Debug");
        ImGui::Text("pos: (%.2f, %.2f, %.2f)",
                    cam.position.x, cam.position.y, cam.position.z);
        ImGui::Text("yaw: %.1f | pitch: %.1f", cam.yaw, cam.pitch);
        ImGui::SliderFloat("speed", &cam.speed, 1.0f, 20.0f);
        ImGui::Text("fps: %.1f", ImGui::GetIO().Framerate);

        if (ctx->input.mouse_captured)
        {
            ImGui::TextColored(ImVec4(0, 1, 0, 1), "mouse: captured (ESC)");
        }
        else
        {
            ImGui::TextColored(ImVec4(1, 0, 0, 1), "mouse: free (ESC)");
        }

        ImGui::Separator();
        bool hr = ctx->shaders->hot_reload_enabled();
        if (ImGui::Checkbox("Shader Hot-Reload", &hr))
        {
            ctx->shaders->enable_hot_reload(hr);
        }
        ImGui::Text("F5 - reload game");
        ImGui::End();
        break;
    }
}
