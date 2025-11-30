#include "engine.hpp"
#include "audio.hpp"
#include "camera.hpp"
#include "imgui_layer.hpp"
#include "render.hpp"
#include "shader.hpp"

#include <imgui.h>
#include <spdlog/spdlog.h>

#include <stdexcept>

namespace as3
{

// SDL Resource Deleters
void SDLWindowDeleter::operator()(SDL_Window* w) const noexcept
{
    if (w)
    {
        spdlog::debug("Destroying SDL window");
        SDL_DestroyWindow(w);
    }
}

void SDLGPUDeviceDeleter::operator()(SDL_GPUDevice* d) const noexcept
{
    if (d)
    {
        spdlog::debug("Destroying SDL GPU device");
        SDL_DestroyGPUDevice(d);
    }
}

void SDLSharedObjectDeleter::operator()(SDL_SharedObject* o) const noexcept
{
    if (o)
    {
        spdlog::debug("Unloading shared object");
        SDL_UnloadObject(o);
    }
}

engine::engine() = default;

engine::~engine()
{
    shutdown();
}

bool engine::init(const engine_config& config)
{
    spdlog::info("Initializing engine...");

    // Initialize SDL
    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS))
    {
        spdlog::error("SDL_Init failed: {}", SDL_GetError());
        return false;
    }
    sdl_initialized_ = true;
    spdlog::debug("SDL initialized");

    // Create window
    constexpr SDL_WindowFlags window_flags = SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY;
    auto* raw_window = SDL_CreateWindow(config.title.data(), config.width, config.height, window_flags);
    if (!raw_window)
    {
        spdlog::error("SDL_CreateWindow failed: {}", SDL_GetError());
        return false;
    }
    window_.reset(raw_window);
    spdlog::debug("Window created: {}x{}", config.width, config.height);

    // Create GPU device
    constexpr SDL_GPUShaderFormat shader_formats = 
        SDL_GPU_SHADERFORMAT_SPIRV | SDL_GPU_SHADERFORMAT_DXIL | SDL_GPU_SHADERFORMAT_MSL;
    
    auto* raw_device = SDL_CreateGPUDevice(shader_formats, false, nullptr);
    if (!raw_device)
    {
        spdlog::error("SDL_CreateGPUDevice failed: {}", SDL_GetError());
        return false;
    }
    device_.reset(raw_device);
    
    if (const char* driver = SDL_GetGPUDeviceDriver(device_.get()))
    {
        spdlog::info("GPU device created, driver: {}", driver);
    }

    // Claim window for GPU
    if (!SDL_ClaimWindowForGPUDevice(device_.get(), window_.get()))
    {
        spdlog::error("SDL_ClaimWindowForGPUDevice failed: {}", SDL_GetError());
        return false;
    }
    spdlog::debug("Window claimed for GPU device");

    // Initialize shader manager
    shader_manager_ = std::make_unique<ShaderManager>(device_.get());
    shader_manager_->set_shader_directory("shaders");

    // Initialize renderer
    renderer_ = std::make_unique<Renderer>();
    if (!renderer_->init(device_.get(), shader_manager_.get()))
    {
        spdlog::error("Renderer initialization failed");
        return false;
    }
    spdlog::debug("Renderer initialized");
    
    // Create initial depth texture
    renderer_->ensure_depth_texture(static_cast<Uint32>(config.width), static_cast<Uint32>(config.height));

    // Initialize ImGui layer
    imgui_layer_ = std::make_unique<ImGuiLayer>();
    if (!imgui_layer_->init(window_.get(), device_.get()))
    {
        spdlog::error("ImGui layer initialization failed");
        return false;
    }
    spdlog::debug("ImGui layer initialized");

    // Initialize camera system
    camera_system_ = std::make_unique<CameraSystem>();
    
    // Initialize audio
    audio_ = std::make_unique<AudioManager>();
    if (!audio_->init())
    {
        spdlog::warn("Audio initialization failed, continuing without audio");
    }

    // Setup context for game
    context_.registry  = &registry_;
    context_.renderer  = renderer_.get();
    context_.shaders   = shader_manager_.get();
    context_.audio     = audio_.get();
    context_.imgui_ctx = ImGui::GetCurrentContext();

    // Load game if specified
    if (!config.game_lib.empty())
    {
        if (!load_game(config.game_lib))
        {
            spdlog::warn("Failed to load game library, continuing without game");
        }
    }

    last_time_ = SDL_GetPerformanceCounter();
    running_   = true;

    spdlog::info("Engine initialized successfully");
    return true;
}

void engine::shutdown() noexcept
{
    if (!sdl_initialized_)
    {
        return;
    }

    spdlog::info("Shutting down engine...");

    // Shutdown game first (while DLL is still loaded)
    if (game_shutdown_)
    {
        try
        {
            game_shutdown_();
        }
        catch (const std::exception& e)
        {
            spdlog::error("Exception in game_shutdown: {}", e.what());
        }
        spdlog::default_logger()->flush();
    }

    // Clear registry BEFORE unloading DLL (component destructors may need DLL code)
    registry_.clear();

    // Unload game library
    cleanup_game_pointers();
    game_lib_.reset();

    // Shutdown subsystems in reverse order
    audio_.reset();  // Shutdown audio first
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

    camera_system_.reset();

    // Release window from GPU before destroying device
    if (window_ && device_)
    {
        SDL_ReleaseWindowFromGPUDevice(device_.get(), window_.get());
        spdlog::debug("Released window from GPU device");
    }

    // Smart pointers handle destruction, but we need specific order
    device_.reset();
    window_.reset();

    // Quit SDL
    SDL_Quit();
    sdl_initialized_ = false;

    spdlog::info("Engine shutdown complete");
}

bool engine::load_game(const std::filesystem::path& path)
{
    spdlog::info("Loading game: {}", path.string());

    game_lib_path_ = path;

    auto* raw_lib = SDL_LoadObject(path.c_str());
    if (!raw_lib)
    {
        spdlog::error("SDL_LoadObject('{}') failed: {}", path.string(), SDL_GetError());
        return false;
    }
    game_lib_.reset(raw_lib);

    // Load function pointers
    game_init_     = reinterpret_cast<game_init_fn>(SDL_LoadFunction(raw_lib, "game_init"));
    game_shutdown_ = reinterpret_cast<game_shutdown_fn>(SDL_LoadFunction(raw_lib, "game_shutdown"));
    game_update_   = reinterpret_cast<game_update_fn>(SDL_LoadFunction(raw_lib, "game_update"));
    game_render_   = reinterpret_cast<game_render_fn>(SDL_LoadFunction(raw_lib, "game_render"));
    game_ui_       = reinterpret_cast<game_ui_fn>(SDL_LoadFunction(raw_lib, "game_ui"));

    // Validate all required exports
    bool missing = false;
    if (!game_init_)     { spdlog::error("Missing export: game_init");     missing = true; }
    if (!game_shutdown_) { spdlog::error("Missing export: game_shutdown"); missing = true; }
    if (!game_update_)   { spdlog::error("Missing export: game_update");   missing = true; }
    if (!game_render_)   { spdlog::error("Missing export: game_render");   missing = true; }
    if (!game_ui_)       { spdlog::error("Missing export: game_ui");       missing = true; }

    if (missing)
    {
        cleanup_game_pointers();
        game_lib_.reset();
        return false;
    }

    // Initialize game
    update_context();
    if (!game_init_(&context_))
    {
        spdlog::error("game_init() returned false");
        cleanup_game_pointers();
        game_lib_.reset();
        return false;
    }

    spdlog::info("Game loaded successfully");
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
        catch (const std::exception& e)
        {
            spdlog::error("Exception in game_shutdown: {}", e.what());
        }
        spdlog::default_logger()->flush();
    }

    // Clear registry before unloading DLL
    registry_.clear();

    // Unload library
    cleanup_game_pointers();
    game_lib_.reset();

    spdlog::debug("Game unloaded");
}

bool engine::reload_game()
{
    if (game_lib_path_.empty())
    {
        spdlog::warn("No game library path set, cannot reload");
        return false;
    }

    spdlog::info("Reloading game...");
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
    if (!SDL_GetWindowSizeInPixels(window_.get(), &w, &h))
    {
        spdlog::warn("SDL_GetWindowSizeInPixels failed: {}", SDL_GetError());
    }

    context_.display.width  = w;
    context_.display.height = h;
    context_.display.aspect = (h > 0) ? static_cast<float>(w) / static_cast<float>(h) : 1.0f;
    context_.input          = input_;
    context_.delta_time     = delta_time_;
}

void engine::set_mouse_captured(bool captured) noexcept
{
    if (!SDL_SetWindowRelativeMouseMode(window_.get(), captured))
    {
        spdlog::warn("SDL_SetWindowRelativeMouseMode failed: {}", SDL_GetError());
        return;
    }
    mouse_captured_ = captured;
}

void engine::process_events()
{
    input_.mouse_xrel = 0.0f;
    input_.mouse_yrel = 0.0f;

    SDL_Event event;
    while (SDL_PollEvent(&event))
    {
        imgui_layer_->process_event(event);

        switch (event.type)
        {
        case SDL_EVENT_QUIT:
            spdlog::debug("Quit event received");
            running_ = false;
            break;

        case SDL_EVENT_KEY_DOWN:
            if (event.key.key == SDLK_ESCAPE)
            {
                set_mouse_captured(false);
            }
            else if (event.key.key == SDLK_F5)
            {
                (void)reload_game();
            }
            break;

        case SDL_EVENT_MOUSE_BUTTON_DOWN:
            if (!ImGui::GetIO().WantCaptureMouse)
            {
                set_mouse_captured(true);
            }
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
    // Update camera from registry
    auto view = registry_.view<CameraComponent>();
    for (auto entity : view)
    {
        auto& cam = view.get<CameraComponent>(entity);

        if (mouse_captured_)
        {
            cam.yaw   += input_.mouse_xrel * cam.look_speed;
            cam.pitch -= input_.mouse_yrel * cam.look_speed;
            cam.pitch  = glm::clamp(cam.pitch, -k_pitch_limit, k_pitch_limit);

            const float speed = cam.move_speed * delta_time_;
            if (input_.keyboard)
            {
                if (input_.keyboard[SDL_SCANCODE_W]) cam.position += cam.front() * speed;
                if (input_.keyboard[SDL_SCANCODE_S]) cam.position -= cam.front() * speed;
                if (input_.keyboard[SDL_SCANCODE_A]) cam.position -= cam.right() * speed;
                if (input_.keyboard[SDL_SCANCODE_D]) cam.position += cam.right() * speed;
                if (input_.keyboard[SDL_SCANCODE_E]) cam.position.y += speed;
                if (input_.keyboard[SDL_SCANCODE_Q]) cam.position.y -= speed;
            }
        }
    }

    update_context();

    if (game_update_)
    {
        game_update_(&context_);
    }
}

void engine::render()
{
    auto* cmd = SDL_AcquireGPUCommandBuffer(device_.get());
    if (!cmd)
    {
        spdlog::warn("SDL_AcquireGPUCommandBuffer failed: {}", SDL_GetError());
        return;
    }

    SDL_GPUTexture* swapchain = nullptr;
    Uint32 swapchain_w = 0, swapchain_h = 0;
    if (!SDL_WaitAndAcquireGPUSwapchainTexture(cmd, window_.get(), &swapchain, &swapchain_w, &swapchain_h))
    {
        spdlog::warn("SDL_WaitAndAcquireGPUSwapchainTexture failed: {}", SDL_GetError());
        SDL_CancelGPUCommandBuffer(cmd);
        return;
    }

    if (!swapchain)
    {
        // Window minimized or similar
        SDL_SubmitGPUCommandBuffer(cmd);
        return;
    }
    
    // Ensure depth texture matches swapchain size
    renderer_->ensure_depth_texture(swapchain_w, swapchain_h);

    // Setup render pass with color and depth targets
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
    depth_target.cycle            = false;
    depth_target.clear_stencil    = 0;

    // Only use depth target if texture was created successfully
    SDL_GPUDepthStencilTargetInfo* depth_ptr = depth_target.texture ? &depth_target : nullptr;
    auto* pass = SDL_BeginGPURenderPass(cmd, &color_target, 1, depth_ptr);
    if (pass)
    {
        renderer_->begin_frame(cmd, pass);

        // Set view-projection from camera
        auto view = registry_.view<CameraComponent>();
        for (auto entity : view)
        {
            const auto& cam = view.get<CameraComponent>(entity);
            renderer_->set_view_projection(cam.projection(context_.display.aspect) * cam.view());
            break;
        }

        renderer_->bind_pipeline();

        if (game_render_)
        {
            game_render_(&context_);
        }

        renderer_->end_frame();
        SDL_EndGPURenderPass(pass);
    }
    else
    {
        spdlog::warn("SDL_BeginGPURenderPass failed: {}", SDL_GetError());
    }

    // ImGui pass
    imgui_layer_->begin_frame();
    if (game_ui_)
    {
        game_ui_(&context_);
    }
    imgui_layer_->end_frame(cmd, swapchain);

    // Submit
    if (!SDL_SubmitGPUCommandBuffer(cmd))
    {
        spdlog::warn("SDL_SubmitGPUCommandBuffer failed: {}", SDL_GetError());
    }

    // Check for shader hot-reload
    shader_manager_->check_for_updates();
}

void engine::run()
{
    spdlog::info("Entering main loop");

    while (running_)
    {
        const Uint64 now  = SDL_GetPerformanceCounter();
        const Uint64 freq = SDL_GetPerformanceFrequency();

        delta_time_ = static_cast<float>(now - last_time_) / static_cast<float>(freq);
        last_time_  = now;

        // Clamp delta time to avoid huge jumps (e.g., after breakpoint)
        if (delta_time_ > k_max_delta_time)
        {
            delta_time_ = k_max_delta_time;
        }

        process_events();
        update();
        render();
    }

    spdlog::info("Exiting main loop");
}

// CameraSystem implementation
CameraMatrices CameraSystem::get_matrices(const entt::registry& reg, float aspect) const
{
    auto view = reg.view<CameraComponent>();
    for (auto entity : view)
    {
        const auto& cam = view.get<CameraComponent>(entity);
        const auto v = cam.view();
        const auto p = cam.projection(aspect);
        return { v, p, p * v };
    }
    return { glm::mat4(1.0f), glm::mat4(1.0f), glm::mat4(1.0f) };
}

entt::entity CameraSystem::get_active_camera(const entt::registry& reg) const
{
    auto view = reg.view<CameraComponent>();
    for (auto entity : view)
    {
        return entity;
    }
    return entt::null;
}

} // namespace as3
