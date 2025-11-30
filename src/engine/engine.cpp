#include "engine.hpp"

#include "camera.hpp"
#include "imgui_layer.hpp"
#include "render.hpp"
#include "shader.hpp"

#include <imgui.h>
#include <spdlog/spdlog.h>

namespace as3
{

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

    window_ = SDL_CreateWindow(config.title,
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

    // Initialize shader manager
    shader_manager_       = std::make_unique<ShaderManager>(device_);
    const char* base_path = SDL_GetBasePath();
    if (base_path)
    {
        shader_manager_->set_shader_directory(
            std::filesystem::path(base_path) / "shaders");
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

    // Setup context
    context_.registry  = &registry_;
    context_.renderer  = renderer_.get();
    context_.shaders   = shader_manager_.get();
    context_.imgui_ctx = ImGui::GetCurrentContext();

    // Load game if specified
    if (config.game_lib)
    {
        if (!load_game(config.game_lib))
        {
            spdlog::warn("Failed to load game library, running without game");
        }
    }

    set_mouse_captured(true);
    last_time_ = SDL_GetTicks();

    return true;
}

void engine::shutdown()
{
    unload_game();

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

bool engine::load_game(const std::filesystem::path& path)
{
    unload_game();

    game_lib_path_   = path;
    game_lib_handle_ = SDL_LoadObject(path.c_str());
    if (!game_lib_handle_)
    {
        spdlog::error("Failed to load game library '{}': {}",
                      path.string(),
                      SDL_GetError());
        return false;
    }

    game_init_ = reinterpret_cast<game_init_fn>(
        SDL_LoadFunction(game_lib_handle_, "game_init"));
    game_shutdown_ = reinterpret_cast<game_shutdown_fn>(
        SDL_LoadFunction(game_lib_handle_, "game_shutdown"));
    game_update_ = reinterpret_cast<game_update_fn>(
        SDL_LoadFunction(game_lib_handle_, "game_update"));
    game_render_ = reinterpret_cast<game_render_fn>(
        SDL_LoadFunction(game_lib_handle_, "game_render"));
    game_ui_ = reinterpret_cast<game_ui_fn>(
        SDL_LoadFunction(game_lib_handle_, "game_ui"));

    if (!game_init_ || !game_shutdown_ || !game_update_ || !game_render_)
    {
        spdlog::error("Game library missing required exports");
        unload_game();
        return false;
    }

    update_context();
    if (!game_init_(&context_))
    {
        spdlog::error("Game init failed");
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
    }

    if (game_lib_handle_)
    {
        SDL_UnloadObject(game_lib_handle_);
        game_lib_handle_ = nullptr;
    }

    game_init_     = nullptr;
    game_shutdown_ = nullptr;
    game_update_   = nullptr;
    game_render_   = nullptr;
    game_ui_       = nullptr;
}

bool engine::reload_game()
{
    if (game_lib_path_.empty())
    {
        return false;
    }
    return load_game(game_lib_path_);
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

void engine::update_context()
{
    int w{}, h{};
    SDL_GetWindowSizeInPixels(window_, &w, &h);

    context_.display.width  = w;
    context_.display.height = h;
    context_.display.aspect = w > 0 && h > 0
                                  ? static_cast<float>(w) / static_cast<float>(h)
                                  : 1.0f;
    context_.delta_time     = delta_time_;
    context_.input          = input_;
}

void engine::process_events()
{
    input_.mouse_xrel    = 0.0f;
    input_.mouse_yrel    = 0.0f;
    input_.mouse_captured = mouse_captured_;

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
        else if (event.type == SDL_EVENT_KEY_DOWN)
        {
            if (event.key.key == SDLK_ESCAPE)
            {
                set_mouse_captured(!mouse_captured_);
            }
            else if (event.key.key == SDLK_F5)
            {
                reload_game();
            }
        }
        else if (mouse_captured_ && event.type == SDL_EVENT_MOUSE_MOTION)
        {
            input_.mouse_xrel += static_cast<float>(event.motion.xrel);
            input_.mouse_yrel += static_cast<float>(event.motion.yrel);
        }
    }

    input_.keyboard = SDL_GetKeyboardState(nullptr);
}

void engine::update()
{
    const Uint64 current_time = SDL_GetTicks();
    delta_time_ = static_cast<float>(current_time - last_time_) / 1000.0f;
    last_time_  = current_time;

    // Shader hot-reload check
    static float shader_timer = 0.0f;
    shader_timer += delta_time_;
    if (shader_timer >= 0.5f)
    {
        shader_timer = 0.0f;
        shader_manager_->check_for_updates();
        renderer_->reload_pipeline();
    }

    update_context();

    if (game_update_)
    {
        game_update_(&context_);
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
            renderer_->begin_frame(cmd, pass);
            renderer_->bind_pipeline();

            if (game_render_)
            {
                game_render_(&context_);
            }

            renderer_->end_frame();
            SDL_EndGPURenderPass(pass);
        }

        // ImGui pass
        imgui_layer_->begin_frame();
        if (game_ui_)
        {
            game_ui_(&context_);
        }
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
