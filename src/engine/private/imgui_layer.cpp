#include "imgui_layer.hpp"

#include <imgui.h>
#include <imgui_freetype.h>
#include <imgui_impl_sdl3.h>
#include <imgui_impl_sdlgpu3.h>
#include <spdlog/spdlog.h>

namespace euengine
{

ImGuiLayer::~ImGuiLayer()
{
    shutdown();
}

bool ImGuiLayer::init(SDL_Window* window, SDL_GPUDevice* device)
{
    device_ = device;

    IMGUI_CHECKVERSION();
    if (ImGui::CreateContext() == nullptr)
    {
        spdlog::error("Failed to create ImGui context");
        return false;
    }

    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    ImGui::StyleColorsDark();

    // Configure FreeType for better font rendering and alignment
    // Use FreeType loader instead of default stb_truetype for better quality
    io.Fonts->SetFontLoader(ImGuiFreeType::GetFontLoader());

    // Set loader flags for better text alignment and readability (like
    // ClearType)
    ImFontAtlas* atlas = io.Fonts;
    // Use LightHinting for better text alignment and readability (like
    // ClearType)
    atlas->FontLoaderFlags = ImGuiFreeTypeLoaderFlags_LightHinting;

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

    ImGui_ImplSDLGPU3_InitInfo init_info {};
    init_info.Device               = device;
    init_info.ColorTargetFormat    = target_format_;
    init_info.MSAASamples          = SDL_GPU_SAMPLECOUNT_1;
    init_info.SwapchainComposition = SDL_GPU_SWAPCHAINCOMPOSITION_SDR;

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

void ImGuiLayer::process_event(const SDL_Event& event) const
{
    if (input_enabled_)
    {
        ImGui_ImplSDL3_ProcessEvent(&event);
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

    SDL_GPUColorTargetInfo color_target {};
    color_target.texture  = target;
    color_target.load_op  = SDL_GPU_LOADOP_LOAD;
    color_target.store_op = SDL_GPU_STOREOP_STORE;

    SDL_GPURenderPass* pass =
        SDL_BeginGPURenderPass(cmd, &color_target, 1, nullptr);
    if (pass != nullptr)
    {
        ImGui_ImplSDLGPU3_RenderDrawData(draw_data, cmd, pass);
        SDL_EndGPURenderPass(pass);
    }
}

void ImGuiLayer::set_draw_callback(DrawCallback callback)
{
    draw_callback_ = std::move(callback);
}

} // namespace euengine
