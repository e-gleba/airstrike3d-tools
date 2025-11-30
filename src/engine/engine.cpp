#include "engine.hpp"
#include "camera.hpp"
#include "imgui_layer.hpp"
#include "render.hpp"
#include "shader.hpp"

#include <imgui.h>
#include <spdlog/spdlog.h>
#include <SDL3/SDL_loadso.h>

namespace as3
{

namespace
{
}

engine::engine() = default;
engine::~engine() { shutdown(); }

bool engine::init(const engine_config& config)
{
    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS))
    {
        spdlog::error("SDL_Init: {}", SDL_GetError());
        return false;
    }

    window_ = SDL_CreateWindow(config.title, config.width, config.height, SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY);
    if (!window_) { spdlog::error("SDL_CreateWindow: {}", SDL_GetError()); return false; }

    device_ = SDL_CreateGPUDevice(SDL_GPU_SHADERFORMAT_SPIRV | SDL_GPU_SHADERFORMAT_DXIL | SDL_GPU_SHADERFORMAT_MSL, false, nullptr);
    if (!device_) { spdlog::error("SDL_CreateGPUDevice: {}", SDL_GetError()); return false; }

    if (!SDL_ClaimWindowForGPUDevice(device_, window_))
    {
        spdlog::error("SDL_ClaimWindowForGPUDevice: {}", SDL_GetError());
        return false;
    }

    shader_manager_ = std::make_unique<ShaderManager>(device_);
    shader_manager_->set_shader_directory("shaders");

    renderer_ = std::make_unique<Renderer>();
    if (!renderer_->init(device_, shader_manager_.get()))
    {
        spdlog::error("Renderer init failed");
        return false;
    }

    imgui_layer_ = std::make_unique<ImGuiLayer>();
    if (!imgui_layer_->init(window_, device_))
    {
        spdlog::error("ImGuiLayer init failed");
        return false;
    }

    camera_system_ = std::make_unique<CameraSystem>();

    context_.registry  = &registry_;
    context_.renderer  = renderer_.get();
    context_.shaders   = shader_manager_.get();
    context_.imgui_ctx = ImGui::GetCurrentContext();

    if (config.game_lib) load_game(config.game_lib);

    last_time_ = SDL_GetPerformanceCounter();
    running_ = true;
    return true;
}

void engine::shutdown()
{
    // Call game shutdown first
    if (game_shutdown_)
    {
        game_shutdown_();
        spdlog::default_logger()->flush();
    }
    
    // Clear registry BEFORE unloading DLL (destructors may need DLL code)
    registry_.clear();
    
    // Now safe to unload DLL
    if (game_lib_handle_)
    {
        SDL_UnloadObject(game_lib_handle_);
        game_lib_handle_ = nullptr;
    }
    game_init_ = nullptr;
    game_shutdown_ = nullptr;
    game_update_ = nullptr;
    game_render_ = nullptr;
    game_ui_ = nullptr;

    imgui_layer_.reset();
    renderer_->shutdown();
    renderer_.reset();
    shader_manager_->release_all();
    shader_manager_.reset();
    camera_system_.reset();

    if (window_ && device_) SDL_ReleaseWindowFromGPUDevice(device_, window_);
    if (device_) SDL_DestroyGPUDevice(device_);
    if (window_) SDL_DestroyWindow(window_);
    device_ = nullptr; window_ = nullptr;
    SDL_Quit();
}

bool engine::load_game(const std::filesystem::path& path)
{
    game_lib_path_ = path;
    game_lib_handle_ = SDL_LoadObject(path.c_str());
    if (!game_lib_handle_)
    {
        spdlog::error("Failed to load game '{}': {}", path.string(), SDL_GetError());
        return false;
    }

    game_init_     = reinterpret_cast<game_init_fn>(SDL_LoadFunction(game_lib_handle_, "game_init"));
    game_shutdown_ = reinterpret_cast<game_shutdown_fn>(SDL_LoadFunction(game_lib_handle_, "game_shutdown"));
    game_update_   = reinterpret_cast<game_update_fn>(SDL_LoadFunction(game_lib_handle_, "game_update"));
    game_render_   = reinterpret_cast<game_render_fn>(SDL_LoadFunction(game_lib_handle_, "game_render"));
    game_ui_       = reinterpret_cast<game_ui_fn>(SDL_LoadFunction(game_lib_handle_, "game_ui"));

    if (!game_init_ || !game_shutdown_ || !game_update_ || !game_render_ || !game_ui_)
    {
        spdlog::error("Game '{}' missing required exports", path.string());
        unload_game();
        return false;
    }

    update_context();
    if (!game_init_(&context_))
    {
        spdlog::error("game_init failed");
        unload_game();
        return false;
    }

    spdlog::info("Loaded game: {}", path.string());
    return true;
}

void engine::unload_game()
{
    if (game_shutdown_)
    {
        game_shutdown_();
        spdlog::default_logger()->flush();
    }
    
    // Clear registry before unloading DLL
    registry_.clear();

    if (game_lib_handle_)
    {
        SDL_UnloadObject(game_lib_handle_);
        game_lib_handle_ = nullptr;
    }

    game_init_ = nullptr;
    game_shutdown_ = nullptr;
    game_update_ = nullptr;
    game_render_ = nullptr;
    game_ui_ = nullptr;
}

bool engine::reload_game()
{
    if (game_lib_path_.empty()) return false;
    auto path = game_lib_path_;
    unload_game();
    return load_game(path);
}

void engine::update_context()
{
    int w{}, h{};
    SDL_GetWindowSizeInPixels(window_, &w, &h);
    context_.display.width  = w;
    context_.display.height = h;
    context_.display.aspect = h > 0 ? static_cast<float>(w) / static_cast<float>(h) : 1.0f;
    context_.input          = input_;
    context_.delta_time     = delta_time_;
}

void engine::set_mouse_captured(bool captured)
{
    mouse_captured_ = captured;
    SDL_SetWindowRelativeMouseMode(window_, captured);
}

void engine::process_events()
{
    input_.mouse_xrel = 0.0f;
    input_.mouse_yrel = 0.0f;

    SDL_Event e;
    while (SDL_PollEvent(&e))
    {
        imgui_layer_->process_event(e);

        if (e.type == SDL_EVENT_QUIT)
        {
            running_ = false;
        }
        else if (e.type == SDL_EVENT_KEY_DOWN && e.key.key == SDLK_ESCAPE)
        {
            set_mouse_captured(false);
        }
        else if (e.type == SDL_EVENT_MOUSE_BUTTON_DOWN && !ImGui::GetIO().WantCaptureMouse)
        {
            set_mouse_captured(true);
        }
        else if (e.type == SDL_EVENT_MOUSE_MOTION && mouse_captured_)
        {
            input_.mouse_xrel = e.motion.xrel;
            input_.mouse_yrel = e.motion.yrel;
        }
        else if (e.type == SDL_EVENT_KEY_DOWN && e.key.key == SDLK_F5)
        {
            reload_game();
        }
    }

    input_.keyboard       = SDL_GetKeyboardState(nullptr);
    input_.mouse_captured = mouse_captured_;
}

void engine::update()
{
    // Update camera from registry
    auto view = registry_.view<CameraComponent>();
    for (auto entity : view)
    {
        auto& cam = view.get<CameraComponent>(entity);

        if (mouse_captured_)
        {
            cam.yaw   += input_.mouse_xrel * cam.look_speed;
            cam.pitch -= input_.mouse_yrel * cam.look_speed;
            cam.pitch  = glm::clamp(cam.pitch, -89.0f, 89.0f);

            float speed = cam.move_speed * delta_time_;
            if (input_.keyboard && input_.keyboard[SDL_SCANCODE_W]) cam.position += cam.front() * speed;
            if (input_.keyboard && input_.keyboard[SDL_SCANCODE_S]) cam.position -= cam.front() * speed;
            if (input_.keyboard && input_.keyboard[SDL_SCANCODE_A]) cam.position -= cam.right() * speed;
            if (input_.keyboard && input_.keyboard[SDL_SCANCODE_D]) cam.position += cam.right() * speed;
            if (input_.keyboard && input_.keyboard[SDL_SCANCODE_E]) cam.position.y += speed;
            if (input_.keyboard && input_.keyboard[SDL_SCANCODE_Q]) cam.position.y -= speed;
        }
    }

    update_context();
    if (game_update_) game_update_(&context_);
}

void engine::render()
{
    auto* cmd = SDL_AcquireGPUCommandBuffer(device_);
    if (!cmd) return;

    SDL_GPUTexture* swapchain{};
    if (!SDL_WaitAndAcquireGPUSwapchainTexture(cmd, window_, &swapchain, nullptr, nullptr))
    {
        SDL_CancelGPUCommandBuffer(cmd);
        return;
    }
    if (!swapchain)
    {
        SDL_SubmitGPUCommandBuffer(cmd);
        return;
    }

    SDL_GPUColorTargetInfo color_target{};
    color_target.texture     = swapchain;
    color_target.clear_color = { 0.1f, 0.1f, 0.15f, 1.0f };
    color_target.load_op     = SDL_GPU_LOADOP_CLEAR;
    color_target.store_op    = SDL_GPU_STOREOP_STORE;

    auto* pass = SDL_BeginGPURenderPass(cmd, &color_target, 1, nullptr);
    if (pass)
    {
        renderer_->begin_frame(cmd, pass);

        // Set VP from camera
        auto view = registry_.view<CameraComponent>();
        for (auto entity : view)
        {
            const auto& cam = view.get<CameraComponent>(entity);
            renderer_->set_view_projection(cam.projection(context_.display.aspect) * cam.view());
            break;
        }

        renderer_->bind_pipeline();
        if (game_render_) game_render_(&context_);

        renderer_->end_frame();
        SDL_EndGPURenderPass(pass);
    }

    imgui_layer_->begin_frame();
    if (game_ui_) game_ui_(&context_);
    imgui_layer_->end_frame(cmd, swapchain);

    SDL_SubmitGPUCommandBuffer(cmd);

    shader_manager_->check_for_updates();
}

void engine::run()
{
    while (running_)
    {
        Uint64 now = SDL_GetPerformanceCounter();
        delta_time_ = static_cast<float>(now - last_time_) / static_cast<float>(SDL_GetPerformanceFrequency());
        last_time_ = now;

        process_events();
        update();
        render();
    }
}

// CameraSystem
CameraMatrices CameraSystem::get_matrices(const entt::registry& reg, float aspect) const
{
    auto view = reg.view<CameraComponent>();
    for (auto entity : view)
    {
        const auto& cam = view.get<CameraComponent>(entity);
        auto v = cam.view();
        auto p = cam.projection(aspect);
        return { v, p, p * v };
    }
    return { glm::mat4(1.0f), glm::mat4(1.0f), glm::mat4(1.0f) };
}

entt::entity CameraSystem::get_active_camera(const entt::registry& reg) const
{
    auto view = reg.view<CameraComponent>();
    for (auto entity : view) return entity;
    return entt::null;
}

} // namespace as3
