#include "imgui_layer.hpp"

#include <imgui.h>
#include <imgui_impl_sdl3.h>
#include <imgui_impl_sdlgpu3.h>
#include <spdlog/spdlog.h>

namespace as3
{

ImGuiLayer::~ImGuiLayer()
{
    shutdown();
}

bool ImGuiLayer::init(SDL_GPUDevice* device, SDL_Window* window)
{
    device_ = device;

    IMGUI_CHECKVERSION();
    if (!ImGui::CreateContext())
    {
        spdlog::error("Failed to create ImGui context");
        return false;
    }

    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    ImGui::StyleColorsDark();

    if (!ImGui_ImplSDL3_InitForOther(window))
    {
        spdlog::error("Failed to init ImGui SDL3 backend");
        return false;
    }

    target_format_ = SDL_GetGPUSwapchainTextureFormat(device, window);
    if (target_format_ == SDL_GPU_TEXTUREFORMAT_INVALID)
    {
        spdlog::error("Failed to get swapchain format: {}", SDL_GetError());
        return false;
    }

    ImGui_ImplSDLGPU3_InitInfo init_info{
        .Device               = device,
        .ColorTargetFormat    = target_format_,
        .MSAASamples          = SDL_GPU_SAMPLECOUNT_1,
        .SwapchainComposition = SDL_GPU_SWAPCHAINCOMPOSITION_SDR
    };

    if (!ImGui_ImplSDLGPU3_Init(&init_info))
    {
        spdlog::error("Failed to init ImGui SDL GPU3 backend");
        return false;
    }

    initialized_ = true;
    return true;
}

void ImGuiLayer::shutdown()
{
    if (initialized_)
    {
        ImGui_ImplSDLGPU3_Shutdown();
        ImGui_ImplSDL3_Shutdown();
        ImGui::DestroyContext();
        initialized_ = false;
    }
}

void ImGuiLayer::process_event(const SDL_Event* event)
{
    if (input_enabled_)
    {
        ImGui_ImplSDL3_ProcessEvent(event);
    }
}

void ImGuiLayer::begin_frame()
{
    ImGui_ImplSDLGPU3_NewFrame();
    ImGui_ImplSDL3_NewFrame();
    ImGui::NewFrame();

    if (draw_callback_)
    {
        draw_callback_();
    }
}

void ImGuiLayer::end_frame(SDL_GPUCommandBuffer* cmd, SDL_GPUTexture* target)
{
    ImGui::Render();
    ImDrawData* draw_data = ImGui::GetDrawData();
    ImGui_ImplSDLGPU3_PrepareDrawData(draw_data, cmd);

    SDL_GPUColorTargetInfo color_target{
        .texture               = target,
        .mip_level             = 0,
        .layer_or_depth_plane  = 0,
        .clear_color           = {},
        .load_op               = SDL_GPU_LOADOP_LOAD,
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
        ImGui_ImplSDLGPU3_RenderDrawData(draw_data, cmd, pass);
        SDL_EndGPURenderPass(pass);
    }
}

void ImGuiLayer::set_draw_callback(DrawCallback callback)
{
    draw_callback_ = std::move(callback);
}

} // namespace as3

