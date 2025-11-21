#define WIN32_LEAN_AND_MEAN
#define _CRT_SECURE_NO_WARNINGS

#include "bass_proxy.hpp"
#include <GL/gl.h>
#include <GL/glu.h>
#include <MinHook.h>
#include <atomic>
#include <cmath>
#include <imgui.h>
#include <imgui_impl_opengl3.h>
#include <imgui_impl_win32.h>
#include <mutex>
#include <thread>
#include <windows.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// ------------------------------------------------------------------------------------------------
// GLOBALS & TYPES
// ------------------------------------------------------------------------------------------------

using wgl_swap_t         = BOOL(WINAPI*)(HDC);
using gl_load_identity_t = void(APIENTRY*)();
using gl_load_matrix_f_t = void(APIENTRY*)(const GLfloat*);
using gl_load_matrix_d_t = void(APIENTRY*)(const GLdouble*);
using glu_look_at_t      = void(APIENTRY*)(GLdouble,
                                      GLdouble,
                                      GLdouble,
                                      GLdouble,
                                      GLdouble,
                                      GLdouble,
                                      GLdouble,
                                      GLdouble,
                                      GLdouble);

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND,
                                                             UINT,
                                                             WPARAM,
                                                             LPARAM);

struct free_camera_settings final
{
    std::atomic<bool> enabled{ true };
    std::atomic<bool> mouse_look_active{ false };

    // Standard hooks enabled
    std::atomic<bool> force_on_identity{ true };
    std::atomic<bool> force_on_matrix_load{ true };

    std::atomic<float> base_speed{ 20.0f };
    std::atomic<float> sprint_multiplier{ 4.0f };
    std::atomic<float> mouse_sensitivity{ 0.15f };

    double pos_x{ 0.0 }, pos_y{ 10.0 }, pos_z{ 0.0 };
    float  yaw{ -90.0f }, pitch{ 0.0f };

    // For cursor restoration
    POINT saved_cursor_pos{ 0, 0 };
};

struct global_context final
{
    HWND              game_window{ nullptr };
    WNDPROC           orig_wnd_proc{ nullptr };
    std::atomic<bool> imgui_ready{ false };
    std::atomic<bool> shutting_down{ false };
    std::atomic<bool> overlay_visible{ true };

    free_camera_settings free_cam;
    GLenum               current_matrix_mode{ GL_MODELVIEW };
};

static global_context ctx;

// Original Pointers
static wgl_swap_t         orig_wgl_swap            = nullptr;
static gl_load_identity_t orig_gl_load_identity    = nullptr;
static gl_load_matrix_f_t orig_gl_load_matrix_f    = nullptr;
static gl_load_matrix_d_t orig_gl_load_matrix_d    = nullptr;
static glu_look_at_t      orig_glu_look_at         = nullptr;
static void(APIENTRY* orig_gl_matrix_mode)(GLenum) = nullptr;

static void             draw_ui();
static LRESULT CALLBACK detour_wnd_proc(HWND, UINT, WPARAM, LPARAM);

// ------------------------------------------------------------------------------------------------
// CAMERA & INPUT LOGIC
// ------------------------------------------------------------------------------------------------

static void handle_mouse_input()
{
    if (!ctx.free_cam.mouse_look_active.load(std::memory_order_relaxed))
        return;

    // 1. Get Center of Window
    RECT rect;
    GetWindowRect(ctx.game_window, &rect);
    int center_x = (rect.left + rect.right) / 2;
    int center_y = (rect.top + rect.bottom) / 2;

    // 2. Get Current Delta
    POINT cur_pos;
    GetCursorPos(&cur_pos);

    float delta_x = static_cast<float>(cur_pos.x - center_x);
    float delta_y = static_cast<float>(cur_pos.y - center_y);

    // 3. Apply Rotation only if we moved
    if (delta_x != 0.0f || delta_y != 0.0f)
    {
        float sens =
            ctx.free_cam.mouse_sensitivity.load(std::memory_order_relaxed);

        ctx.free_cam.yaw += delta_x * sens;
        ctx.free_cam.pitch -=
            delta_y * sens; // Subtract Y to invert mouse (standard FPS feel)

        // Clamp Pitch
        if (ctx.free_cam.pitch > 89.0f)
            ctx.free_cam.pitch = 89.0f;
        if (ctx.free_cam.pitch < -89.0f)
            ctx.free_cam.pitch = -89.0f;

        // Wrap Yaw
        while (ctx.free_cam.yaw > 360.0f)
            ctx.free_cam.yaw -= 360.0f;
        while (ctx.free_cam.yaw < -360.0f)
            ctx.free_cam.yaw += 360.0f;

        // 4. FORCE RESET CURSOR TO CENTER
        // This creates the "infinite" movement and prevents hitting screen
        // edges
        SetCursorPos(center_x, center_y);
    }
}

static void update_free_camera()
{
    if (!ctx.free_cam.enabled.load(std::memory_order_relaxed))
        return;

    // Handle mouse rotation first
    handle_mouse_input();

    // Calculate vectors
    double rad_yaw   = ctx.free_cam.yaw * M_PI / 180.0;
    double rad_pitch = ctx.free_cam.pitch * M_PI / 180.0;

    double front_x = cos(rad_yaw) * cos(rad_pitch);
    double front_y = sin(rad_pitch);
    double front_z = sin(rad_yaw) * cos(rad_pitch);

    double len =
        sqrt(front_x * front_x + front_y * front_y + front_z * front_z);
    if (len > 0.0)
    {
        front_x /= len;
        front_y /= len;
        front_z /= len;
    }

    double right_x = cos(rad_yaw + M_PI / 2.0);
    double right_z = sin(rad_yaw + M_PI / 2.0);

    // Movement Speed
    float speed = ctx.free_cam.base_speed.load(std::memory_order_relaxed);
    if (GetAsyncKeyState(VK_SHIFT) & 0x8000)
    {
        speed *= ctx.free_cam.sprint_multiplier.load(std::memory_order_relaxed);
    }

    // Frame-rate independent movement
    float fps = ImGui::GetIO().Framerate;
    if (fps > 0.0f)
        speed /= fps;

    // Keyboard Input
    if (GetAsyncKeyState('W') & 0x8000)
    {
        ctx.free_cam.pos_x += front_x * speed;
        ctx.free_cam.pos_y += front_y * speed;
        ctx.free_cam.pos_z += front_z * speed;
    }
    if (GetAsyncKeyState('S') & 0x8000)
    {
        ctx.free_cam.pos_x -= front_x * speed;
        ctx.free_cam.pos_y -= front_y * speed;
        ctx.free_cam.pos_z -= front_z * speed;
    }
    if (GetAsyncKeyState('D') & 0x8000)
    {
        ctx.free_cam.pos_x += right_x * speed;
        ctx.free_cam.pos_z += right_z * speed;
    }
    if (GetAsyncKeyState('A') & 0x8000)
    {
        ctx.free_cam.pos_x -= right_x * speed;
        ctx.free_cam.pos_z -= right_z * speed;
    }
    if (GetAsyncKeyState(VK_SPACE) & 0x8000)
        ctx.free_cam.pos_y += speed;
    if (GetAsyncKeyState(VK_CONTROL) & 0x8000)
        ctx.free_cam.pos_y -= speed;
}

static void apply_camera_transform()
{
    double rad_yaw   = ctx.free_cam.yaw * M_PI / 180.0;
    double rad_pitch = ctx.free_cam.pitch * M_PI / 180.0;

    double cx = ctx.free_cam.pos_x;
    double cy = ctx.free_cam.pos_y;
    double cz = ctx.free_cam.pos_z;

    double lx = cx + cos(rad_yaw) * cos(rad_pitch);
    double ly = cy + sin(rad_pitch);
    double lz = cz + sin(rad_yaw) * cos(rad_pitch);

    if (orig_glu_look_at)
    {
        orig_glu_look_at(cx, cy, cz, lx, ly, lz, 0.0, 1.0, 0.0);
    }
}

// ------------------------------------------------------------------------------------------------
// HOOKS
// ------------------------------------------------------------------------------------------------

static void APIENTRY detour_gl_matrix_mode(GLenum mode)
{
    ctx.current_matrix_mode = mode;
    orig_gl_matrix_mode(mode);
}

static void APIENTRY detour_gl_load_identity()
{
    orig_gl_load_identity();
    if (ctx.free_cam.enabled.load(std::memory_order_relaxed) &&
        ctx.free_cam.force_on_identity.load() &&
        ctx.current_matrix_mode == GL_MODELVIEW)
    {
        apply_camera_transform();
    }
}

static void APIENTRY detour_gl_load_matrix_f(const GLfloat* m)
{
    if (ctx.free_cam.enabled.load(std::memory_order_relaxed) &&
        ctx.free_cam.force_on_matrix_load.load() &&
        ctx.current_matrix_mode == GL_MODELVIEW)
    {
        orig_gl_load_identity();
        apply_camera_transform();
        return;
    }
    orig_gl_load_matrix_f(m);
}

static void APIENTRY detour_gl_load_matrix_d(const GLdouble* m)
{
    if (ctx.free_cam.enabled.load(std::memory_order_relaxed) &&
        ctx.free_cam.force_on_matrix_load.load() &&
        ctx.current_matrix_mode == GL_MODELVIEW)
    {
        orig_gl_load_identity();
        apply_camera_transform();
        return;
    }
    orig_gl_load_matrix_d(m);
}

static void APIENTRY detour_glu_look_at(GLdouble eyeX,
                                        GLdouble eyeY,
                                        GLdouble eyeZ,
                                        GLdouble centerX,
                                        GLdouble centerY,
                                        GLdouble centerZ,
                                        GLdouble upX,
                                        GLdouble upY,
                                        GLdouble upZ)
{
    if (ctx.free_cam.enabled.load(std::memory_order_relaxed))
    {
        apply_camera_transform();
    }
    else
    {
        orig_glu_look_at(
            eyeX, eyeY, eyeZ, centerX, centerY, centerZ, upX, upY, upZ);
    }
}

static BOOL WINAPI detour_wgl_swap(HDC dc)
{
    if (ctx.imgui_ready.load(std::memory_order_acquire))
    {
        update_free_camera();
        if (ctx.overlay_visible.load(std::memory_order_relaxed))
        {
            draw_ui();
        }
    }

    static std::once_flag init_flag;
    if (wglGetCurrentContext())
    {
        std::call_once(
            init_flag,
            [&]()
            {
                ctx.game_window = WindowFromDC(dc);
                if (ctx.game_window)
                {
                    ctx.orig_wnd_proc =
                        (WNDPROC)SetWindowLongPtrA(ctx.game_window,
                                                   GWLP_WNDPROC,
                                                   (LONG_PTR)detour_wnd_proc);
                    ImGui::CreateContext();
                    ImGuiIO& io = ImGui::GetIO();
                    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
                    io.IniFilename = nullptr;
                    ImGui::StyleColorsDark();
                    ImGui_ImplWin32_Init(ctx.game_window);
                    ImGui_ImplOpenGL3_Init("#version 110");
                    SetWindowTextA(ctx.game_window,
                                   "Airstrike 3D II [FREECAM]");
                    ctx.imgui_ready.store(true, std::memory_order_release);
                }
            });
    }
    return orig_wgl_swap(dc);
}

static LRESULT CALLBACK detour_wnd_proc(HWND h, UINT m, WPARAM w, LPARAM l)
{
    if (m == WM_KEYDOWN && w == VK_INSERT)
    {
        ctx.overlay_visible.store(!ctx.overlay_visible.load());
        return 0;
    }

    // Simplified Window Proc - Mouse logic is now handled in update loop for
    // smoothness
    if (ctx.free_cam.enabled.load())
    {
        if (m == WM_RBUTTONDOWN)
        {
            // 1. Save Cursor Pos
            GetCursorPos(&ctx.free_cam.saved_cursor_pos);

            // 2. Center cursor immediately to prepare for loop
            RECT rect;
            GetWindowRect(h, &rect);
            int cx = (rect.left + rect.right) / 2;
            int cy = (rect.top + rect.bottom) / 2;
            SetCursorPos(cx, cy);

            // 3. Activate
            ctx.free_cam.mouse_look_active = true;
            ShowCursor(FALSE);
            return 0;
        }
        if (m == WM_RBUTTONUP)
        {
            ctx.free_cam.mouse_look_active = false;

            // 4. Restore Cursor Pos (Usability upgrade)
            SetCursorPos(ctx.free_cam.saved_cursor_pos.x,
                         ctx.free_cam.saved_cursor_pos.y);

            ShowCursor(TRUE);
            return 0;
        }
    }

    if (!ctx.shutting_down && ctx.overlay_visible &&
        ImGui_ImplWin32_WndProcHandler(h, m, w, l))
        return true;
    return CallWindowProc(ctx.orig_wnd_proc, h, m, w, l);
}

template <typename FuncT>
static bool hook_safe(LPCWSTR mod, LPCSTR name, void* detour, FuncT** orig)
{
    if (MH_CreateHookApi(mod, name, detour, (void**)orig) != MH_OK)
        return false;
    return true;
}

void install_hooks()
{
    MH_Initialize();
    hook_safe(L"opengl32.dll",
              "wglSwapBuffers",
              (void*)detour_wgl_swap,
              &orig_wgl_swap);
    hook_safe(L"opengl32.dll",
              "glMatrixMode",
              (void*)detour_gl_matrix_mode,
              &orig_gl_matrix_mode);
    hook_safe(L"opengl32.dll",
              "glLoadIdentity",
              (void*)detour_gl_load_identity,
              &orig_gl_load_identity);
    hook_safe(L"opengl32.dll",
              "glLoadMatrixf",
              (void*)detour_gl_load_matrix_f,
              &orig_gl_load_matrix_f);
    hook_safe(L"opengl32.dll",
              "glLoadMatrixd",
              (void*)detour_gl_load_matrix_d,
              &orig_gl_load_matrix_d);
    hook_safe(L"glu32.dll",
              "gluLookAt",
              (void*)detour_glu_look_at,
              &orig_glu_look_at);
    MH_EnableHook(MH_ALL_HOOKS);
}

void uninstall_hooks()
{
    ctx.shutting_down = true;
    if (ctx.game_window)
        SetWindowLongPtrA(
            ctx.game_window, GWLP_WNDPROC, (LONG_PTR)ctx.orig_wnd_proc);
    MH_DisableHook(MH_ALL_HOOKS);
    MH_Uninitialize();
}

static void draw_ui()
{
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();

    ImGui::SetNextWindowPos(ImVec2(20, 20), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(420, 380), ImGuiCond_FirstUseEver);

    if (ImGui::Begin("Camera Control", nullptr, ImGuiWindowFlags_NoCollapse))
    {

        bool enabled = ctx.free_cam.enabled.load();
        if (ImGui::Checkbox("Master Enable", &enabled))
            ctx.free_cam.enabled.store(enabled);

        ImGui::Separator();
        ImGui::TextColored(
            ImVec4(0, 1, 0, 1), "Status: %s", enabled ? "ACTIVE" : "DISABLED");

        if (enabled)
        {
            ImGui::SeparatorText("Input");
            float sens = ctx.free_cam.mouse_sensitivity.load();
            if (ImGui::DragFloat(
                    "Mouse Sensitivity", &sens, 0.01f, 0.01f, 2.0f))
            {
                ctx.free_cam.mouse_sensitivity.store(sens);
            }

            float speed = ctx.free_cam.base_speed.load();
            if (ImGui::DragFloat(
                    "Base Speed", &speed, 0.5f, 0.1f, 500.0f, "%.1f"))
            {
                ctx.free_cam.base_speed.store(speed);
            }

            float mult = ctx.free_cam.sprint_multiplier.load();
            if (ImGui::DragFloat(
                    "Sprint (Shift)", &mult, 0.1f, 1.0f, 20.0f, "%.1fx"))
            {
                ctx.free_cam.sprint_multiplier.store(mult);
            }

            ImGui::SeparatorText("Hooks");
            bool f_id = ctx.free_cam.force_on_identity.load();
            if (ImGui::Checkbox("Force on Identity", &f_id))
                ctx.free_cam.force_on_identity.store(f_id);

            bool f_mx = ctx.free_cam.force_on_matrix_load.load();
            if (ImGui::Checkbox("Force on Matrix Load", &f_mx))
                ctx.free_cam.force_on_matrix_load.store(f_mx);

            ImGui::SeparatorText("Position");
            ImGui::Text(
                "X: %8.2f  Yaw:   %.1f", ctx.free_cam.pos_x, ctx.free_cam.yaw);
            ImGui::Text("Y: %8.2f  Pitch: %.1f",
                        ctx.free_cam.pos_y,
                        ctx.free_cam.pitch);
            ImGui::Text("Z: %8.2f", ctx.free_cam.pos_z);

            if (ImGui::Button("Reset to Origin"))
            {
                ctx.free_cam.pos_x = 0;
                ctx.free_cam.pos_y = 10;
                ctx.free_cam.pos_z = 0;
            }
        }

        ImGui::Separator();
        ImGui::TextDisabled(
            "Controls:\n[WASD] Move  [Shift] Fast\n[Space/Ctrl] "
            "Up/Down\n[Right Click] Look Around (Locked)\n[Insert] Hide Menu");

        if (ImGui::Button("Unload DLL"))
            std::thread([] { uninstall_hooks(); }).detach();
    }
    ImGui::End();
    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
}