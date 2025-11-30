#include "engine.hpp"

#include "camera.hpp"
#include "imgui_layer.hpp"
#include "render.hpp"
#include "shader.hpp"

#include <imgui.h>
#include <spdlog/spdlog.h>

#include <vector>

namespace as3
{

namespace
{
std::vector<gpu_mesh> test_meshes;
} // namespace

engine::engine()  = default;
engine::~engine() = default;

bool engine::init(const engine_config& config)
{
    SDL_SetHint(SDL_HINT_VIDEO_DRIVER, "x11");

    if (!SDL_Init(SDL_INIT_EVENTS | SDL_INIT_VIDEO))
    {
        spdlog::error("SDL init failed: {}", SDL_GetError());
        return false;
    }

    window_ =
        SDL_CreateWindow(config.title,
                         config.width,
                         config.height,
                         SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY);
    if (!window_)
    {
        spdlog::error("Window creation failed: {}", SDL_GetError());
        return false;
    }

    device_ = SDL_CreateGPUDevice(SDL_GPU_SHADERFORMAT_SPIRV |
                                      SDL_GPU_SHADERFORMAT_DXIL |
                                      SDL_GPU_SHADERFORMAT_MSL,
                                  true,
                                  nullptr);
    if (!device_)
    {
        spdlog::error("GPU device creation failed: {}", SDL_GetError());
        return false;
    }

    if (!SDL_ClaimWindowForGPUDevice(device_, window_))
    {
        spdlog::error("Window claim failed: {}", SDL_GetError());
        return false;
    }

    SDL_SetGPUSwapchainParameters(device_,
                                  window_,
                                  SDL_GPU_SWAPCHAINCOMPOSITION_SDR,
                                  SDL_GPU_PRESENTMODE_VSYNC);

    swapchain_format_ = SDL_GetGPUSwapchainTextureFormat(device_, window_);

    // Initialize shader manager
    shader_manager_       = std::make_unique<ShaderManager>(device_);
    const char* base_path = SDL_GetBasePath();
    if (base_path)
    {
        shader_manager_->set_shader_directory(std::filesystem::path(base_path) /
                                              "shaders");
    }

    // Initialize renderer
    renderer_ = std::make_unique<Renderer>();
    if (!renderer_->init(device_, shader_manager_.get()))
    {
        spdlog::error("Renderer init failed");
        return false;
    }

    // Initialize systems
    camera_system_ = std::make_unique<CameraSystem>();

    // Initialize ImGui layer
    imgui_layer_ = std::make_unique<ImGuiLayer>();
    if (!imgui_layer_->init(device_, window_))
    {
        spdlog::error("ImGui layer init failed");
        return false;
    }

    setup_default_scene();

    // Setup ImGui debug callback
    imgui_layer_->set_draw_callback(
        [this]()
        {
            auto cam_entity = camera_system_->get_active_camera(registry_);
            if (cam_entity != entt::null)
            {
                auto& cam = registry_.get<CameraComponent>(cam_entity);
                ImGui::Begin("Debug");
                ImGui::Text("pos: (%.2f, %.2f, %.2f)",
                            cam.position.x,
                            cam.position.y,
                            cam.position.z);
                ImGui::Text("yaw: %.1f | pitch: %.1f", cam.yaw, cam.pitch);
                ImGui::SliderFloat("speed", &cam.speed, 1.0f, 20.0f);
                ImGui::Text("fps: %.1f", ImGui::GetIO().Framerate);

                if (mouse_captured_)
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
                bool hot_reload = shader_manager_->hot_reload_enabled();
                if (ImGui::Checkbox("Shader Hot-Reload", &hot_reload))
                {
                    shader_manager_->enable_hot_reload(hot_reload);
                }
                ImGui::End();
            }
        });

    set_mouse_captured(true);
    last_time_ = SDL_GetTicks();

    return true;
}

void engine::shutdown()
{
    for (auto& mesh : test_meshes)
    {
        destroy_mesh(device_, mesh);
    }
    test_meshes.clear();

    renderer_.reset();
    imgui_layer_.reset();
    shader_manager_.reset();
    camera_system_.reset();

    if (device_ && window_)
    {
        SDL_ReleaseWindowFromGPUDevice(device_, window_);
    }
    if (device_)
    {
        SDL_DestroyGPUDevice(device_);
        device_ = nullptr;
    }
    if (window_)
    {
        SDL_DestroyWindow(window_);
        window_ = nullptr;
    }
    SDL_Quit();
}

void engine::setup_default_scene()
{
    auto camera = registry_.create();
    registry_.emplace<CameraComponent>(camera);

    // Create test cubes
    test_meshes.push_back(create_wireframe_cube(device_,
                                                glm::vec3(0.0f, 1.0f, 0.0f),
                                                2.0f,
                                                glm::vec3(0.0f, 1.0f, 0.0f)));
    test_meshes.push_back(create_wireframe_cube(device_,
                                                glm::vec3(-4.0f, 0.5f, -3.0f),
                                                1.0f,
                                                glm::vec3(1.0f, 0.0f, 0.0f)));
    test_meshes.push_back(create_wireframe_cube(device_,
                                                glm::vec3(4.0f, 1.5f, 2.0f),
                                                3.0f,
                                                glm::vec3(0.0f, 0.0f, 1.0f)));
    test_meshes.push_back(create_wireframe_cube(device_,
                                                glm::vec3(0.0f, 0.5f, -6.0f),
                                                1.0f,
                                                glm::vec3(1.0f, 1.0f, 0.0f)));
}

void engine::set_mouse_captured(bool captured)
{
    mouse_captured_ = captured;
    if (window_)
    {
        SDL_SetWindowRelativeMouseMode(window_, captured);
    }
    if (imgui_layer_)
    {
        imgui_layer_->set_input_enabled(!captured);
    }
}

void engine::process_events()
{
    SDL_Event event{};
    while (SDL_PollEvent(&event))
    {
        if (!mouse_captured_)
        {
            imgui_layer_->process_event(&event);
        }

        if (event.type == SDL_EVENT_QUIT)
        {
            running_ = false;
        }
        else if (event.type == SDL_EVENT_KEY_DOWN &&
                 event.key.key == SDLK_ESCAPE)
        {
            set_mouse_captured(!mouse_captured_);
        }
        else if (mouse_captured_ && event.type == SDL_EVENT_MOUSE_MOTION)
        {
            camera_system_->handle_mouse_motion(
                registry_,
                static_cast<float>(event.motion.xrel),
                static_cast<float>(event.motion.yrel));
        }
    }
}

void engine::update()
{
    const Uint64 current_time = SDL_GetTicks();
    delta_time_ = static_cast<float>(current_time - last_time_) / 1000.0f;
    last_time_  = current_time;

    camera_system_->update(registry_, delta_time_, mouse_captured_);

    static float shader_timer = 0.0f;
    shader_timer += delta_time_;
    if (shader_timer >= 0.5f)
    {
        shader_timer = 0.0f;
        shader_manager_->check_for_updates();
        renderer_->reload_pipeline();
    }
}

void engine::render()
{
    SDL_GPUCommandBuffer* cmd = SDL_AcquireGPUCommandBuffer(device_);
    if (!cmd)
    {
        return;
    }

    SDL_GPUTexture* swapchain = nullptr;
    if (!SDL_AcquireGPUSwapchainTexture(
            cmd, window_, &swapchain, nullptr, nullptr))
    {
        return;
    }

    if (swapchain)
    {
        int screen_w{}, screen_h{};
        SDL_GetWindowSizeInPixels(window_, &screen_w, &screen_h);
        const float aspect =
            static_cast<float>(screen_w) / static_cast<float>(screen_h);

        const auto matrices = camera_system_->get_matrices(registry_, aspect);

        // Scene render pass
        SDL_GPUColorTargetInfo color_target{
            .texture               = swapchain,
            .mip_level             = 0,
            .layer_or_depth_plane  = 0,
            .clear_color           = { .r = 20.0f / 255.0f,
                                       .g = 22.0f / 255.0f,
                                       .b = 30.0f / 255.0f,
                                       .a = 1.0f },
            .load_op               = SDL_GPU_LOADOP_CLEAR,
            .store_op              = SDL_GPU_STOREOP_STORE,
            .resolve_texture       = nullptr,
            .resolve_mip_level     = 0,
            .resolve_layer         = 0,
            .cycle                 = false,
            .cycle_resolve_texture = false,
            .padding1              = {},
            .padding2              = {},
        };

        SDL_GPURenderPass* pass =
            SDL_BeginGPURenderPass(cmd, &color_target, 1, nullptr);
        if (pass)
        {
            renderer_->begin_frame(cmd);
            renderer_->set_view_projection(matrices.view_projection);

            // Bind pipeline and draw meshes
            renderer_->bind_pipeline(pass);
            for (const auto& mesh : test_meshes)
            {
                renderer_->draw_mesh(pass, cmd, mesh);
            }

            renderer_->end_frame();
            SDL_EndGPURenderPass(pass);
        }

        // ImGui pass
        imgui_layer_->begin_frame();
        imgui_layer_->end_frame(cmd, swapchain);
    }

    SDL_SubmitGPUCommandBuffer(cmd);
}

void engine::run()
{
    running_ = true;
    while (running_)
    {
        process_events();
        update();
        render();
    }
}

} // namespace as3