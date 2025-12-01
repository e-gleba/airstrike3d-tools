#include "engine.hpp"
#include "audio.hpp"
#include "imgui_layer.hpp"
#include "render.hpp"
#include "shader.hpp"

#include "../shared/camera.hpp"

#include <imgui.h>
#include <spdlog/spdlog.h>

#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <format>
#include <ranges>

namespace as3
{

// RAII deleters implementation
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

    // Initialize SDL with video and events
    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS))
    {
        spdlog::error("SDL_Init: {}", SDL_GetError());
        return false;
    }
    sdl_initialized_ = true;

    // Build window flags from settings
    SDL_WindowFlags window_flags = SDL_WINDOW_VULKAN;
    if (config.window.resizable)
        window_flags |= SDL_WINDOW_RESIZABLE;
    if (config.window.high_dpi)
        window_flags |= SDL_WINDOW_HIGH_PIXEL_DENSITY;

    // Apply window mode
    switch (config.window.mode)
    {
        case window_mode::borderless:
            window_flags |= SDL_WINDOW_BORDERLESS;
            break;
        case window_mode::fullscreen:
            window_flags |= SDL_WINDOW_FULLSCREEN;
            break;
        case window_mode::fullscreen_desktop:
            window_flags |= SDL_WINDOW_FULLSCREEN;
            break;
        case window_mode::windowed:
        default:
            break;
    }

    auto* raw_window = SDL_CreateWindow(config.window.title.data(),
                                        config.window.width,
                                        config.window.height,
                                        window_flags);
    if (!raw_window)
    {
        spdlog::error("SDL_CreateWindow: {}", SDL_GetError());
        return false;
    }
    window_.reset(raw_window);

    // Set minimum window size if specified
    if (config.window.min_width > 0 && config.window.min_height > 0)
    {
        SDL_SetWindowMinimumSize(
            window_.get(), config.window.min_width, config.window.min_height);
    }

    // Create GPU device with multi-format support
    constexpr SDL_GPUShaderFormat shader_formats = SDL_GPU_SHADERFORMAT_SPIRV |
                                                   SDL_GPU_SHADERFORMAT_DXIL |
                                                   SDL_GPU_SHADERFORMAT_MSL;

    auto* raw_device = SDL_CreateGPUDevice(
        shader_formats, platform::is_debug_build(), nullptr);
    if (!raw_device)
    {
        spdlog::error("SDL_CreateGPUDevice: {}", SDL_GetError());
        return false;
    }
    device_.reset(raw_device);

    if (const char* drv = SDL_GetGPUDeviceDriver(device_.get()))
    {
        gpu_driver_name_ = drv;
        spdlog::info("GPU driver: {}", drv);
    }

    // Claim window for GPU rendering
    if (!SDL_ClaimWindowForGPUDevice(device_.get(), window_.get()))
    {
        spdlog::error("SDL_ClaimWindowForGPUDevice: {}", SDL_GetError());
        return false;
    }

    // Apply VSync setting
    current_vsync_ = config.window.vsync;
    apply_vsync_mode();

    // Initialize shader manager
    shader_manager_ = std::make_unique<ShaderManager>(device_.get());
    shader_manager_->set_shader_directory("shaders");

    // Initialize renderer
    renderer_ = std::make_unique<Renderer>();
    if (!renderer_->init(device_.get(), shader_manager_.get()))
    {
        spdlog::error("renderer init failed");
        return false;
    }
    renderer_->ensure_depth_texture(static_cast<Uint32>(config.window.width),
                                    static_cast<Uint32>(config.window.height));

    // Initialize ImGui layer
    imgui_layer_ = std::make_unique<ImGuiLayer>();
    if (!imgui_layer_->init(window_.get(), device_.get()))
    {
        spdlog::error("imgui init failed");
        return false;
    }

    // Initialize audio subsystem
    audio_ = std::make_unique<AudioManager>();
    if (!audio_->init())
        spdlog::warn("audio init failed, continuing without audio");

    // Setup engine context for game
    context_.registry  = &registry_;
    context_.renderer  = renderer_.get();
    context_.shaders   = shader_manager_.get();
    context_.audio     = audio_.get();
    context_.settings  = this; // Engine implements IEngineSettings
    context_.imgui_ctx = ImGui::GetCurrentContext();

    // Load game library if specified
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

    // Shutdown game first
    if (game_shutdown_)
    {
        try
        {
            game_shutdown_();
        }
        catch (...)
        {
            // Ignore exceptions during shutdown
        }
        spdlog::default_logger()->flush();
    }
    registry_.clear();
    cleanup_game_pointers();
    game_lib_.reset();

    // Shutdown subsystems in reverse order
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

void engine::apply_vsync_mode() noexcept
{
    if (!device_ || !window_)
        return;

    SDL_GPUPresentMode present_mode{};
    switch (current_vsync_)
    {
        case vsync_mode::disabled:
            present_mode = SDL_GPU_PRESENTMODE_IMMEDIATE;
            break;
        case vsync_mode::adaptive:
            present_mode = SDL_GPU_PRESENTMODE_MAILBOX;
            break;
        case vsync_mode::enabled:
        default:
            present_mode = SDL_GPU_PRESENTMODE_VSYNC;
            break;
    }

    SDL_SetGPUSwapchainParameters(device_.get(),
                                  window_.get(),
                                  SDL_GPU_SWAPCHAINCOMPOSITION_SDR,
                                  present_mode);
}

void engine::set_vsync(vsync_mode mode) noexcept
{
    if (current_vsync_ != mode)
    {
        // Defer the change to be applied at the start of next frame
        // Changing swapchain parameters mid-frame can cause crashes
        pending_vsync_ = mode;
        vsync_dirty_   = true;
        spdlog::info("VSync mode changed to: {}",
                     mode == vsync_mode::disabled  ? "disabled"
                     : mode == vsync_mode::enabled ? "enabled"
                                                   : "adaptive");
    }
}

void engine::set_fullscreen(bool fullscreen) noexcept
{
    if (!window_)
        return;
    SDL_SetWindowFullscreen(window_.get(), fullscreen);
}

bool engine::is_fullscreen() const noexcept
{
    if (!window_)
        return false;
    return (SDL_GetWindowFlags(window_.get()) & SDL_WINDOW_FULLSCREEN) != 0;
}

std::int32_t engine::get_window_width() const noexcept
{
    if (!window_)
        return 0;
    int w = 0;
    SDL_GetWindowSizeInPixels(window_.get(), &w, nullptr);
    return w;
}

std::int32_t engine::get_window_height() const noexcept
{
    if (!window_)
        return 0;
    int h = 0;
    SDL_GetWindowSizeInPixels(window_.get(), nullptr, &h);
    return h;
}

std::string_view engine::get_gpu_driver() const noexcept
{
    return gpu_driver_name_;
}

void engine::set_target_fps(float fps) noexcept
{
    target_fps_ = std::max(0.0f, fps);
    spdlog::info("Target FPS set to: {}",
                 target_fps_ > 0 ? target_fps_ : -1.0f);
}

void engine::set_master_volume(float volume) noexcept
{
    master_volume_ = std::clamp(volume, 0.0f, 1.0f);
    if (audio_)
    {
        audio_->set_music_volume(master_volume_ * music_volume_mult_);
        audio_->set_sound_volume(master_volume_ * sfx_volume_mult_);
    }
}

float engine::get_master_volume() const noexcept
{
    return master_volume_;
}

bool engine::load_game(const std::filesystem::path& path)
{
    spdlog::info("=> loading game: {}", path.string());
    game_lib_path_ = path;

    // For hot-reload to work, we must copy the DLL to a temp location
    // Otherwise the dynamic loader caches the old version
    std::filesystem::path load_path = path;

    if (std::filesystem::exists(path))
    {
        // Create temp copy with timestamp to ensure unique name
        auto temp_dir = std::filesystem::temp_directory_path();
        auto timestamp =
            std::chrono::steady_clock::now().time_since_epoch().count();
        auto temp_name = std::format(
            "game_{}_{}{}", timestamp, std::rand(), path.extension().string());
        auto temp_path = temp_dir / temp_name;

        std::error_code ec;
        std::filesystem::copy_file(
            path,
            temp_path,
            std::filesystem::copy_options::overwrite_existing,
            ec);

        if (!ec)
        {
            load_path       = temp_path;
            game_temp_path_ = temp_path; // Store for cleanup
            spdlog::debug("=> copied game lib to: {}", temp_path.string());
        }
        else
        {
            spdlog::warn("=> failed to copy game lib: {}", ec.message());
        }
    }

    auto* raw_lib = SDL_LoadObject(load_path.c_str());
    if (!raw_lib)
    {
        spdlog::error("SDL_LoadObject: {}", SDL_GetError());
        return false;
    }
    game_lib_.reset(raw_lib);

    // Load function pointers
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

    // Verify all required exports are present
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

    // Cleanup temp file after unloading the library
    if (!game_temp_path_.empty())
    {
        std::error_code ec;
        std::filesystem::remove(game_temp_path_, ec);
        if (ec)
            spdlog::debug("=> failed to remove temp game lib: {}",
                          ec.message());
        game_temp_path_.clear();
    }
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
    input_.mouse_xrel = 0.0f;
    input_.mouse_yrel = 0.0f;

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
                    static_cast<void>(reload_game());
                else if (event.key.key == SDLK_F11)
                    set_fullscreen(!is_fullscreen());
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
    // Update camera components using ranges
    auto camera_view = registry_.view<CameraComponent>();
    for (auto&& [entity, cam] : camera_view.each())
    {
        if (mouse_captured_)
        {
            cam.yaw += input_.mouse_xrel * cam.look_speed;
            cam.pitch -= input_.mouse_yrel * cam.look_speed;
            cam.pitch = std::clamp(cam.pitch, -k_pitch_limit, k_pitch_limit);

            const float speed = cam.move_speed * delta_time_;
            if (input_.keyboard != nullptr)
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
    // Apply deferred VSync change before acquiring swapchain
    if (vsync_dirty_)
    {
        current_vsync_ = pending_vsync_;
        apply_vsync_mode();
        vsync_dirty_ = false;
    }

    auto* cmd = SDL_AcquireGPUCommandBuffer(device_.get());
    if (!cmd)
        return;

    SDL_GPUTexture* swapchain   = nullptr;
    Uint32          swapchain_w = 0;
    Uint32          swapchain_h = 0;

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

    // Setup color target with dark clear color
    SDL_GPUColorTargetInfo color_target{};
    color_target.texture     = swapchain;
    color_target.clear_color = { 0.08f, 0.08f, 0.12f, 1.0f };
    color_target.load_op     = SDL_GPU_LOADOP_CLEAR;
    color_target.store_op    = SDL_GPU_STOREOP_STORE;

    // Setup depth target
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

        // Set view projection from first camera
        auto camera_view = registry_.view<CameraComponent>();
        for (auto&& [entity, cam] : camera_view.each())
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

    // Render ImGui
    imgui_layer_->begin_frame();
    if (game_ui_)
        game_ui_(&context_);
    imgui_layer_->end_frame(cmd, swapchain);

    SDL_SubmitGPUCommandBuffer(cmd);
    shader_manager_->check_for_updates();
}

void engine::run()
{
    const Uint64 freq = SDL_GetPerformanceFrequency();

    while (running_)
    {
        const Uint64 now = SDL_GetPerformanceCounter();
        delta_time_ =
            static_cast<float>(now - last_time_) / static_cast<float>(freq);
        last_time_ = now;

        // Clamp delta time to avoid spiral of death
        delta_time_ = std::min(delta_time_, k_max_delta_time);

        process_events();
        update();
        render();
    }
}

} // namespace as3
