#define WIN32_LEAN_AND_MEAN
#define _CRT_SECURE_NO_WARNINGS

#include "bass_proxy.hpp"

#include <MinHook.h>

#include <GL/gl.h>
#include <imgui.h>
#include <imgui_impl_opengl3.h>
#include <imgui_impl_win32.h>

#include <windows.h>
#include <psapi.h>

#include <array>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <format>
#include <fstream>
#include <mutex>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

using wgl_swap_t         = BOOL(WINAPI*)(HDC);
using qpc_t              = BOOL(WINAPI*)(LARGE_INTEGER*);
using set_cursor_t       = BOOL(WINAPI*)(int, int);
using sleep_t            = VOID(WINAPI*)(DWORD);
using create_file_a_t    = HANDLE(WINAPI*)(LPCSTR, DWORD, DWORD, LPSECURITY_ATTRIBUTES, DWORD, DWORD, HANDLE);
using rand_t             = int(__cdecl*)();
using gl_draw_elems_t    = void(APIENTRY*)(GLenum, GLsizei, GLenum, const GLvoid*);
using gl_viewport_t      = void(APIENTRY*)(GLint, GLint, GLsizei, GLsizei);
using gl_clear_t         = void(APIENTRY*)(GLbitfield);
using gl_ortho_t         = void(APIENTRY*)(GLdouble, GLdouble, GLdouble, GLdouble, GLdouble, GLdouble);
using gl_frustum_t       = void(APIENTRY*)(GLdouble, GLdouble, GLdouble, GLdouble, GLdouble, GLdouble);
using gl_matrix_mode_t   = void(APIENTRY*)(GLenum);
using gl_load_identity_t = void(APIENTRY*)();

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND, UINT, WPARAM, LPARAM);

struct visual_settings final {
    std::atomic<bool> disable_depth{ false };
    std::atomic<bool> wireframe{ false };
    std::atomic<bool> fog_override{ false };
    
    ImVec4 clear_color{ 0.0f, 0.0f, 0.0f, 0.0f };
    ImVec4 fog_color{ 0.5f, 0.6f, 0.7f, 1.0f };
    bool   enable_clear{ false };
};

struct render_settings final {
    std::atomic<int>  render_scale{ 100 };
    std::atomic<bool> log_matrices{ false };
    
    GLdouble last_frustum[6]{ 0 };
    GLdouble last_ortho[6]{ 0 };
    bool     has_frustum{ false };
    bool     has_ortho{ false };
};

struct gameplay_settings final {
    std::atomic<float> speed_multiplier{ 1.0f };
    std::atomic<bool>  block_mouse{ false };
    std::atomic<bool>  no_sleep{ false };
    std::atomic<bool>  freeze_rng{ false };
};

struct debug_settings final {
    std::atomic<bool> log_fs{ false };
    std::atomic<bool> log_gl_calls{ false };
    std::atomic<bool> show_metrics{ false };
    std::atomic<bool> show_demo{ false };
};

struct global_context final {
    HWND              game_window{ nullptr };
    WNDPROC           orig_wnd_proc{ nullptr };
    std::atomic<bool> imgui_ready{ false };
    std::atomic<bool> shutting_down{ false };
    std::atomic<bool> overlay_visible{ true };
    
    visual_settings   visuals;
    render_settings   render;
    gameplay_settings gameplay;
    debug_settings    debug;
    
    std::atomic<int> frame_count{ 0 };
    std::atomic<int> draw_call_count{ 0 };
    
    GLint  current_viewport[4]{ 0, 0, 800, 600 };
    GLenum current_matrix_mode{ GL_MODELVIEW };
};

static global_context ctx;

class logger final {
public:
    logger() {
        log_file.open("bass_proxy_log.txt", std::ios::out | std::ios::trunc);
    }

    ~logger() {
        if (log_file.is_open()) {
            log_file.close();
        }
    }

    template <typename... Args>
    void log(std::format_string<Args...> fmt, Args&&... args) {
        try {
            const std::string msg = std::format(fmt, std::forward<Args>(args)...);
            auto              now = std::chrono::system_clock::now();
            auto              sys_time = std::chrono::system_clock::to_time_t(now);
            std::tm           local_tm{};
#ifdef _WIN32
            localtime_s(&local_tm, &sys_time);
#else
            localtime_r(&sys_time, &local_tm);
#endif

            const std::string timestamped = std::format(
                "[{:02}:{:02}:{:02}] {}",
                local_tm.tm_hour,
                local_tm.tm_min,
                local_tm.tm_sec,
                msg);

            {
                std::scoped_lock lock(file_mtx);
                if (log_file.is_open()) {
                    log_file << timestamped << std::endl;
                }
            }

            {
                std::scoped_lock lock(console_mtx);
                console_buf.append(timestamped.c_str());
                console_buf.append("\n");
                should_scroll = true;
            }
        }
        catch (...) {
        }
    }

    void draw_console() {
        std::scoped_lock lock(console_mtx);

        if (ImGui::Button("Clear")) {
            console_buf.clear();
        }
        ImGui::SameLine();
        if (ImGui::Button("Copy All")) {
            ImGui::SetClipboardText(console_buf.c_str());
        }
        ImGui::SameLine();
        filter.Draw("Filter", -100.0f);
        ImGui::Separator();

        if (ImGui::BeginChild("LogScroll", ImVec2(0, 0), false, ImGuiWindowFlags_HorizontalScrollbar)) {
            if (filter.IsActive()) {
                const char* buf_begin = console_buf.begin();
                const char* line      = buf_begin;
                while (line != nullptr) {
                    const char* line_end = strchr(line, '\n');
                    if (filter.PassFilter(line, line_end)) {
                        ImGui::TextUnformatted(line, line_end);
                    }
                    line = (line_end && line_end[1]) ? line_end + 1 : nullptr;
                }
            }
            else {
                ImGui::TextUnformatted(console_buf.begin());
            }

            if (should_scroll && ImGui::GetScrollY() >= ImGui::GetScrollMaxY()) {
                ImGui::SetScrollHereY(1.0f);
                should_scroll = false;
            }
        }
        ImGui::EndChild();
    }

private:
    std::ofstream   log_file;
    std::mutex      file_mtx;
    std::mutex      console_mtx;
    ImGuiTextBuffer console_buf;
    ImGuiTextFilter filter;
    bool            should_scroll = true;
};

static logger logger;

static wgl_swap_t         orig_wgl_swap         = nullptr;
static qpc_t              orig_qpc              = nullptr;
static set_cursor_t       orig_set_cursor       = nullptr;
static sleep_t            orig_sleep            = nullptr;
static create_file_a_t    orig_create_file_a    = nullptr;
static rand_t             orig_rand             = nullptr;
static gl_draw_elems_t    orig_gl_draw_elems    = nullptr;
static gl_viewport_t      orig_gl_viewport      = nullptr;
static gl_clear_t         orig_gl_clear         = nullptr;
static gl_ortho_t         orig_gl_ortho         = nullptr;
static gl_frustum_t       orig_gl_frustum       = nullptr;
static gl_matrix_mode_t   orig_gl_matrix_mode   = nullptr;
static gl_load_identity_t orig_gl_load_identity = nullptr;

static void             draw_ui();
static LRESULT CALLBACK detour_wnd_proc(HWND h, UINT m, WPARAM w, LPARAM l);

static void APIENTRY detour_gl_load_identity() {
    if (ctx.render.log_matrices.load(std::memory_order_relaxed)) {
        logger.log("matrix => load_identity (mode={})", 
            ctx.current_matrix_mode == GL_PROJECTION ? "PROJECTION" : 
            ctx.current_matrix_mode == GL_MODELVIEW ? "MODELVIEW" : "OTHER");
    }
    orig_gl_load_identity();
}

static void APIENTRY detour_gl_frustum(GLdouble left, GLdouble right, GLdouble bottom, GLdouble top, GLdouble near_val, GLdouble far_val) {
    ctx.render.last_frustum[0] = left;
    ctx.render.last_frustum[1] = right;
    ctx.render.last_frustum[2] = bottom;
    ctx.render.last_frustum[3] = top;
    ctx.render.last_frustum[4] = near_val;
    ctx.render.last_frustum[5] = far_val;
    ctx.render.has_frustum = true;
    
    if (ctx.render.log_matrices.load(std::memory_order_relaxed)) {
        logger.log("frustum => l={:.2f} r={:.2f} b={:.2f} t={:.2f} n={:.2f} f={:.2f}",
            left, right, bottom, top, near_val, far_val);
    }
    
    orig_gl_frustum(left, right, bottom, top, near_val, far_val);
}

static void APIENTRY detour_gl_ortho(GLdouble left, GLdouble right, GLdouble bottom, GLdouble top, GLdouble near_val, GLdouble far_val) {
    ctx.render.last_ortho[0] = left;
    ctx.render.last_ortho[1] = right;
    ctx.render.last_ortho[2] = bottom;
    ctx.render.last_ortho[3] = top;
    ctx.render.last_ortho[4] = near_val;
    ctx.render.last_ortho[5] = far_val;
    ctx.render.has_ortho = true;
    
    if (ctx.render.log_matrices.load(std::memory_order_relaxed)) {
        logger.log("ortho => l={:.2f} r={:.2f} b={:.2f} t={:.2f} n={:.2f} f={:.2f}",
            left, right, bottom, top, near_val, far_val);
    }
    
    orig_gl_ortho(left, right, bottom, top, near_val, far_val);
}

static void APIENTRY detour_gl_matrix_mode(GLenum mode) {
    ctx.current_matrix_mode = mode;
    
    if (ctx.render.log_matrices.load(std::memory_order_relaxed)) {
        logger.log("matrix => mode changed to {}",
            mode == GL_PROJECTION ? "PROJECTION" :
            mode == GL_MODELVIEW ? "MODELVIEW" :
            mode == GL_TEXTURE ? "TEXTURE" : "OTHER");
    }
    
    orig_gl_matrix_mode(mode);
}

static BOOL WINAPI detour_wgl_swap(HDC dc) {
    ctx.frame_count.fetch_add(1, std::memory_order_relaxed);
    
    if (ctx.imgui_ready.load(std::memory_order_acquire)) {
        if (ImGui::GetCurrentContext()) {
            ctx.gameplay.block_mouse.store(ImGui::GetIO().WantCaptureMouse, std::memory_order_relaxed);
        }
        if (ctx.overlay_visible.load(std::memory_order_relaxed)) {
            draw_ui();
        }
    }

    static std::once_flag init_flag;
    if (wglGetCurrentContext()) {
        std::call_once(init_flag, [&]() {
            logger.log("imgui => initializing context");
            ctx.game_window = WindowFromDC(dc);
            if (ctx.game_window) {
                ctx.orig_wnd_proc = reinterpret_cast<WNDPROC>(
                    SetWindowLongPtrA(ctx.game_window, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(detour_wnd_proc)));

                ImGui::CreateContext();
                ImGuiIO& io        = ImGui::GetIO();
                io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
                io.FontGlobalScale = 1.15f;
                io.IniFilename     = "bass_proxy_imgui.ini";

                ImGui::StyleColorsDark();
                ImGuiStyle& style = ImGui::GetStyle();
                style.WindowRounding = 6.0f;
                style.FrameRounding  = 4.0f;
                style.GrabRounding   = 3.0f;
                style.WindowBorderSize = 1.0f;
                style.FrameBorderSize  = 0.0f;
                
                ImGui_ImplWin32_Init(ctx.game_window);
                ImGui_ImplOpenGL3_Init("#version 110");

                SetWindowTextA(ctx.game_window, "Airstrike 3D II [ENHANCED]");
                
                ctx.imgui_ready.store(true, std::memory_order_release);
                logger.log("imgui => initialization complete");
            }
        });
    }
    
    ctx.draw_call_count.store(0, std::memory_order_relaxed);
    return orig_wgl_swap(dc);
}

static BOOL WINAPI detour_qpc(LARGE_INTEGER* lp_performance_count) {
    if (!orig_qpc || !orig_qpc(lp_performance_count)) {
        return FALSE;
    }

    static LARGE_INTEGER last_real = {};
    static LARGE_INTEGER last_fake = {};
    static bool          first     = true;
    static std::mutex    qpc_mtx;

    const float mult = ctx.gameplay.speed_multiplier.load(std::memory_order_relaxed);

    if (std::abs(mult - 1.0f) < 0.0001f && !first) {
        return TRUE;
    }

    std::scoped_lock lock(qpc_mtx);
    if (first) {
        last_real = *lp_performance_count;
        last_fake = *lp_performance_count;
        first     = false;
        return TRUE;
    }

    const LONGLONG diff = lp_performance_count->QuadPart - last_real.QuadPart;
    last_real           = *lp_performance_count;

    const double scaled_diff = static_cast<double>(diff) * static_cast<double>(mult);
    last_fake.QuadPart += static_cast<LONGLONG>(scaled_diff);

    *lp_performance_count = last_fake;
    return TRUE;
}

static BOOL WINAPI detour_set_cursor(int x, int y) {
    if (ctx.gameplay.block_mouse.load(std::memory_order_relaxed)) {
        return TRUE;
    }
    return orig_set_cursor(x, y);
}

static VOID WINAPI detour_sleep(DWORD dw_milliseconds) {
    if (ctx.gameplay.no_sleep.load(std::memory_order_relaxed)) {
        return orig_sleep(0);
    }
    return orig_sleep(dw_milliseconds);
}

static HANDLE WINAPI detour_create_file_a(
    LPCSTR file_name, DWORD access, DWORD share, LPSECURITY_ATTRIBUTES sec, DWORD disp, DWORD attr, HANDLE temp) {
    if (ctx.debug.log_fs.load(std::memory_order_relaxed)) {
        logger.log("fs => \"{}\"", file_name ? file_name : "NULL");
    }
    return orig_create_file_a(file_name, access, share, sec, disp, attr, temp);
}

static int __cdecl detour_rand() {
    if (ctx.gameplay.freeze_rng.load(std::memory_order_relaxed)) {
        return 42;
    }
    return orig_rand();
}

static void APIENTRY detour_gl_draw_elems(GLenum mode, GLsizei count, GLenum type, const GLvoid* indices) {
    ctx.draw_call_count.fetch_add(1, std::memory_order_relaxed);
    
    if (ctx.debug.log_gl_calls.load(std::memory_order_relaxed)) {
        logger.log("gl => draw_elements(mode={}, count={})", mode, count);
    }
    
    bool depth = ctx.visuals.disable_depth.load(std::memory_order_relaxed);
    if (depth) {
        glDisable(GL_DEPTH_TEST);
        glDepthMask(GL_FALSE);
    }

    if (ctx.visuals.wireframe.load(std::memory_order_relaxed)) {
        glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
    }

    orig_gl_draw_elems(mode, count, type, indices);

    if (depth) {
        glEnable(GL_DEPTH_TEST);
        glDepthMask(GL_TRUE);
    }
    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
}

static void APIENTRY detour_gl_viewport(GLint x, GLint y, GLsizei width, GLsizei height) {
    ctx.current_viewport[0] = x;
    ctx.current_viewport[1] = y;
    ctx.current_viewport[2] = width;
    ctx.current_viewport[3] = height;
    
    const int scale = ctx.render.render_scale.load(std::memory_order_relaxed);
    if (scale != 100 && scale > 0) {
        width  = (width * scale) / 100;
        height = (height * scale) / 100;
        
        if (ctx.render.log_matrices.load(std::memory_order_relaxed)) {
            logger.log("viewport => scaled to {}x{} ({}%)", width, height, scale);
        }
    }
    
    orig_gl_viewport(x, y, width, height);
}

static void APIENTRY detour_gl_clear(GLbitfield mask) {
    if (ctx.visuals.enable_clear) {
        glClearColor(
            ctx.visuals.clear_color.x,
            ctx.visuals.clear_color.y,
            ctx.visuals.clear_color.z,
            ctx.visuals.clear_color.w);
    }
    
    if (ctx.visuals.fog_override.load(std::memory_order_relaxed)) {
        GLfloat fog_col[] = { 
            ctx.visuals.fog_color.x, 
            ctx.visuals.fog_color.y, 
            ctx.visuals.fog_color.z, 
            ctx.visuals.fog_color.w 
        };
        glFogfv(GL_FOG_COLOR, fog_col);
        glFogi(GL_FOG_MODE, GL_LINEAR);
        glFogf(GL_FOG_START, 10.0f);
        glFogf(GL_FOG_END, 100.0f);
        glEnable(GL_FOG);
    }
    
    orig_gl_clear(mask);
}

static LRESULT CALLBACK detour_wnd_proc(HWND h, UINT m, WPARAM w, LPARAM l) {
    if (m == WM_KEYDOWN && w == VK_INSERT) {
        ctx.overlay_visible.store(!ctx.overlay_visible.load(), std::memory_order_relaxed);
        return 0;
    }
    
    if (!ctx.shutting_down.load() && ctx.overlay_visible.load() && ImGui_ImplWin32_WndProcHandler(h, m, w, l)) {
        return true;
    }
    return CallWindowProc(ctx.orig_wnd_proc, h, m, w, l);
}

template <typename FuncT>
static bool create_hook_checked(LPCWSTR module, LPCSTR func_name, void* detour, FuncT** orig) {
    const MH_STATUS status = MH_CreateHookApi(module, func_name, detour, reinterpret_cast<LPVOID*>(orig));
    if (status != MH_OK) {
        logger.log("minhook => create_hook failed: {} ({})", func_name, MH_StatusToString(status));
        return false;
    }
    logger.log("minhook => created hook: {}", func_name);
    return true;
}

void install_hooks() {
    logger.log("minhook => initializing library");
    
    MH_STATUS init_status = MH_Initialize();
    if (init_status != MH_OK) {
        logger.log("minhook => MH_Initialize failed: {}", MH_StatusToString(init_status));
        return;
    }

    bool all_ok = true;
    all_ok &= create_hook_checked(L"opengl32.dll", "wglSwapBuffers", (void*)detour_wgl_swap, &orig_wgl_swap);
    all_ok &= create_hook_checked(L"opengl32.dll", "glDrawElements", (void*)detour_gl_draw_elems, &orig_gl_draw_elems);
    all_ok &= create_hook_checked(L"opengl32.dll", "glViewport", (void*)detour_gl_viewport, &orig_gl_viewport);
    all_ok &= create_hook_checked(L"opengl32.dll", "glClear", (void*)detour_gl_clear, &orig_gl_clear);
    all_ok &= create_hook_checked(L"opengl32.dll", "glOrtho", (void*)detour_gl_ortho, &orig_gl_ortho);
    all_ok &= create_hook_checked(L"opengl32.dll", "glFrustum", (void*)detour_gl_frustum, &orig_gl_frustum);
    all_ok &= create_hook_checked(L"opengl32.dll", "glMatrixMode", (void*)detour_gl_matrix_mode, &orig_gl_matrix_mode);
    all_ok &= create_hook_checked(L"opengl32.dll", "glLoadIdentity", (void*)detour_gl_load_identity, &orig_gl_load_identity);
    all_ok &= create_hook_checked(L"kernel32.dll", "QueryPerformanceCounter", (void*)detour_qpc, &orig_qpc);
    all_ok &= create_hook_checked(L"kernel32.dll", "Sleep", (void*)detour_sleep, &orig_sleep);
    all_ok &= create_hook_checked(L"kernel32.dll", "CreateFileA", (void*)detour_create_file_a, &orig_create_file_a);
    all_ok &= create_hook_checked(L"user32.dll", "SetCursorPos", (void*)detour_set_cursor, &orig_set_cursor);
    all_ok &= create_hook_checked(L"msvcrt.dll", "rand", (void*)detour_rand, &orig_rand);

    if (!all_ok) {
        logger.log("minhook => some hooks failed, continuing anyway");
    }

    const MH_STATUS enable_status = MH_EnableHook(MH_ALL_HOOKS);
    if (enable_status != MH_OK) {
        logger.log("minhook => enable_hook failed: {}", MH_StatusToString(enable_status));
        return;
    }

    logger.log("minhook => all hooks enabled");
}

void uninstall_hooks() {
    logger.log("minhook => uninstall requested");
    ctx.shutting_down.store(true);

    if (ctx.game_window && ctx.orig_wnd_proc) {
        SetWindowLongPtrA(ctx.game_window, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(ctx.orig_wnd_proc));
        SetWindowTextA(ctx.game_window, "Airstrike 3D II");
    }

    const MH_STATUS disable_status = MH_DisableHook(MH_ALL_HOOKS);
    if (disable_status != MH_OK) {
        logger.log("minhook => disable_hook failed: {}", MH_StatusToString(disable_status));
    }

    const MH_STATUS uninit_status = MH_Uninitialize();
    if (uninit_status != MH_OK) {
        logger.log("minhook => uninitialize failed: {}", MH_StatusToString(uninit_status));
    }

    if (ctx.imgui_ready.exchange(false)) {
        ImGui_ImplOpenGL3_Shutdown();
        ImGui_ImplWin32_Shutdown();
        ImGui::DestroyContext();
    }

    logger.log("minhook => uninstallation complete");
}

static void draw_ui() {
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();

    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(ImVec2(viewport->WorkPos.x + 20, viewport->WorkPos.y + 20), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(550, 520), ImGuiCond_FirstUseEver);

    if (ImGui::Begin("Airstrike 3D II - Enhanced", nullptr, ImGuiWindowFlags_MenuBar)) {
        
        if (ImGui::BeginMenuBar()) {
            if (ImGui::BeginMenu("Menu")) {
                if (ImGui::MenuItem("Unload DLL")) {
                    std::thread([] { uninstall_hooks(); }).detach();
                }
                ImGui::Separator();
                if (ImGui::MenuItem("Exit Game")) {
                    PostQuitMessage(0);
                }
                ImGui::EndMenu();
            }
            
            if (ImGui::BeginMenu("View")) {
                bool show_metrics = ctx.debug.show_metrics.load();
                if (ImGui::MenuItem("Metrics", nullptr, &show_metrics)) {
                    ctx.debug.show_metrics.store(show_metrics);
                }
                bool show_demo = ctx.debug.show_demo.load();
                if (ImGui::MenuItem("ImGui Demo", nullptr, &show_demo)) {
                    ctx.debug.show_demo.store(show_demo);
                }
                ImGui::EndMenu();
            }
            
            ImGui::EndMenuBar();
        }

        if (ImGui::BeginTabBar("MainTabs", ImGuiTabBarFlags_None)) {
            
            if (ImGui::BeginTabItem("Visuals")) {
                ImGui::SeparatorText("Rendering");
                
                bool depth = ctx.visuals.disable_depth.load();
                if (ImGui::Checkbox("Disable Depth Test (Wallhack)", &depth)) {
                    ctx.visuals.disable_depth.store(depth);
                }

                bool wire = ctx.visuals.wireframe.load();
                if (ImGui::Checkbox("Wireframe Mode", &wire)) {
                    ctx.visuals.wireframe.store(wire);
                }

                ImGui::SeparatorText("Background");
                
                ImGui::Checkbox("Override Clear Color", &ctx.visuals.enable_clear);
                if (ctx.visuals.enable_clear) {
                    ImGui::ColorEdit4("Clear Color", &ctx.visuals.clear_color.x, ImGuiColorEditFlags_AlphaBar);
                }
                
                bool fog = ctx.visuals.fog_override.load();
                if (ImGui::Checkbox("Override Fog", &fog)) {
                    ctx.visuals.fog_override.store(fog);
                }
                if (fog) {
                    ImGui::ColorEdit4("Fog Color", &ctx.visuals.fog_color.x);
                }
                
                ImGui::EndTabItem();
            }

            if (ImGui::BeginTabItem("Render Scale")) {
                ImGui::SeparatorText("Viewport Scaling");
                
                int scale = ctx.render.render_scale.load();
                if (ImGui::SliderInt("Scale %", &scale, 50, 200, "%d%%")) {
                    ctx.render.render_scale.store(scale);
                    logger.log("render => scale set to {}%", scale);
                }
                ImGui::TextDisabled("50%% = half resolution, 200%% = 2x supersampling");
                
                if (ImGui::Button("50%%")) scale = 50;
                ImGui::SameLine();
                if (ImGui::Button("75%%")) scale = 75;
                ImGui::SameLine();
                if (ImGui::Button("100%%")) scale = 100;
                ImGui::SameLine();
                if (ImGui::Button("150%%")) scale = 150;
                ImGui::SameLine();
                if (ImGui::Button("200%%")) scale = 200;
                ctx.render.render_scale.store(scale);

                ImGui::SeparatorText("Matrix Debug");
                
                bool log_mat = ctx.render.log_matrices.load();
                if (ImGui::Checkbox("Log Matrix Operations", &log_mat)) {
                    ctx.render.log_matrices.store(log_mat);
                }
                ImGui::TextDisabled("Logs glFrustum, glOrtho, glLoadIdentity calls");
                
                if (ctx.render.has_frustum) {
                    ImGui::Separator();
                    ImGui::Text("Last Frustum:");
                    ImGui::Text("  Left   = %.4f", ctx.render.last_frustum[0]);
                    ImGui::Text("  Right  = %.4f", ctx.render.last_frustum[1]);
                    ImGui::Text("  Bottom = %.4f", ctx.render.last_frustum[2]);
                    ImGui::Text("  Top    = %.4f", ctx.render.last_frustum[3]);
                    ImGui::Text("  Near   = %.4f", ctx.render.last_frustum[4]);
                    ImGui::Text("  Far    = %.4f", ctx.render.last_frustum[5]);
                }
                
                if (ctx.render.has_ortho) {
                    ImGui::Separator();
                    ImGui::Text("Last Ortho:");
                    ImGui::Text("  Left   = %.4f", ctx.render.last_ortho[0]);
                    ImGui::Text("  Right  = %.4f", ctx.render.last_ortho[1]);
                    ImGui::Text("  Bottom = %.4f", ctx.render.last_ortho[2]);
                    ImGui::Text("  Top    = %.4f", ctx.render.last_ortho[3]);
                    ImGui::Text("  Near   = %.4f", ctx.render.last_ortho[4]);
                    ImGui::Text("  Far    = %.4f", ctx.render.last_ortho[5]);
                }
                
                ImGui::EndTabItem();
            }

            if (ImGui::BeginTabItem("Gameplay")) {
                ImGui::SeparatorText("Game Speed");
                
                float spd = ctx.gameplay.speed_multiplier.load();
                if (ImGui::SliderFloat("Speed Multiplier", &spd, 0.1f, 10.0f, "%.2fx")) {
                    ctx.gameplay.speed_multiplier.store(spd);
                }
                
                if (ImGui::Button("0.5x")) spd = 0.5f;
                ImGui::SameLine();
                if (ImGui::Button("1.0x")) spd = 1.0f;
                ImGui::SameLine();
                if (ImGui::Button("2.0x")) spd = 2.0f;
                ImGui::SameLine();
                if (ImGui::Button("5.0x")) spd = 5.0f;
                ctx.gameplay.speed_multiplier.store(spd);

                ImGui::SeparatorText("Tweaks");
                
                bool ns = ctx.gameplay.no_sleep.load();
                if (ImGui::Checkbox("No Sleep (Uncap FPS)", &ns)) {
                    ctx.gameplay.no_sleep.store(ns);
                }

                bool rng = ctx.gameplay.freeze_rng.load();
                if (ImGui::Checkbox("Freeze RNG", &rng)) {
                    ctx.gameplay.freeze_rng.store(rng);
                }
                ImGui::TextDisabled("Makes random() always return 42");
                
                ImGui::EndTabItem();
            }

            if (ImGui::BeginTabItem("Debug")) {
                ImGui::SeparatorText("Logging");
                
                bool log_fs = ctx.debug.log_fs.load();
                if (ImGui::Checkbox("Log Filesystem Access", &log_fs)) {
                    ctx.debug.log_fs.store(log_fs);
                }
                
                bool log_gl = ctx.debug.log_gl_calls.load();
                if (ImGui::Checkbox("Log OpenGL Draw Calls", &log_gl)) {
                    ctx.debug.log_gl_calls.store(log_gl);
                }

                ImGui::SeparatorText("Console");
                
                logger.draw_console();
                
                ImGui::EndTabItem();
            }

            if (ImGui::BeginTabItem("Stats")) {
                ImGui::SeparatorText("Performance");
                
                ImGui::Text("FPS: %.1f", ImGui::GetIO().Framerate);
                ImGui::Text("Frame Time: %.3f ms", 1000.0f / ImGui::GetIO().Framerate);
                ImGui::Text("Total Frames: %d", ctx.frame_count.load());
                ImGui::Text("Draw Calls/Frame: %d", ctx.draw_call_count.load());
                
                ImGui::SeparatorText("Memory");
                
                PROCESS_MEMORY_COUNTERS pmc{};
                if (GetProcessMemoryInfo(GetCurrentProcess(), &pmc, sizeof(pmc))) {
                    ImGui::Text("Working Set: %.2f MB", pmc.WorkingSetSize / (1024.0 * 1024.0));
                    ImGui::Text("Peak Working Set: %.2f MB", pmc.PeakWorkingSetSize / (1024.0 * 1024.0));
                }
                
                ImGui::SeparatorText("System Info");
                
                RECT rect{};
                if (GetClientRect(ctx.game_window, &rect)) {
                    ImGui::Text("Window Size: %ldx%ld", 
                        static_cast<long>(rect.right - rect.left), 
                        static_cast<long>(rect.bottom - rect.top));
                }
                
                GLint viewport[4]{};
                glGetIntegerv(GL_VIEWPORT, viewport);
                ImGui::Text("Viewport: %dx%d at (%d,%d)", viewport[2], viewport[3], viewport[0], viewport[1]);
                
                const GLubyte* vendor   = glGetString(GL_VENDOR);
                const GLubyte* renderer = glGetString(GL_RENDERER);
                const GLubyte* version  = glGetString(GL_VERSION);
                
                if (vendor) ImGui::Text("GPU Vendor: %s", vendor);
                if (renderer) ImGui::Text("GPU: %s", renderer);
                if (version) ImGui::Text("OpenGL: %s", version);
                
                ImGui::EndTabItem();
            }

            ImGui::EndTabBar();
        }
        
        ImGui::Separator();
        ImGui::TextDisabled("Press INSERT to toggle overlay");
    }
    ImGui::End();
    
    if (ctx.debug.show_metrics.load()) {
        ImGui::ShowMetricsWindow();
    }
    if (ctx.debug.show_demo.load()) {
        ImGui::ShowDemoWindow();
    }

    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
}
