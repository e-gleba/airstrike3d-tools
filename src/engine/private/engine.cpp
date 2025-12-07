#include "engine.hpp"
#include "audio.hpp"
#include "imgui_layer.hpp"
#include "render.hpp"
#include "shader.hpp"

#include <core-api/camera.hpp>

#include <imgui.h>
#include <spdlog/spdlog.h>

#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <format>
#include <ranges>

namespace euengine
{

// RAII deleters implementation
void sdl_window_deleter::operator()(SDL_Window* w) const noexcept
{
    if (w != nullptr)
    {
        SDL_DestroyWindow(w);
    }
}

void sdl_gpu_device_deleter::operator()(SDL_GPUDevice* d) const noexcept
{
    if (d != nullptr)
    {
        SDL_DestroyGPUDevice(d);
    }
}

void sdl_shared_object_deleter::operator()(SDL_SharedObject* o) const noexcept
{
    if (o != nullptr)
    {
        SDL_UnloadObject(o);
    }
}

engine::engine() = default;

engine::~engine()
{
    shutdown();
}

bool engine::init(const preinit_settings& settings)
{
    spdlog::info("=> engine init");

    // Note: SDL should already be initialized by the callback system
    // We just mark it as initialized for our tracking
    sdl_initialized_ = true;

    // Build window flags from settings
    SDL_WindowFlags window_flags = SDL_WINDOW_VULKAN;
    if (settings.window.resizable)
    {
        window_flags |= SDL_WINDOW_RESIZABLE;
    }
    if (settings.window.high_dpi)
    {
        window_flags |= SDL_WINDOW_HIGH_PIXEL_DENSITY;
    }

    // Apply window mode
    switch (settings.window.mode)
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

    // SDL_CreateWindow requires null-terminated const char*
    // std::string::c_str() provides that
    auto* raw_window = SDL_CreateWindow(settings.window.title.c_str(),
                                        settings.window.width,
                                        settings.window.height,
                                        window_flags);
    if (raw_window == nullptr)
    {
        spdlog::error("SDL_CreateWindow: {}", SDL_GetError());
        return false;
    }
    window_.reset(raw_window);

    // Set minimum window size if specified
    if (settings.window.min_width > 0 && settings.window.min_height > 0)
    {
        SDL_SetWindowMinimumSize(window_.get(),
                                 settings.window.min_width,
                                 settings.window.min_height);
    }

    // Create GPU device with multi-format support
    constexpr SDL_GPUShaderFormat shader_formats = SDL_GPU_SHADERFORMAT_SPIRV |
                                                   SDL_GPU_SHADERFORMAT_DXIL |
                                                   SDL_GPU_SHADERFORMAT_MSL;

    auto* raw_device = SDL_CreateGPUDevice(
        shader_formats, platform::is_debug_build(), nullptr);
    if (raw_device == nullptr)
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
    current_vsync_ = settings.window.vsync;
    apply_vsync_mode();

    // Initialize rendering settings
    current_msaa_   = settings.window.msaa;
    render_scale_   = settings.renderer.render_scale;
    max_anisotropy_ = settings.renderer.max_anisotropy;

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

    // Apply initial rendering settings to renderer
    renderer_->set_msaa_samples(current_msaa_);
    renderer_->set_max_anisotropy(max_anisotropy_);

    // Set frame buffering (default: 2 = double buffering)
    SDL_SetGPUAllowedFramesInFlight(device_.get(), frames_in_flight_);
    spdlog::info("Frames in flight: {}", frames_in_flight_);

    renderer_->ensure_depth_texture(
        static_cast<Uint32>(settings.window.width),
        static_cast<Uint32>(settings.window.height));

    // Initialize ImGui layer
    imgui_layer_ = std::make_unique<ImGuiLayer>();
    if (!imgui_layer_->init(window_.get(), device_.get()))
    {
        spdlog::error("imgui init failed");
        return false;
    }

    // Initialize audio subsystem
    audio_ = std::make_unique<audio_manager>();
    if (!audio_->init())
    {
        spdlog::warn("audio init failed, continuing without audio");
    }

    // Apply audio settings
    if (audio_)
    {
        master_volume_     = settings.audio.master_volume;
        music_volume_mult_ = settings.audio.music_volume;
        sfx_volume_mult_   = settings.audio.sound_volume;
        audio_->set_music_volume(master_volume_ * music_volume_mult_);
        audio_->set_sound_volume(master_volume_ * sfx_volume_mult_);
    }

    // Apply background color from settings
    background_ = settings.background;

    // Setup engine context for game
    context_.registry   = &registry_;
    context_.renderer   = renderer_.get();
    context_.shaders    = shader_manager_.get();
    context_.audio      = audio_.get();
    context_.settings   = this; // Engine implements i_engine_settings
    context_.imgui_ctx  = ImGui::GetCurrentContext();
    context_.background = &background_;

    // Initialize timing
    start_time_ = SDL_GetPerformanceCounter();
    last_time_  = start_time_;
    running_    = true;
    return true;
}

void engine::shutdown() noexcept
{
    if (!sdl_initialized_)
    {
        return;
    }

    spdlog::info("=> engine shutdown");

    // Shutdown game first
    if (game_shutdown_ != nullptr)
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
    {
        SDL_ReleaseWindowFromGPUDevice(device_.get(), window_.get());
    }

    device_.reset();
    window_.reset();

    // Note: SDL_Quit is called by the callback system, not here
    sdl_initialized_ = false;
}

void engine::apply_vsync_mode() noexcept
{
    if (!device_ || !window_)
    {
        return;
    }

    SDL_GPUPresentMode present_mode {};
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
    {
        return;
    }
    SDL_SetWindowFullscreen(window_.get(), fullscreen);
}

bool engine::is_fullscreen() const noexcept
{
    if (!window_)
    {
        return false;
    }
    return (SDL_GetWindowFlags(window_.get()) & SDL_WINDOW_FULLSCREEN) != 0;
}

std::int32_t engine::get_window_width() const noexcept
{
    if (!window_)
    {
        return 0;
    }
    int w = 0;
    SDL_GetWindowSizeInPixels(window_.get(), &w, nullptr);
    return w;
}

std::int32_t engine::get_window_height() const noexcept
{
    if (!window_)
    {
        return 0;
    }
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

void engine::set_msaa(msaa_samples samples) noexcept
{
    if (current_msaa_ != samples)
    {
        current_msaa_ = samples;
        if (renderer_)
        {
            renderer_->set_msaa_samples(samples);
        }
    }
}

void engine::set_render_scale(float scale) noexcept
{
    render_scale_ = std::clamp(scale, 0.25f, 4.0f);
    // Render scale can be applied immediately in render() if needed
}

void engine::set_max_anisotropy(float anisotropy) noexcept
{
    max_anisotropy_ = std::clamp(anisotropy, 1.0f, 16.0f);
    if (renderer_)
    {
        renderer_->set_max_anisotropy(max_anisotropy_);
    }
}

void engine::set_frames_in_flight(std::uint32_t frames) noexcept
{
    frames_in_flight_ = std::clamp(frames, 1u, 3u);
    // Note: SDL_SetGPUAllowedFramesInFlight should only be called before
    // rendering starts or between frames. Changing it during rendering can
    // cause crashes. We'll apply it at the start of the next frame via a dirty
    // flag.
    frames_in_flight_dirty_ = true;
    spdlog::info("Frames in flight will be set to: {}", frames_in_flight_);
}

bool engine::is_msaa_supported(msaa_samples samples) const noexcept
{
    if (!device_)
        return false;

    SDL_GPUSampleCount count = SDL_GPU_SAMPLECOUNT_1;
    switch (samples)
    {
        case msaa_samples::none:
            count = SDL_GPU_SAMPLECOUNT_1;
            break;
        case msaa_samples::x2:
            count = SDL_GPU_SAMPLECOUNT_2;
            break;
        case msaa_samples::x4:
            count = SDL_GPU_SAMPLECOUNT_4;
            break;
        case msaa_samples::x8:
            count = SDL_GPU_SAMPLECOUNT_8;
            break;
        case msaa_samples::x16:
            // SDL_GPU only supports up to 8x, check 8x support for 16x
            count = SDL_GPU_SAMPLECOUNT_8;
            break;
    }

    // Check if this sample count is supported for the swapchain format
    auto format =
        SDL_GetGPUSwapchainTextureFormat(device_.get(), window_.get());
    bool supported =
        SDL_GPUTextureSupportsSampleCount(device_.get(), format, count);

    // For 16x, warn that it's actually 8x
    if (samples == msaa_samples::x16 && supported)
    {
        // It's supported but will use 8x internally
    }

    return supported;
}

void engine::set_fxaa_enabled(bool enabled) noexcept
{
    fxaa_enabled_ = enabled;
    // FXAA will be applied in post-processing pass (requires shader
    // implementation)
}

void engine::set_texture_filter(texture_filter filter) noexcept
{
    texture_filter_ = filter;
    // Texture filter will be applied when creating/updating samplers
    if (renderer_)
    {
        i_renderer::texture_filter rf =
            static_cast<i_renderer::texture_filter>(static_cast<int>(filter));
        renderer_->set_texture_filter(rf);
    }
}

void engine::set_gamma(float gamma) noexcept
{
    gamma_ = std::clamp(gamma, 1.0f, 3.0f);
}

void engine::set_brightness(float brightness) noexcept
{
    brightness_ = std::clamp(brightness, -1.0f, 1.0f);
}

void engine::set_contrast(float contrast) noexcept
{
    contrast_ = std::clamp(contrast, 0.5f, 2.0f);
}

void engine::set_saturation(float saturation) noexcept
{
    saturation_ = std::clamp(saturation, 0.0f, 2.0f);
}

void engine::set_vignette(float intensity) noexcept
{
    vignette_ = std::clamp(intensity, 0.0f, 1.0f);
}

void engine::set_render_distance(float distance) noexcept
{
    render_distance_ = std::clamp(distance, 10.0f, 10000.0f);

    // Update camera far plane
    auto camera_view = registry_.view<camera_component>();
    for (auto&& [entity, cam] : camera_view.each())
    {
        cam.far_plane = render_distance_;
        break;
    }
}

bool engine::is_postprocess_available() const noexcept
{
    return renderer_ != nullptr;
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
    if (raw_lib == nullptr)
    {
        spdlog::error("SDL_LoadObject: {}", SDL_GetError());
        return false;
    }
    game_lib_.reset(raw_lib);

    // Load function pointers (preinit is optional for hot-reload)
    game_preinit_ = reinterpret_cast<game_preinit_fn>(
        SDL_LoadFunction(raw_lib, "game_preinit"));
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

    // Verify all required exports are present (preinit is optional)
    if ((game_init_ == nullptr) || (game_shutdown_ == nullptr) ||
        (game_update_ == nullptr) || (game_render_ == nullptr) ||
        (game_ui_ == nullptr))
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
    if (game_shutdown_ != nullptr)
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
        {
            spdlog::debug("=> failed to remove temp game lib: {}",
                          ec.message());
        }
        game_temp_path_.clear();
    }
}

[[nodiscard]] bool engine::reload_game() noexcept
{
    if (game_lib_path_.empty())
    {
        return false;
    }
    auto path = game_lib_path_;
    unload_game();
    return load_game(path);
}

void engine::cleanup_game_pointers() noexcept
{
    game_preinit_  = nullptr;
    game_init_     = nullptr;
    game_shutdown_ = nullptr;
    game_update_   = nullptr;
    game_render_   = nullptr;
    game_ui_       = nullptr;
}

void engine::update_context() noexcept
{
    int w = 0;
    int h = 0;
    SDL_GetWindowSizeInPixels(window_.get(), &w, &h);
    context_.display.width  = w;
    context_.display.height = h;
    context_.display.aspect =
        (h > 0) ? static_cast<float>(w) / static_cast<float>(h) : 1.0f;
    context_.input = input_;

    // Update time info
    context_.time.delta       = delta_time_;
    context_.time.elapsed     = elapsed_time_;
    context_.time.frame_count = frame_count_;
    context_.time.fps         = smoothed_fps_;
}

void engine::set_mouse_captured(bool captured) noexcept
{
    if (!SDL_SetWindowRelativeMouseMode(window_.get(), captured))
    {
        return;
    }
    mouse_captured_ = captured;
}

bool engine::process_event(const SDL_Event& event)
{
    imgui_layer_->process_event(event);

    switch (event.type)
    {
        case SDL_EVENT_QUIT:
            running_ = false;
            return false;

        case SDL_EVENT_KEY_DOWN:
            if (event.key.key == SDLK_ESCAPE)
            {
                set_mouse_captured(false);
            }
            else if (event.key.key == SDLK_F5)
            {
                static_cast<void>(reload_game());
            }
            else if (event.key.key == SDLK_F11)
            {
                set_fullscreen(!is_fullscreen());
            }
            break;

        case SDL_EVENT_MOUSE_BUTTON_DOWN:
            if (event.button.button == SDL_BUTTON_LEFT)
            {
                input_.mouse_left = true;
            }
            else if (event.button.button == SDL_BUTTON_RIGHT)
            {
                input_.mouse_right = true;
            }
            else if (event.button.button == SDL_BUTTON_MIDDLE)
            {
                input_.mouse_middle = true;
            }
            if (!ImGui::GetIO().WantCaptureMouse &&
                event.button.button == SDL_BUTTON_LEFT)
            {
                set_mouse_captured(true);
            }
            break;

        case SDL_EVENT_MOUSE_BUTTON_UP:
            if (event.button.button == SDL_BUTTON_LEFT)
            {
                input_.mouse_left = false;
            }
            else if (event.button.button == SDL_BUTTON_RIGHT)
            {
                input_.mouse_right = false;
            }
            else if (event.button.button == SDL_BUTTON_MIDDLE)
            {
                input_.mouse_middle = false;
            }
            break;

        case SDL_EVENT_MOUSE_MOTION:
            input_.mouse_x = event.motion.x;
            input_.mouse_y = event.motion.y;
            if (mouse_captured_)
            {
                input_.mouse_xrel += event.motion.xrel;
                input_.mouse_yrel += event.motion.yrel;
            }
            break;

        default:
            break;
    }

    return true;
}

void engine::update()
{
    // Update camera components using ranges
    auto camera_view = registry_.view<camera_component>();
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
                {
                    cam.position += cam.front() * speed;
                }
                if (input_.keyboard[SDL_SCANCODE_S])
                {
                    cam.position -= cam.front() * speed;
                }
                if (input_.keyboard[SDL_SCANCODE_A])
                {
                    cam.position -= cam.right() * speed;
                }
                if (input_.keyboard[SDL_SCANCODE_D])
                {
                    cam.position += cam.right() * speed;
                }
                if (input_.keyboard[SDL_SCANCODE_E])
                {
                    cam.position.y += speed;
                }
                if (input_.keyboard[SDL_SCANCODE_Q])
                {
                    cam.position.y -= speed;
                }
            }
        }
    }

    update_context();
    if (game_update_ != nullptr)
    {
        game_update_(&context_);
    }
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

    // Apply deferred frames in flight change
    if (frames_in_flight_dirty_)
    {
        // Wait for GPU to be idle before changing this
        SDL_WaitForGPUIdle(device_.get());
        SDL_SetGPUAllowedFramesInFlight(device_.get(), frames_in_flight_);
        frames_in_flight_dirty_ = false;
        spdlog::info("Frames in flight applied: {}", frames_in_flight_);
    }

    auto* cmd = SDL_AcquireGPUCommandBuffer(device_.get());
    if (cmd == nullptr)
    {
        return;
    }

    SDL_GPUTexture* swapchain   = nullptr;
    Uint32          swapchain_w = 0;
    Uint32          swapchain_h = 0;

    if (!SDL_WaitAndAcquireGPUSwapchainTexture(
            cmd, window_.get(), &swapchain, &swapchain_w, &swapchain_h))
    {
        SDL_CancelGPUCommandBuffer(cmd);
        return;
    }

    if (swapchain == nullptr)
    {
        SDL_SubmitGPUCommandBuffer(cmd);
        return;
    }

    renderer_->ensure_depth_texture(swapchain_w, swapchain_h);

    // Get swapchain format for MSAA and post-processing targets
    auto swapchain_format =
        SDL_GetGPUSwapchainTextureFormat(device_.get(), window_.get());

    // Check if post-processing is enabled (any non-default value)
    const bool use_postprocess =
        (gamma_ != 2.2f || brightness_ != 0.0f || contrast_ != 1.0f ||
         saturation_ != 1.0f || vignette_ > 0.001f || fxaa_enabled_);

    // Ensure post-processing target if needed
    if (use_postprocess)
    {
        renderer_->ensure_pp_target(swapchain_w, swapchain_h, swapchain_format);
    }

    // Ensure MSAA render targets if MSAA is enabled
    const bool use_msaa = (renderer_->get_msaa_samples() != msaa_samples::none);
    if (use_msaa)
    {
        renderer_->ensure_msaa_targets(
            swapchain_w, swapchain_h, swapchain_format);
    }

    // Determine scene output target:
    // - With postprocess: output to pp_color_texture
    // - Without postprocess: output to swapchain
    SDL_GPUTexture* scene_output =
        use_postprocess ? renderer_->pp_color_target() : swapchain;

    // Determine which textures to render to for the scene
    SDL_GPUTexture* color_texture =
        use_msaa ? renderer_->msaa_color_target() : scene_output;
    SDL_GPUTexture* depth_texture =
        use_msaa ? renderer_->msaa_depth_target() : renderer_->depth_texture();

    // Fallback if MSAA target creation failed
    if (use_msaa && (color_texture == nullptr || depth_texture == nullptr))
    {
        color_texture = scene_output;
        depth_texture = renderer_->depth_texture();
    }

    // Setup color target with game-controlled clear color
    SDL_GPUColorTargetInfo color_target {};
    color_target.texture     = color_texture;
    color_target.clear_color = {
        .r = background_.r,
        .g = background_.g,
        .b = background_.b,
        .a = background_.a,
    };
    color_target.load_op  = SDL_GPU_LOADOP_CLEAR;
    color_target.store_op = SDL_GPU_STOREOP_STORE;

    // For MSAA, we need to resolve to the scene_output target
    if (use_msaa && renderer_->msaa_color_target() != nullptr)
    {
        color_target.resolve_texture = scene_output;
        color_target.store_op        = SDL_GPU_STOREOP_RESOLVE;
    }

    // Setup depth target
    SDL_GPUDepthStencilTargetInfo depth_target {};
    depth_target.texture          = depth_texture;
    depth_target.clear_depth      = 1.0f;
    depth_target.load_op          = SDL_GPU_LOADOP_CLEAR;
    depth_target.store_op         = SDL_GPU_STOREOP_STORE;
    depth_target.stencil_load_op  = SDL_GPU_LOADOP_CLEAR;
    depth_target.stencil_store_op = SDL_GPU_STOREOP_DONT_CARE;

    auto* depth_ptr =
        (depth_target.texture != nullptr) ? &depth_target : nullptr;
    if (auto* pass = SDL_BeginGPURenderPass(cmd, &color_target, 1, depth_ptr))
    {
        renderer_->begin_frame(cmd, pass);

        // Set view projection from first camera
        auto camera_view = registry_.view<camera_component>();
        for (auto&& [entity, cam] : camera_view.each())
        {
            renderer_->set_view_projection(
                cam.projection(context_.display.aspect) * cam.view());
            break;
        }

        renderer_->bind_pipeline();
        if (game_render_ != nullptr)
        {
            game_render_(&context_);
        }

        renderer_->end_frame();
        SDL_EndGPURenderPass(pass);
    }

    // Apply post-processing if enabled
    if (use_postprocess && renderer_->pp_color_target() != nullptr)
    {
        Renderer::postprocess_params pp_params {};
        pp_params.gamma        = gamma_;
        pp_params.brightness   = brightness_;
        pp_params.contrast     = contrast_;
        pp_params.saturation   = saturation_;
        pp_params.vignette     = vignette_;
        pp_params.fxaa_enabled = fxaa_enabled_ ? 1.0f : 0.0f;
        pp_params.res_x        = static_cast<float>(swapchain_w);
        pp_params.res_y        = static_cast<float>(swapchain_h);

        renderer_->apply_postprocess(cmd, swapchain, pp_params);
    }

    // Render ImGui
    imgui_layer_->begin_frame();
    if (game_ui_ != nullptr)
    {
        game_ui_(&context_);
    }
    imgui_layer_->end_frame(cmd, swapchain);

    SDL_SubmitGPUCommandBuffer(cmd);
    shader_manager_->check_for_updates();
}

void engine::iterate()
{
    const Uint64 freq = SDL_GetPerformanceFrequency();
    const Uint64 now  = SDL_GetPerformanceCounter();

    delta_time_ =
        static_cast<float>(now - last_time_) / static_cast<float>(freq);
    last_time_ = now;

    // Clamp delta time to avoid spiral of death
    delta_time_ = std::min(delta_time_, k_max_delta_time);

    // Update timing info
    elapsed_time_ =
        static_cast<float>(now - start_time_) / static_cast<float>(freq);
    ++frame_count_;

    // Smooth FPS using exponential moving average
    constexpr float fps_smoothing = 0.9f;
    const float instant_fps = (delta_time_ > 0.0f) ? 1.0f / delta_time_ : 0.0f;
    smoothed_fps_ =
        fps_smoothing * smoothed_fps_ + (1.0f - fps_smoothing) * instant_fps;

    // Update keyboard state (mouse motion was accumulated via process_event)
    input_.keyboard       = SDL_GetKeyboardState(nullptr);
    input_.mouse_captured = mouse_captured_;

    update();
    render();

    // Reset accumulated mouse motion for next frame
    input_.mouse_xrel = 0.0f;
    input_.mouse_yrel = 0.0f;
}

} // namespace euengine
