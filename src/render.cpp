
#include "render.hpp"

#include <SDL3_shadercross/SDL_shadercross.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <imgui.h>
#include <imgui_impl_sdl3.h>
#include <imgui_impl_sdlgpu3.h>
#include <ranges>
#include <vector>

struct camera final
{
    glm::vec3 position    = { 0.0f, 2.0f, 8.0f };
    glm::vec3 front       = { 0.0f, 0.0f, -1.0f };
    glm::vec3 up          = { 0.0f, 1.0f, 0.0f };
    float     yaw         = -90.0f;
    float     pitch       = 0.0f;
    float     speed       = 5.0f;
    float     sensitivity = 0.1f;
};

struct renderable_cube final
{
    glm::vec3 position;
    float     size;
    glm::vec3 color;
    float     distance_to_cam;

    void update_distance(const glm::vec3& cam_pos) noexcept
    {
        distance_to_cam = glm::distance(cam_pos, position);
    }
};

struct vertex_data final
{
    glm::vec3 position;
    glm::vec3 color;
};

struct uniform_data final
{
    glm::mat4 view_proj;
};

namespace
{
constexpr float k_max_pitch   = 89.0f;
constexpr float k_min_pitch   = -89.0f;
constexpr float k_fov_degrees = 75.0f;
constexpr float k_near_plane  = 0.1f;
constexpr float k_far_plane   = 100.0f;

camera                   cam;
float                    delta_time      = 0.016f;
bool                     mouse_captured  = false;
SDL_GPUGraphicsPipeline* pipeline        = nullptr;
SDL_GPUBuffer*           vertex_buffer   = nullptr;
SDL_GPUBuffer*           index_buffer    = nullptr;
SDL_GPUTransferBuffer*   transfer_buffer = nullptr;

constexpr std::array<glm::vec3, 8> k_cube_offsets = { {
    { -1.0f, -1.0f, -1.0f },
    { 1.0f, -1.0f, -1.0f },
    { 1.0f, 1.0f, -1.0f },
    { -1.0f, 1.0f, -1.0f },
    { -1.0f, -1.0f, 1.0f },
    { 1.0f, -1.0f, 1.0f },
    { 1.0f, 1.0f, 1.0f },
    { -1.0f, 1.0f, 1.0f },
} };

constexpr std::array<std::pair<size_t, size_t>, 12> k_cube_edges = { {
    { 0, 1 },
    { 1, 2 },
    { 2, 3 },
    { 3, 0 },
    { 4, 5 },
    { 5, 6 },
    { 6, 7 },
    { 7, 4 },
    { 0, 4 },
    { 1, 5 },
    { 2, 6 },
    { 3, 7 },
} };

[[nodiscard]] static constexpr float clamp_pitch(float pitch) noexcept
{
    return std::clamp(pitch, k_min_pitch, k_max_pitch);
}

static void update_camera_direction(camera& c) noexcept
{
    glm::vec3 direction{};
    direction.x =
        std::cos(glm::radians(c.yaw)) * std::cos(glm::radians(c.pitch));
    direction.y = std::sin(glm::radians(c.pitch));
    direction.z =
        std::sin(glm::radians(c.yaw)) * std::cos(glm::radians(c.pitch));
    c.front = glm::normalize(direction);
}

static void handle_mouse_motion(camera& c,
                                float   xrel,
                                float   yrel,
                                float   sensitivity) noexcept
{
    c.yaw += xrel * sensitivity;
    c.pitch = clamp_pitch(c.pitch + (-yrel) * sensitivity);
    update_camera_direction(c);
}

static void update_camera_position(camera&     c,
                                   const bool* keys,
                                   float       velocity) noexcept
{
    if (keys[SDL_SCANCODE_W])
        c.position += c.front * velocity;
    if (keys[SDL_SCANCODE_S])
        c.position -= c.front * velocity;

    const glm::vec3 right = glm::normalize(glm::cross(c.front, c.up));
    if (keys[SDL_SCANCODE_A])
        c.position -= right * velocity;
    if (keys[SDL_SCANCODE_D])
        c.position += right * velocity;

    if (keys[SDL_SCANCODE_SPACE])
        c.position += c.up * velocity;
    if (keys[SDL_SCANCODE_LCTRL])
        c.position -= c.up * velocity;
}

[[nodiscard]] static SDL_GPUShader* compile_shader(
    SDL_GPUDevice*              device,
    const char*                 source,
    SDL_ShaderCross_ShaderStage stage,
    const char*                 entry = "main") noexcept
{
    SDL_ShaderCross_HLSL_Info info{};
    info.source       = source;
    info.entrypoint   = entry;
    info.shader_stage = stage;
    info.include_dir  = nullptr;
    info.defines      = nullptr;
    info.props        = 0;

    size_t spirv_size{};
    void*  spirv = SDL_ShaderCross_CompileSPIRVFromHLSL(&info, &spirv_size);
    if (!spirv)
    {
        SDL_LogError(SDL_LOG_CATEGORY_GPU,
                     "failed to compile spirv: %s",
                     SDL_GetError());
        return nullptr;
    }

    SDL_ShaderCross_SPIRV_Info spirv_info{};
    spirv_info.bytecode      = static_cast<Uint8*>(spirv);
    spirv_info.bytecode_size = spirv_size;
    spirv_info.entrypoint    = entry;
    spirv_info.shader_stage  = stage;
    spirv_info.props         = 0;

    SDL_ShaderCross_GraphicsShaderMetadata* metadata =
        SDL_ShaderCross_ReflectGraphicsSPIRV(
            spirv_info.bytecode, spirv_info.bytecode_size, 0);
    if (!metadata)
    {
        SDL_LogError(SDL_LOG_CATEGORY_GPU,
                     "failed to reflect spirv: %s",
                     SDL_GetError());
        SDL_free(spirv);
        return nullptr;
    }

    SDL_GPUShader* shader = SDL_ShaderCross_CompileGraphicsShaderFromSPIRV(
        device, &spirv_info, &metadata->resource_info, 0);

    SDL_free(metadata);
    SDL_free(spirv);

    if (!shader)
    {
        SDL_LogError(SDL_LOG_CATEGORY_GPU,
                     "failed to create shader: %s",
                     SDL_GetError());
    }

    return shader;
}

[[nodiscard]] static SDL_GPUGraphicsPipeline* create_pipeline(
    SDL_GPUDevice* device, SDL_GPUTextureFormat format) noexcept
{
    constexpr const char* vertex_shader = R"(
struct VertexInput
{
    float3 position : POSITION;
    float3 color : COLOR;
};

struct VertexOutput
{
    float4 position : SV_Position;
    float3 color : COLOR;
};

cbuffer UniformBlock : register(b0, space1)
{
    float4x4 view_proj;
};

VertexOutput main(VertexInput input)
{
    VertexOutput output;
    output.position = mul(view_proj, float4(input.position, 1.0));
    output.color = input.color;
    return output;
}
)";

    constexpr const char* fragment_shader = R"(
struct PixelInput
{
    float3 color : COLOR;
};

float4 main(PixelInput input) : SV_Target0
{
    return float4(input.color, 1.0);
}
)";

    SDL_GPUShader* vert = compile_shader(
        device, vertex_shader, SDL_SHADERCROSS_SHADERSTAGE_VERTEX);
    SDL_GPUShader* frag = compile_shader(
        device, fragment_shader, SDL_SHADERCROSS_SHADERSTAGE_FRAGMENT);

    if (!vert || !frag)
    {
        if (vert)
            SDL_ReleaseGPUShader(device, vert);
        if (frag)
            SDL_ReleaseGPUShader(device, frag);
        return nullptr;
    }

    SDL_GPUVertexAttribute vertex_attrs[2]{};
    vertex_attrs[0].location    = 0;
    vertex_attrs[0].format      = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3;
    vertex_attrs[0].offset      = 0;
    vertex_attrs[0].buffer_slot = 0;
    vertex_attrs[1].location    = 1;
    vertex_attrs[1].format      = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3;
    vertex_attrs[1].offset      = sizeof(glm::vec3);
    vertex_attrs[1].buffer_slot = 0;

    SDL_GPUVertexBufferDescription vertex_buffer_desc{};
    vertex_buffer_desc.slot       = 0;
    vertex_buffer_desc.pitch      = sizeof(vertex_data);
    vertex_buffer_desc.input_rate = SDL_GPU_VERTEXINPUTRATE_VERTEX;

    SDL_GPUVertexInputState vertex_input{};
    vertex_input.num_vertex_buffers         = 1;
    vertex_input.vertex_buffer_descriptions = &vertex_buffer_desc;
    vertex_input.num_vertex_attributes      = 2;
    vertex_input.vertex_attributes          = vertex_attrs;

    SDL_GPUColorTargetDescription color_target{};
    color_target.format = format;

    SDL_GPUGraphicsPipelineTargetInfo target_info{};
    target_info.num_color_targets         = 1;
    target_info.color_target_descriptions = &color_target;
    target_info.has_depth_stencil_target  = false;

    SDL_GPUGraphicsPipelineCreateInfo pipeline_info{};
    pipeline_info.target_info                = target_info;
    pipeline_info.vertex_shader              = vert;
    pipeline_info.fragment_shader            = frag;
    pipeline_info.vertex_input_state         = vertex_input;
    pipeline_info.primitive_type             = SDL_GPU_PRIMITIVETYPE_LINELIST;
    pipeline_info.rasterizer_state.fill_mode = SDL_GPU_FILLMODE_FILL;
    pipeline_info.rasterizer_state.cull_mode = SDL_GPU_CULLMODE_NONE;

    SDL_GPUGraphicsPipeline* p =
        SDL_CreateGPUGraphicsPipeline(device, &pipeline_info);

    SDL_ReleaseGPUShader(device, vert);
    SDL_ReleaseGPUShader(device, frag);

    if (!p)
    {
        SDL_LogError(SDL_LOG_CATEGORY_GPU,
                     "failed to create pipeline: %s",
                     SDL_GetError());
    }

    return p;
}

static void build_cube_geometry(std::vector<vertex_data>& vertices,
                                std::vector<Uint16>&      indices,
                                const glm::vec3&          center,
                                float                     size,
                                const glm::vec3&          color) noexcept
{
    const float  half = size * 0.5f;
    const Uint16 base = static_cast<Uint16>(vertices.size());

    for (const auto& offset : k_cube_offsets)
    {
        vertices.push_back({ center + offset * half, color });
    }

    for (const auto& [i, j] : k_cube_edges)
    {
        indices.push_back(base + static_cast<Uint16>(i));
        indices.push_back(base + static_cast<Uint16>(j));
    }
}
} // namespace

void as3::init_renderer(SDL_GPUDevice* device, SDL_Window* window)
{
    SDL_ShaderCross_Init();

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    ImGui::StyleColorsDark();

    ImGui_ImplSDL3_InitForOther(window);

    ImGui_ImplSDLGPU3_InitInfo init_info{};
    init_info.Device = device;
    init_info.ColorTargetFormat =
        SDL_GetGPUSwapchainTextureFormat(device, window);
    init_info.MSAASamples          = SDL_GPU_SAMPLECOUNT_1;
    init_info.SwapchainComposition = SDL_GPU_SWAPCHAINCOMPOSITION_SDR;

    ImGui_ImplSDLGPU3_Init(&init_info);

    pipeline = create_pipeline(device, init_info.ColorTargetFormat);

    constexpr size_t max_vertices = 4 * 8;
    constexpr size_t max_indices  = 4 * 24;

    SDL_GPUBufferCreateInfo vb_info{};
    vb_info.usage = SDL_GPU_BUFFERUSAGE_VERTEX;
    vb_info.size  = max_vertices * sizeof(vertex_data);
    vertex_buffer = SDL_CreateGPUBuffer(device, &vb_info);

    SDL_GPUBufferCreateInfo ib_info{};
    ib_info.usage = SDL_GPU_BUFFERUSAGE_INDEX;
    ib_info.size  = max_indices * sizeof(Uint16);
    index_buffer  = SDL_CreateGPUBuffer(device, &ib_info);

    SDL_GPUTransferBufferCreateInfo tb_info{};
    tb_info.usage   = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
    tb_info.size    = vb_info.size + ib_info.size;
    transfer_buffer = SDL_CreateGPUTransferBuffer(device, &tb_info);

    SDL_SetWindowRelativeMouseMode(window, true);
    mouse_captured = true;
}

void as3::shutdown_renderer() noexcept
{
    ImGui_ImplSDLGPU3_Shutdown();
    ImGui_ImplSDL3_Shutdown();
    ImGui::DestroyContext();
    SDL_ShaderCross_Quit();
}

void as3::handle_event(const SDL_Event* event) noexcept
{
    if (!mouse_captured)
    {
        ImGui_ImplSDL3_ProcessEvent(event);
    }

    if (event->type == SDL_EVENT_KEY_DOWN && event->key.key == SDLK_ESCAPE)
        [[unlikely]]
    {
        mouse_captured     = !mouse_captured;
        SDL_Window* window = SDL_GetWindowFromID(event->key.windowID);
        SDL_SetWindowRelativeMouseMode(window, mouse_captured);
    }

    if (mouse_captured && event->type == SDL_EVENT_MOUSE_MOTION) [[likely]]
    {
        handle_mouse_motion(cam,
                            static_cast<float>(event->motion.xrel),
                            static_cast<float>(event->motion.yrel),
                            cam.sensitivity);
    }
}

void as3::render(SDL_GPUDevice* device, SDL_Window* window)
{
    static Uint64 last_time    = SDL_GetTicks();
    const Uint64  current_time = SDL_GetTicks();
    delta_time = static_cast<float>(current_time - last_time) / 1000.0f;
    last_time  = current_time;

    if (mouse_captured)
    {
        const bool* keys     = SDL_GetKeyboardState(nullptr);
        const float velocity = cam.speed * delta_time;
        update_camera_position(cam, keys, velocity);
    }

    int screen_w{}, screen_h{};
    SDL_GetWindowSizeInPixels(window, &screen_w, &screen_h);

    const float aspect =
        static_cast<float>(screen_w) / static_cast<float>(screen_h);
    const glm::mat4 projection = glm::perspective(
        glm::radians(k_fov_degrees), aspect, k_near_plane, k_far_plane);
    const glm::mat4 view =
        glm::lookAt(cam.position, cam.position + cam.front, cam.up);
    const uniform_data uniforms{ projection * view };

    std::array<renderable_cube, 4> cubes = { {
        { { 0.0f, 1.0f, 0.0f }, 2.0f, { 0.0f, 1.0f, 0.0f }, 0.0f },
        { { -4.0f, 0.5f, -3.0f }, 1.0f, { 1.0f, 0.0f, 0.0f }, 0.0f },
        { { 4.0f, 1.5f, 2.0f }, 3.0f, { 0.0f, 0.0f, 1.0f }, 0.0f },
        { { 0.0f, 0.5f, -6.0f }, 1.0f, { 1.0f, 1.0f, 0.0f }, 0.0f },
    } };

    std::ranges::for_each(cubes,
                          [&](renderable_cube& cube) noexcept
                          { cube.update_distance(cam.position); });
    std::ranges::sort(
        cubes,
        [](const renderable_cube& a, const renderable_cube& b) noexcept
        { return a.distance_to_cam > b.distance_to_cam; });

    std::vector<vertex_data> vertices;
    std::vector<Uint16>      indices;
    vertices.reserve(cubes.size() * 8);
    indices.reserve(cubes.size() * 24);

    for (const auto& cube : cubes)
    {
        build_cube_geometry(
            vertices, indices, cube.position, cube.size, cube.color);
    }

    SDL_GPUCommandBuffer* cmd = SDL_AcquireGPUCommandBuffer(device);
    if (!cmd)
    {
        SDL_LogError(SDL_LOG_CATEGORY_GPU,
                     "failed to acquire cmd buffer: %s",
                     SDL_GetError());
        return;
    }

    void* transfer_ptr =
        SDL_MapGPUTransferBuffer(device, transfer_buffer, false);
    const auto vb_size =
        static_cast<Uint32>(vertices.size() * sizeof(vertex_data));
    const auto ib_size = static_cast<Uint32>(indices.size() * sizeof(Uint16));

    std::memcpy(transfer_ptr, vertices.data(), vb_size);
    std::memcpy(
        static_cast<char*>(transfer_ptr) + vb_size, indices.data(), ib_size);
    SDL_UnmapGPUTransferBuffer(device, transfer_buffer);

    SDL_GPUCopyPass* copy = SDL_BeginGPUCopyPass(cmd);

    SDL_GPUTransferBufferLocation src_vb{};
    src_vb.transfer_buffer = transfer_buffer;
    src_vb.offset          = 0;

    SDL_GPUBufferRegion dst_vb{};
    dst_vb.buffer = vertex_buffer;
    dst_vb.offset = 0;
    dst_vb.size   = vb_size;

    SDL_UploadToGPUBuffer(copy, &src_vb, &dst_vb, false);

    SDL_GPUTransferBufferLocation src_ib{};
    src_ib.transfer_buffer = transfer_buffer;
    src_ib.offset          = vb_size;

    SDL_GPUBufferRegion dst_ib{};
    dst_ib.buffer = index_buffer;
    dst_ib.offset = 0;
    dst_ib.size   = ib_size;

    SDL_UploadToGPUBuffer(copy, &src_ib, &dst_ib, false);
    SDL_EndGPUCopyPass(copy);

    SDL_GPUTexture* swapchain{};
    if (!SDL_AcquireGPUSwapchainTexture(
            cmd, window, &swapchain, nullptr, nullptr))
    {
        SDL_LogError(SDL_LOG_CATEGORY_GPU,
                     "failed to acquire swapchain: %s",
                     SDL_GetError());
        return;
    }

    if (swapchain)
    {
        SDL_GPUColorTargetInfo color_target{};
        color_target.texture     = swapchain;
        color_target.clear_color = {
            20.0f / 255.0f, 22.0f / 255.0f, 30.0f / 255.0f, 1.0f
        };
        color_target.load_op  = SDL_GPU_LOADOP_CLEAR;
        color_target.store_op = SDL_GPU_STOREOP_STORE;

        SDL_GPURenderPass* pass =
            SDL_BeginGPURenderPass(cmd, &color_target, 1, nullptr);

        SDL_BindGPUGraphicsPipeline(pass, pipeline);

        SDL_GPUBufferBinding vb_binding{};
        vb_binding.buffer = vertex_buffer;
        vb_binding.offset = 0;
        SDL_BindGPUVertexBuffers(pass, 0, &vb_binding, 1);

        SDL_GPUBufferBinding ib_binding{};
        ib_binding.buffer = index_buffer;
        ib_binding.offset = 0;
        SDL_BindGPUIndexBuffer(
            pass, &ib_binding, SDL_GPU_INDEXELEMENTSIZE_16BIT);

        SDL_PushGPUVertexUniformData(cmd, 0, &uniforms, sizeof(uniform_data));
        SDL_DrawGPUIndexedPrimitives(
            pass, static_cast<Uint32>(indices.size()), 1, 0, 0, 0);

        SDL_EndGPURenderPass(pass);

        ImGui_ImplSDLGPU3_NewFrame();
        ImGui_ImplSDL3_NewFrame();
        ImGui::NewFrame();

        ImGui::Begin("camera debug");
        ImGui::Text("pos: (%.2f, %.2f, %.2f)",
                    cam.position.x,
                    cam.position.y,
                    cam.position.z);
        ImGui::Text("yaw: %.1f | pitch: %.1f", cam.yaw, cam.pitch);
        ImGui::SliderFloat("speed", &cam.speed, 1.0f, 20.0f);
        ImGui::Text("fps: %.1f", ImGui::GetIO().Framerate);

        if (mouse_captured)
        {
            ImGui::TextColored({ 0.0f, 1.0f, 0.0f, 1.0f },
                               "mouse: captured (ESC to release)");
        }
        else
        {
            ImGui::TextColored({ 1.0f, 0.0f, 0.0f, 1.0f },
                               "mouse: free (ESC to capture)");
        }
        ImGui::End();

        ImGui::Render();
        ImDrawData* draw_data = ImGui::GetDrawData();
        ImGui_ImplSDLGPU3_PrepareDrawData(draw_data, cmd);

        color_target.load_op = SDL_GPU_LOADOP_LOAD;
        SDL_GPURenderPass* imgui_pass =
            SDL_BeginGPURenderPass(cmd, &color_target, 1, nullptr);
        ImGui_ImplSDLGPU3_RenderDrawData(draw_data, cmd, imgui_pass);
        SDL_EndGPURenderPass(imgui_pass);
    }

    SDL_SubmitGPUCommandBuffer(cmd);
}
