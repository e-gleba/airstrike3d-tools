#include "engine.hpp"
#include "audio.hpp"
#include "imgui_layer.hpp"
#include "render.hpp"
#include "shader.hpp"

#include "../shared/camera.hpp"

#include <imgui.h>
#include <spdlog/spdlog.h>

namespace as3
{

void SDLWindowDeleter::operator()(SDL_Window* w) const noexcept
{
    if (w)
        SDL_DestroyWindow(w);
}
void SDLGPUDeviceDeleter::operator()(SDL_GPUDevice* d) const noexcept
{
    if (d)
        SDL_DestroyGPUDevice(d);
}
void SDLSharedObjectDeleter::operator()(SDL_SharedObject* o) const noexcept
{
    if (o)
        SDL_UnloadObject(o);
}

engine::engine() = default;
engine::~engine()
{
    shutdown();
}

bool engine::init(const engine_config& config)
{
    spdlog::info("=> engine init");

    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS))
    {
        spdlog::error("SDL_Init: {}", SDL_GetError());
        return false;
    }
    sdl_initialized_ = true;

    constexpr SDL_WindowFlags wf =
        SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY;
    auto* raw_window =
        SDL_CreateWindow(config.title.data(), config.width, config.height, wf);
    if (!raw_window)
    {
        spdlog::error("SDL_CreateWindow: {}", SDL_GetError());
        return false;
    }
    window_.reset(raw_window);

    constexpr SDL_GPUShaderFormat sf = SDL_GPU_SHADERFORMAT_SPIRV |
                                       SDL_GPU_SHADERFORMAT_DXIL |
                                       SDL_GPU_SHADERFORMAT_MSL;
    auto* raw_device = SDL_CreateGPUDevice(sf, false, nullptr);
    if (!raw_device)
    {
        spdlog::error("SDL_CreateGPUDevice: {}", SDL_GetError());
        return false;
    }
    device_.reset(raw_device);

    if (const char* drv = SDL_GetGPUDeviceDriver(device_.get()))
        spdlog::info("GPU driver: {}", drv);

    if (!SDL_ClaimWindowForGPUDevice(device_.get(), window_.get()))
    {
        spdlog::error("SDL_ClaimWindowForGPUDevice: {}", SDL_GetError());
        return false;
    }

    shader_manager_ = std::make_unique<ShaderManager>(device_.get());
    shader_manager_->set_shader_directory("shaders");

    renderer_ = std::make_unique<Renderer>();
    if (!renderer_->init(device_.get(), shader_manager_.get()))
    {
        spdlog::error("renderer init failed");
        return false;
    }
    renderer_->ensure_depth_texture(static_cast<Uint32>(config.width),
                                    static_cast<Uint32>(config.height));

    imgui_layer_ = std::make_unique<ImGuiLayer>();
    if (!imgui_layer_->init(window_.get(), device_.get()))
    {
        spdlog::error("imgui init failed");
        return false;
    }

    audio_ = std::make_unique<AudioManager>();
    if (!audio_->init())
        spdlog::warn("audio init failed, continuing without audio");

    context_.registry  = &registry_;
    context_.renderer  = renderer_.get();
    context_.shaders   = shader_manager_.get();
    context_.audio     = audio_.get();
    context_.imgui_ctx = ImGui::GetCurrentContext();

    if (!config.game_lib.empty() && !load_game(config.game_lib))
        spdlog::warn("game load failed, continuing without game");

    last_time_ = SDL_GetPerformanceCounter();
    running_   = true;
    return true;
}

void engine::shutdown() noexcept
{
    if (!sdl_initialized_)
        return;
    spdlog::info("=> engine shutdown");

    if (game_shutdown_)
    {
        try
        {
            game_shutdown_();
        }
        catch (...)
        {
        }
        spdlog::default_logger()->flush();
    }
    registry_.clear();
    cleanup_game_pointers();
    game_lib_.reset();

    audio_.reset();
    imgui_layer_.reset();
    if (renderer_)
    {
        renderer_->shutdown();
        renderer_.reset();
    }
    if (shader_manager_)
    {
        shader_manager_->release_all();
        shader_manager_.reset();
    }
    if (window_ && device_)
        SDL_ReleaseWindowFromGPUDevice(device_.get(), window_.get());
    device_.reset();
    window_.reset();
    SDL_Quit();
    sdl_initialized_ = false;
}

bool engine::load_game(const std::filesystem::path& path)
{
    spdlog::info("=> loading game: {}", path.string());
    game_lib_path_ = path;

    auto* raw_lib = SDL_LoadObject(path.c_str());
    if (!raw_lib)
    {
        spdlog::error("SDL_LoadObject: {}", SDL_GetError());
        return false;
    }
    game_lib_.reset(raw_lib);

    game_init_ =
        reinterpret_cast<game_init_fn>(SDL_LoadFunction(raw_lib, "game_init"));
    game_shutdown_ = reinterpret_cast<game_shutdown_fn>(
        SDL_LoadFunction(raw_lib, "game_shutdown"));
    game_update_ = reinterpret_cast<game_update_fn>(
        SDL_LoadFunction(raw_lib, "game_update"));
    game_render_ = reinterpret_cast<game_render_fn>(
        SDL_LoadFunction(raw_lib, "game_render"));
    game_ui_ =
        reinterpret_cast<game_ui_fn>(SDL_LoadFunction(raw_lib, "game_ui"));

    if (!game_init_ || !game_shutdown_ || !game_update_ || !game_render_ ||
        !game_ui_)
    {
        spdlog::error("missing game exports");
        cleanup_game_pointers();
        game_lib_.reset();
        return false;
    }

    update_context();
    if (!game_init_(&context_))
    {
        cleanup_game_pointers();
        game_lib_.reset();
        return false;
    }
    return true;
}

void engine::unload_game() noexcept
{
    if (game_shutdown_)
    {
        try
        {
            game_shutdown_();
        }
        catch (...)
        {
        }
        spdlog::default_logger()->flush();
    }
    registry_.clear();
    cleanup_game_pointers();
    game_lib_.reset();
}

[[nodiscard]] bool engine::reload_game()
{
    if (game_lib_path_.empty())
        return false;
    auto path = game_lib_path_;
    unload_game();
    return load_game(path);
}

void engine::cleanup_game_pointers() noexcept
{
    game_init_     = nullptr;
    game_shutdown_ = nullptr;
    game_update_   = nullptr;
    game_render_   = nullptr;
    game_ui_       = nullptr;
}

void engine::update_context() noexcept
{
    int w = 0, h = 0;
    SDL_GetWindowSizeInPixels(window_.get(), &w, &h);
    context_.display.width  = w;
    context_.display.height = h;
    context_.display.aspect =
        (h > 0) ? static_cast<float>(w) / static_cast<float>(h) : 1.0f;
    context_.input      = input_;
    context_.delta_time = delta_time_;
}

void engine::set_mouse_captured(bool captured) noexcept
{
    if (!SDL_SetWindowRelativeMouseMode(window_.get(), captured))
        return;
    mouse_captured_ = captured;
}

void engine::process_events()
{
    input_.mouse_xrel = input_.mouse_yrel = 0.0f;

    SDL_Event event;
    while (SDL_PollEvent(&event))
    {
        imgui_layer_->process_event(event);

        switch (event.type)
        {
            case SDL_EVENT_QUIT:
                running_ = false;
                break;
            case SDL_EVENT_KEY_DOWN:
                if (event.key.key == SDLK_ESCAPE)
                    set_mouse_captured(false);
                else if (event.key.key == SDLK_F5)
                    (void)reload_game();
                break;
            case SDL_EVENT_MOUSE_BUTTON_DOWN:
                if (!ImGui::GetIO().WantCaptureMouse)
                    set_mouse_captured(true);
                break;
            case SDL_EVENT_MOUSE_MOTION:
                if (mouse_captured_)
                {
                    input_.mouse_xrel = event.motion.xrel;
                    input_.mouse_yrel = event.motion.yrel;
                }
                break;
            default:
                break;
        }
    }

    input_.keyboard       = SDL_GetKeyboardState(nullptr);
    input_.mouse_captured = mouse_captured_;
}

void engine::update()
{
    for (auto&& [e, cam] : registry_.view<CameraComponent>().each())
    {
        if (mouse_captured_)
        {
            cam.yaw += input_.mouse_xrel * cam.look_speed;
            cam.pitch -= input_.mouse_yrel * cam.look_speed;
            cam.pitch = glm::clamp(cam.pitch, -k_pitch_limit, k_pitch_limit);

            const float speed = cam.move_speed * delta_time_;
            if (input_.keyboard)
            {
                if (input_.keyboard[SDL_SCANCODE_W])
                    cam.position += cam.front() * speed;
                if (input_.keyboard[SDL_SCANCODE_S])
                    cam.position -= cam.front() * speed;
                if (input_.keyboard[SDL_SCANCODE_A])
                    cam.position -= cam.right() * speed;
                if (input_.keyboard[SDL_SCANCODE_D])
                    cam.position += cam.right() * speed;
                if (input_.keyboard[SDL_SCANCODE_E])
                    cam.position.y += speed;
                if (input_.keyboard[SDL_SCANCODE_Q])
                    cam.position.y -= speed;
            }
        }
    }

    update_context();
    if (game_update_)
        game_update_(&context_);
}

void engine::render()
{
    auto* cmd = SDL_AcquireGPUCommandBuffer(device_.get());
    if (!cmd)
        return;

    SDL_GPUTexture* swapchain   = nullptr;
    Uint32          swapchain_w = 0, swapchain_h = 0;
    if (!SDL_WaitAndAcquireGPUSwapchainTexture(
            cmd, window_.get(), &swapchain, &swapchain_w, &swapchain_h))
    {
        SDL_CancelGPUCommandBuffer(cmd);
        return;
    }

    if (!swapchain)
    {
        SDL_SubmitGPUCommandBuffer(cmd);
        return;
    }

    renderer_->ensure_depth_texture(swapchain_w, swapchain_h);

    SDL_GPUColorTargetInfo color_target{};
    color_target.texture     = swapchain;
    color_target.clear_color = { 0.08f, 0.08f, 0.12f, 1.0f };
    color_target.load_op     = SDL_GPU_LOADOP_CLEAR;
    color_target.store_op    = SDL_GPU_STOREOP_STORE;

    SDL_GPUDepthStencilTargetInfo depth_target{};
    depth_target.texture          = renderer_->depth_texture();
    depth_target.clear_depth      = 1.0f;
    depth_target.load_op          = SDL_GPU_LOADOP_CLEAR;
    depth_target.store_op         = SDL_GPU_STOREOP_STORE;
    depth_target.stencil_load_op  = SDL_GPU_LOADOP_CLEAR;
    depth_target.stencil_store_op = SDL_GPU_STOREOP_DONT_CARE;

    auto* depth_ptr = depth_target.texture ? &depth_target : nullptr;
    if (auto* pass = SDL_BeginGPURenderPass(cmd, &color_target, 1, depth_ptr))
    {
        renderer_->begin_frame(cmd, pass);

        for (auto&& [e, cam] : registry_.view<CameraComponent>().each())
        {
            renderer_->set_view_projection(
                cam.projection(context_.display.aspect) * cam.view());
            break;
        }

        renderer_->bind_pipeline();
        if (game_render_)
            game_render_(&context_);
        renderer_->end_frame();
        SDL_EndGPURenderPass(pass);
    }

    imgui_layer_->begin_frame();
    if (game_ui_)
        game_ui_(&context_);
    imgui_layer_->end_frame(cmd, swapchain);

    SDL_SubmitGPUCommandBuffer(cmd);
    shader_manager_->check_for_updates();
}

void engine::run()
{
    while (running_)
    {
        const Uint64 now  = SDL_GetPerformanceCounter();
        const Uint64 freq = SDL_GetPerformanceFrequency();
        delta_time_ =
            static_cast<float>(now - last_time_) / static_cast<float>(freq);
        last_time_ = now;
        if (delta_time_ > k_max_delta_time)
            delta_time_ = k_max_delta_time;

        process_events();
        update();
        render();
    }
}

} // namespace as3
