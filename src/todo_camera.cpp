#define WIN32_LEAN_AND_MEAN
#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE

#include "bass_proxy.hpp"

// System
#include <GL/gl.h>
#include <windows.h>

// SafetyHook
#include <safetyhook.hpp>

// GLM
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

// ImGui
#include <imgui.h>
#include <imgui_impl_opengl3.h>
#include <imgui_impl_win32.h>

// Std
#include <atomic>
#include <mutex>
#include <thread>

// ------------------------------------------------------------------------------------------------
// CONFIG & DATA
// ------------------------------------------------------------------------------------------------

static constexpr auto ui_toggle_key = VK_INSERT;
static constexpr auto glsl_version  = "#version 110";

// Types
using wgl_swap_t         = BOOL(WINAPI*)(HDC);
using gl_load_identity_t = void(APIENTRY*)();
using glu_look_at_t      = void(APIENTRY*)(GLdouble,
                                      GLdouble,
                                      GLdouble,
                                      GLdouble,
                                      GLdouble,
                                      GLdouble,
                                      GLdouble,
                                      GLdouble,
                                      GLdouble);
using gl_matrix_mode_t   = void(APIENTRY*)(GLenum);

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND,
                                                             UINT,
                                                             WPARAM,
                                                             LPARAM);

struct camera_config final
{
    std::atomic<bool> enabled{ true };
    std::atomic<bool> mouse_look{ false };
    std::atomic<bool> hook_identity{ true };

    std::atomic<float> base_speed{ 20.0f };
    std::atomic<float> sprint_mult{ 4.0f };
    std::atomic<float> sensitivity{ 0.15f };

    // Position & Rotation
    // Yaw initialized to -90.0 (looking down -Z in OpenGL)
    glm::dvec3 pos{ 0.0, 10.0, 0.0 };
    glm::vec2  rot{ -90.0f, 0.0f }; // x=yaw, y=pitch

    POINT cursor_save{ 0, 0 };
};

// Hooks
struct hook_registry final
{
    safetyhook::InlineHook wgl_swap;
    safetyhook::InlineHook gl_matrix_mode;
    safetyhook::InlineHook gl_load_identity;
    safetyhook::InlineHook glu_look_at;

    void reset() { *this = hook_registry{}; }
};

struct app_context final
{
    HWND              window{ nullptr };
    WNDPROC           original_wnd_proc{ nullptr };
    std::atomic<bool> imgui_initialized{ false };
    std::atomic<bool> should_unload{ false };
    std::atomic<bool> show_ui{ true };

    camera_config cam;
    hook_registry hooks;
    GLenum        current_matrix_mode{ GL_MODELVIEW };
};

static app_context ctx;

// Forward Decls
static void             render_overlay();
static LRESULT CALLBACK hk_wnd_proc(HWND, UINT, WPARAM, LPARAM);

// ------------------------------------------------------------------------------------------------
// RELIABLE MATH
// ------------------------------------------------------------------------------------------------

struct camera_vectors
{
    glm::dvec3 front;
    glm::dvec3 right;
    glm::dvec3 up;
};

// Single source of truth for ALL vector math
static camera_vectors calculate_vectors()
{
    // 1. Convert to Radians
    const double yaw_rad   = glm::radians(static_cast<double>(ctx.cam.rot.x));
    const double pitch_rad = glm::radians(static_cast<double>(ctx.cam.rot.y));

    // 2. Spherical to Cartesian (Standard OpenGL Right-Handed)
    glm::dvec3 f;
    f.x = std::cos(yaw_rad) * std::cos(pitch_rad);
    f.y = std::sin(pitch_rad);
    f.z = std::sin(yaw_rad) * std::cos(pitch_rad);

    camera_vectors v;
    v.front = glm::normalize(f);

    // 3. Calculate orthonormal basis
    // World Up is always (0, 1, 0)
    static constexpr glm::dvec3 world_up{ 0.0, 1.0, 0.0 };

    v.right = glm::normalize(glm::cross(v.front, world_up));
    v.up    = glm::normalize(glm::cross(v.right, v.front));

    return v;
}

static void process_input()
{
    if (!ctx.cam.enabled.load(std::memory_order_relaxed))
        return;

    // --- Mouse Input ---
    if (ctx.cam.mouse_look.load(std::memory_order_relaxed))
    {
        RECT rect;
        GetWindowRect(ctx.window, &rect);
        const int cx = (rect.left + rect.right) / 2;
        const int cy = (rect.top + rect.bottom) / 2;

        POINT cur;
        GetCursorPos(&cur);

        if (cur.x != cx || cur.y != cy)
        {
            const float sens =
                ctx.cam.sensitivity.load(std::memory_order_relaxed);

            // Standard FPS Mouse:
            // Move Right (+X) -> Yaw Increases
            // Move Up    (-Y) -> Pitch Increases
            ctx.cam.rot.x += static_cast<float>(cur.x - cx) * sens;
            ctx.cam.rot.y -= static_cast<float>(cur.y - cy) * sens;

            // Clamp Pitch to avoid Gimbal Lock
            ctx.cam.rot.y = glm::clamp(ctx.cam.rot.y, -89.0f, 89.0f);
            // Modulo Yaw
            ctx.cam.rot.x = glm::mod(ctx.cam.rot.x, 360.0f);

            SetCursorPos(cx, cy);
        }
    }

    // --- Keyboard Input ---
    const auto v =
        calculate_vectors(); // Get fresh vectors based on new rotation

    const float dt    = ImGui::GetIO().DeltaTime;
    float       speed = ctx.cam.base_speed.load(std::memory_order_relaxed);
    if (GetAsyncKeyState(VK_SHIFT) & 0x8000)
        speed *= ctx.cam.sprint_mult.load(std::memory_order_relaxed);

    const double step = static_cast<double>(speed * dt);

    // Full Noclip Movement (3D)
    if (GetAsyncKeyState('W') & 0x8000)
        ctx.cam.pos += v.front * step;
    if (GetAsyncKeyState('S') & 0x8000)
        ctx.cam.pos -= v.front * step;
    if (GetAsyncKeyState('D') & 0x8000)
        ctx.cam.pos += v.right * step;
    if (GetAsyncKeyState('A') & 0x8000)
        ctx.cam.pos -= v.right * step;

    // Vertical Absolute
    if (GetAsyncKeyState(VK_SPACE) & 0x8000)
        ctx.cam.pos += glm::dvec3(0, 1, 0) * step;
    if (GetAsyncKeyState(VK_CONTROL) & 0x8000)
        ctx.cam.pos -= glm::dvec3(0, 1, 0) * step;
}

static void apply_camera_transform()
{
    const auto v = calculate_vectors();

    // Apply LookAt matrix
    // Center = Pos + Front
    glm::dmat4 view = glm::lookAt(ctx.cam.pos, ctx.cam.pos + v.front, v.up);

    glMultMatrixd(glm::value_ptr(view));
}

// ------------------------------------------------------------------------------------------------
// DETOURS
// ------------------------------------------------------------------------------------------------

template <typename T> static auto call_orig(safetyhook::InlineHook& hook) -> T
{
    return reinterpret_cast<T>(hook.trampoline().address());
}

static void APIENTRY hk_gl_matrix_mode(GLenum mode)
{
    ctx.current_matrix_mode = mode;
    if (ctx.hooks.gl_matrix_mode)
        call_orig<gl_matrix_mode_t>(ctx.hooks.gl_matrix_mode)(mode);
}

static void APIENTRY hk_gl_load_identity()
{
    // 1. Do the real work
    if (ctx.hooks.gl_load_identity)
        call_orig<gl_load_identity_t>(ctx.hooks.gl_load_identity)();

    // 2. If we are in ModelView, multiply our camera matrix on top of Identity
    if (ctx.cam.enabled.load(std::memory_order_relaxed) &&
        ctx.cam.hook_identity.load(std::memory_order_relaxed) &&
        ctx.current_matrix_mode == GL_MODELVIEW)
    {
        apply_camera_transform();
    }
}

static void APIENTRY hk_glu_look_at(GLdouble ex,
                                    GLdouble ey,
                                    GLdouble ez,
                                    GLdouble cx,
                                    GLdouble cy,
                                    GLdouble cz,
                                    GLdouble ux,
                                    GLdouble uy,
                                    GLdouble uz)
{
    // If enabled, we completely ignore the game's camera request
    if (ctx.cam.enabled.load(std::memory_order_relaxed))
    {
        apply_camera_transform();
    }
    else if (ctx.hooks.glu_look_at)
    {
        call_orig<glu_look_at_t>(ctx.hooks.glu_look_at)(
            ex, ey, ez, cx, cy, cz, ux, uy, uz);
    }
}

static BOOL WINAPI hk_wgl_swap(HDC dc)
{
    if (ctx.imgui_initialized.load(std::memory_order_acquire))
    {
        process_input();
        if (ctx.show_ui.load(std::memory_order_relaxed))
            render_overlay();
    }

    static std::once_flag init_once;
    if (wglGetCurrentContext())
    {
        std::call_once(
            init_once,
            [&](HDC hdc_target)
            {
                ctx.window = WindowFromDC(hdc_target);
                if (ctx.window)
                {
                    ctx.original_wnd_proc = (WNDPROC)SetWindowLongPtrA(
                        ctx.window, GWLP_WNDPROC, (LONG_PTR)hk_wnd_proc);

                    ImGui::CreateContext();
                    ImGuiIO& io = ImGui::GetIO();
                    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
                    io.IniFilename = nullptr;

                    ImGui::StyleColorsDark();
                    ImGui_ImplWin32_Init(ctx.window);
                    ImGui_ImplOpenGL3_Init(glsl_version);

                    SetWindowTextA(ctx.window, "Airstrike 3D [SAFETY]");
                    ctx.imgui_initialized.store(true,
                                                std::memory_order_release);
                }
            },
            dc);
    }

    return call_orig<wgl_swap_t>(ctx.hooks.wgl_swap)(dc);
}

static LRESULT CALLBACK hk_wnd_proc(HWND h, UINT m, WPARAM w, LPARAM l)
{
    if (m == WM_KEYDOWN && w == ui_toggle_key)
    {
        bool s = ctx.show_ui.load();
        ctx.show_ui.store(!s);
        return 0;
    }

    if (ctx.cam.enabled.load(std::memory_order_relaxed))
    {
        if (m == WM_RBUTTONDOWN)
        {
            GetCursorPos(&ctx.cam.cursor_save);
            RECT r;
            GetWindowRect(h, &r);
            SetCursorPos((r.left + r.right) / 2, (r.top + r.bottom) / 2);
            ctx.cam.mouse_look.store(true);
            ShowCursor(FALSE);
            return 0;
        }
        if (m == WM_RBUTTONUP)
        {
            ctx.cam.mouse_look.store(false);
            SetCursorPos(ctx.cam.cursor_save.x, ctx.cam.cursor_save.y);
            ShowCursor(TRUE);
            return 0;
        }
    }

    if (!ctx.should_unload.load() && ctx.show_ui.load() &&
        ImGui_ImplWin32_WndProcHandler(h, m, w, l))
        return true;

    return CallWindowProc(ctx.original_wnd_proc, h, m, w, l);
}

// ------------------------------------------------------------------------------------------------
// INSTALLER
// ------------------------------------------------------------------------------------------------

void install_hooks()
{
    auto get_addr = [](const wchar_t* mod, const char* func) -> void*
    {
        return reinterpret_cast<void*>(
            GetProcAddress(GetModuleHandleW(mod), func));
    };

    ctx.hooks.wgl_swap =
        safetyhook::create_inline(get_addr(L"opengl32.dll", "wglSwapBuffers"),
                                  reinterpret_cast<void*>(hk_wgl_swap));

    ctx.hooks.gl_matrix_mode =
        safetyhook::create_inline(get_addr(L"opengl32.dll", "glMatrixMode"),
                                  reinterpret_cast<void*>(hk_gl_matrix_mode));

    ctx.hooks.gl_load_identity =
        safetyhook::create_inline(get_addr(L"opengl32.dll", "glLoadIdentity"),
                                  reinterpret_cast<void*>(hk_gl_load_identity));

    ctx.hooks.glu_look_at =
        safetyhook::create_inline(get_addr(L"glu32.dll", "gluLookAt"),
                                  reinterpret_cast<void*>(hk_glu_look_at));
}

void uninstall_hooks()
{
    ctx.should_unload.store(true);
    if (ctx.window && ctx.original_wnd_proc)
        SetWindowLongPtrA(
            ctx.window, GWLP_WNDPROC, (LONG_PTR)ctx.original_wnd_proc);

    ctx.hooks.reset();
}

// ------------------------------------------------------------------------------------------------
// UI
// ------------------------------------------------------------------------------------------------

static void render_overlay()
{
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();

    ImGui::SetNextWindowPos({ 20, 20 }, ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize({ 420, 360 }, ImGuiCond_FirstUseEver);

    if (ImGui::Begin("SafetyHook Cam", nullptr, ImGuiWindowFlags_NoCollapse))
    {
        bool enabled = ctx.cam.enabled.load();
        if (ImGui::Checkbox("Master Enable", &enabled))
            ctx.cam.enabled.store(enabled);

        ImGui::SameLine();
        ImGui::TextColored(enabled ? ImVec4(0, 1, 0, 1) : ImVec4(1, 0, 0, 1),
                           "[%s]",
                           enabled ? "ACTIVE" : "OFF");

        if (enabled)
        {
            ImGui::SeparatorText("Settings");
            float sens = ctx.cam.sensitivity.load();
            if (ImGui::DragFloat("Sensitivity", &sens, 0.005f, 0.01f, 2.0f))
                ctx.cam.sensitivity.store(sens);

            float speed = ctx.cam.base_speed.load();
            if (ImGui::DragFloat("Speed", &speed, 0.5f, 0.1f, 1000.0f))
                ctx.cam.base_speed.store(speed);

            float mult = ctx.cam.sprint_mult.load();
            if (ImGui::DragFloat("Sprint Mult", &mult, 0.1f, 1.0f, 50.0f))
                ctx.cam.sprint_mult.store(mult);

            ImGui::SeparatorText("Injection");
            bool f_id = ctx.cam.hook_identity.load();
            if (ImGui::Checkbox("Hook Identity", &f_id))
                ctx.cam.hook_identity.store(f_id);

            ImGui::SeparatorText("Telemetry");
            ImGui::Text("Pos: %.2f %.2f %.2f",
                        ctx.cam.pos.x,
                        ctx.cam.pos.y,
                        ctx.cam.pos.z);
            ImGui::Text("Rot: %.1f / %.1f", ctx.cam.rot.x, ctx.cam.rot.y);

            if (ImGui::Button("Reset Origin", { 120, 0 }))
            {
                ctx.cam.pos = { 0.0, 10.0, 0.0 };
                ctx.cam.rot = { -90.0f, 0.0f };
            }
        }

        ImGui::Separator();
        ImGui::TextDisabled("Press [INSERT] for Cinematic Mode");

        if (ImGui::Button("Unload DLL", { 120, 0 }))
        {
            std::thread([] { uninstall_hooks(); }).detach();
        }
    }
    ImGui::End();
    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
}
