#define WIN32_LEAN_AND_MEAN
#define _CRT_SECURE_NO_WARNINGS

#include "bass_proxy.hpp"

#include <MinHook.h>

#include <GL/gl.h>
#include <imgui.h>
#include <imgui_impl_opengl3.h>
#include <imgui_impl_win32.h>

#include <windows.h>

#include <array>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <format>
#include <fstream>
#include <iostream>
#include <mutex>
#include <string>
#include <string_view>
#include <thread>

using wgl_swap_t      = BOOL(WINAPI*)(HDC);
using qpc_t           = BOOL(WINAPI*)(LARGE_INTEGER*);
using set_cursor_t    = BOOL(WINAPI*)(int, int);
using sleep_t         = VOID(WINAPI*)(DWORD);
using create_file_a_t = HANDLE(WINAPI*)(LPCSTR, DWORD, DWORD, LPSECURITY_ATTRIBUTES, DWORD, DWORD, HANDLE);
using rand_t          = int(__cdecl*)();
using gl_draw_elems_t = void(APIENTRY*)(GLenum, GLsizei, GLenum, const GLvoid*);

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND, UINT, WPARAM, LPARAM);

struct cheat_settings final {
    std::atomic<float> speed_multiplier{ 1.0f };
    std::atomic<bool>  block_mouse{ false };
    std::atomic<bool>  disable_depth{ false };
    std::atomic<bool>  no_sleep{ false };
    std::atomic<bool>  wireframe{ false };
    std::atomic<bool>  log_fs{ false };
    std::atomic<bool>  freeze_rng{ false };

    ImVec4 clear_color{ 0.0f, 0.0f, 0.0f, 0.0f };
    bool   enable_clear{ false };
};

struct global_context final {
    HWND              game_window{ nullptr };
    WNDPROC           orig_wnd_proc{ nullptr };
    std::atomic<bool> imgui_ready{ false };
    std::atomic<bool> shutting_down{ false };
    cheat_settings    settings;
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

// Original function pointers (trampolines)
static wgl_swap_t      orig_wgl_swap      = nullptr;
static qpc_t           orig_qpc           = nullptr;
static set_cursor_t    orig_set_cursor    = nullptr;
static sleep_t         orig_sleep         = nullptr;
static create_file_a_t orig_create_file_a = nullptr;
static rand_t          orig_rand          = nullptr;
static gl_draw_elems_t orig_gl_draw_elems = nullptr;

static void             draw_ui();
static LRESULT CALLBACK detour_wnd_proc(HWND h, UINT m, WPARAM w, LPARAM l);

static BOOL WINAPI detour_wgl_swap(HDC dc) {
    if (ctx.imgui_ready.load(std::memory_order_acquire)) {
        if (ImGui::GetCurrentContext()) {
            ctx.settings.block_mouse.store(ImGui::GetIO().WantCaptureMouse, std::memory_order_relaxed);
        }
        draw_ui();
    }

    static std::once_flag init_flag;
    if (wglGetCurrentContext()) {
        std::call_once(init_flag, [&]() {
            logger.log("imgui => initializing context...");
            ctx.game_window = WindowFromDC(dc);
            if (ctx.game_window) {
                ctx.orig_wnd_proc = reinterpret_cast<WNDPROC>(
                    SetWindowLongPtrA(ctx.game_window, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(detour_wnd_proc)));

                ImGui::CreateContext();
                ImGuiIO& io        = ImGui::GetIO();
                io.FontGlobalScale = 1.2f;
                io.IniFilename     = nullptr;

                ImGui_ImplWin32_Init(ctx.game_window);
                ImGui_ImplOpenGL3_Init("#version 330 core");

                SetWindowTextA(ctx.game_window, "Airstrike 3D II [DEBUG]");
                ctx.imgui_ready.store(true, std::memory_order_release);
                logger.log("imgui => initialization complete");
            }
        });
    }
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

    const float mult = ctx.settings.speed_multiplier.load(std::memory_order_relaxed);

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
    if (ctx.settings.block_mouse.load(std::memory_order_relaxed)) {
        return TRUE;
    }
    return orig_set_cursor(x, y);
}

static VOID WINAPI detour_sleep(DWORD dw_milliseconds) {
    if (ctx.settings.no_sleep.load(std::memory_order_relaxed)) {
        return orig_sleep(0);
    }
    return orig_sleep(dw_milliseconds);
}

static HANDLE WINAPI detour_create_file_a(
    LPCSTR file_name, DWORD access, DWORD share, LPSECURITY_ATTRIBUTES sec, DWORD disp, DWORD attr, HANDLE temp) {
    if (ctx.settings.log_fs.load(std::memory_order_relaxed)) {
        logger.log("fs_access => \"{}\"", file_name ? file_name : "NULL");
    }
    return orig_create_file_a(file_name, access, share, sec, disp, attr, temp);
}

static int __cdecl detour_rand() {
    if (ctx.settings.freeze_rng.load(std::memory_order_relaxed)) {
        return 0;
    }
    return orig_rand();
}

static void APIENTRY detour_gl_draw_elems(GLenum mode, GLsizei count, GLenum type, const GLvoid* indices) {
    bool depth = ctx.settings.disable_depth.load(std::memory_order_relaxed);
    if (depth) {
        glDisable(GL_DEPTH_TEST);
        glDepthMask(GL_FALSE);
    }

    if (ctx.settings.wireframe.load(std::memory_order_relaxed)) {
        glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
    }

    orig_gl_draw_elems(mode, count, type, indices);

    if (depth) {
        glEnable(GL_DEPTH_TEST);
        glDepthMask(GL_TRUE);
    }
    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
}

static LRESULT CALLBACK detour_wnd_proc(HWND h, UINT m, WPARAM w, LPARAM l) {
    if (!ctx.shutting_down.load() && ImGui_ImplWin32_WndProcHandler(h, m, w, l)) {
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

    ImGui::SetNextWindowSize(ImVec2(450, 400), ImGuiCond_FirstUseEver);

    if (ImGui::Begin("Bass Proxy Tools", nullptr, ImGuiWindowFlags_MenuBar)) {
        if (ImGui::BeginMenuBar()) {
            if (ImGui::BeginMenu("Menu")) {
                if (ImGui::MenuItem("Unload")) {
                    std::thread([] { uninstall_hooks(); }).detach();
                }
                if (ImGui::MenuItem("Exit Game")) {
                    PostQuitMessage(0);
                }
                ImGui::EndMenu();
            }
            ImGui::EndMenuBar();
        }

        if (ImGui::BeginTabBar("Tabs")) {
            if (ImGui::BeginTabItem("Visuals")) {
                bool depth = ctx.settings.disable_depth.load();
                if (ImGui::Checkbox("Disable Depth", &depth)) {
                    ctx.settings.disable_depth.store(depth);
                }

                bool wire = ctx.settings.wireframe.load();
                if (ImGui::Checkbox("Wireframe", &wire)) {
                    ctx.settings.wireframe.store(wire);
                }

                ImGui::Separator();
                ImGui::Checkbox("Clear Screen", &ctx.settings.enable_clear);
                ImGui::ColorEdit4("Color", &ctx.settings.clear_color.x);
                ImGui::EndTabItem();
            }

            if (ImGui::BeginTabItem("Gameplay")) {
                float spd = ctx.settings.speed_multiplier.load();
                if (ImGui::SliderFloat("Speed", &spd, 0.1f, 10.0f, "%.2fx")) {
                    ctx.settings.speed_multiplier.store(spd);
                }
                if (ImGui::Button("Reset Speed")) {
                    ctx.settings.speed_multiplier.store(1.0f);
                }

                bool ns = ctx.settings.no_sleep.load();
                if (ImGui::Checkbox("No Sleep (FPS Uncap)", &ns)) {
                    ctx.settings.no_sleep.store(ns);
                }

                bool rng = ctx.settings.freeze_rng.load();
                if (ImGui::Checkbox("Freeze RNG", &rng)) {
                    ctx.settings.freeze_rng.store(rng);
                }
                ImGui::EndTabItem();
            }

            if (ImGui::BeginTabItem("Logs")) {
                bool log_fs = ctx.settings.log_fs.load();
                if (ImGui::Checkbox("Log Filesystem", &log_fs)) {
                    ctx.settings.log_fs.store(log_fs);
                }
                ImGui::Separator();
                logger.draw_console();
                ImGui::EndTabItem();
            }
            ImGui::EndTabBar();
        }
    }
    ImGui::End();

    ImGui::Render();

    if (ctx.settings.enable_clear) {
        glClearColor(
            ctx.settings.clear_color.x,
            ctx.settings.clear_color.y,
            ctx.settings.clear_color.z,
            ctx.settings.clear_color.w);
        glClear(GL_COLOR_BUFFER_BIT);
    }

    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
}
