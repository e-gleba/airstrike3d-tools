#define WIN32_LEAN_AND_MEAN

#include "bass_proxy.hpp"

#include <GL/gl.h>
#include <imgui_impl_opengl3.h>
#include <imgui_impl_win32.h>

#include <array>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdarg>
#include <cstddef>
#include <cstdio>
#include <cstring>
#include <imgui.h>
#include <iostream>
#include <mutex>
#include <psapi.h>
#include <string>
#include <utility>
#include <vector>

#include <windows.h>

// --- Forward Declarations ---
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND,
                                                             UINT,
                                                             WPARAM,
                                                             LPARAM);
static void                   draw_overlay();
static void                   log_msg(const char* fmt, ...);
static void report_winapi_error(const char* operation, DWORD error_code);
// FIX 1: Forward declare wnd_proc so hook_swap can see it
static LRESULT CALLBACK wnd_proc(HWND h, UINT m, WPARAM w, LPARAM l);

// --- Globals & State ---
static HWND              game_window   = nullptr;
static WNDPROC           orig_wnd_proc = nullptr;
static std::atomic<bool> imgui_ready   = false;
static std::atomic<bool> shutting_down = false;

// Cheat States
static ImVec4             clear_color         = ImVec4(0, 0, 0, 0);
static bool               enable_clear        = false;
static bool               wireframe           = false;
static std::atomic<float> g_speed_multiplier  = 1.0f;
static std::atomic<bool>  g_block_mouse_input = false;

// --- Function Signatures ---
using wgl_swap_t   = BOOL(WINAPI*)(HDC);
using qpc_t        = BOOL(WINAPI*)(LARGE_INTEGER*);
using set_cursor_t = BOOL(WINAPI*)(int, int);

// --- Real Function Pointers (Trampolines) ---
static wgl_swap_t   real_wgl_swap   = nullptr;
static qpc_t        real_qpc        = nullptr;
static set_cursor_t real_set_cursor = nullptr;

// --- Hooking Infrastructure ---

struct active_hook
{
    std::string         module_name;
    std::string         func_name;
    void*               target_addr     = nullptr;
    void*               trampoline      = nullptr;
    void**              global_real_ptr = nullptr;
    std::array<BYTE, 5> original_bytes  = {};
    bool                is_active       = false;
};

static std::vector<active_hook> g_installed_hooks;

static bool install_inline_hook(const char* module_name,
                                const char* func_name,
                                void*       hook_func,
                                void**      target_real_ptr)
{
    log_msg("hooking %s -> %s...", module_name, func_name);

    const HMODULE mod = GetModuleHandleA(module_name);
    if (!mod)
    {
        report_winapi_error("GetModuleHandleA", GetLastError());
        return false;
    }

    void* target_void = reinterpret_cast<void*>(GetProcAddress(mod, func_name));
    if (!target_void)
    {
        report_winapi_error("GetProcAddress", GetLastError());
        return false;
    }

    auto* target_addr = static_cast<uint8_t*>(target_void);

    if (target_addr[0] == 0xE9)
    {
        log_msg("warning: %s is already hooked", func_name);
    }

    active_hook hook_entry;
    hook_entry.module_name     = module_name;
    hook_entry.func_name       = func_name;
    hook_entry.target_addr     = target_void;
    hook_entry.global_real_ptr = target_real_ptr;
    std::memcpy(hook_entry.original_bytes.data(), target_addr, 5);

    void* trampoline_void = VirtualAlloc(
        nullptr, 64, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
    if (!trampoline_void)
    {
        report_winapi_error("VirtualAlloc", GetLastError());
        return false;
    }
    auto* trampoline      = static_cast<uint8_t*>(trampoline_void);
    hook_entry.trampoline = trampoline_void;

    DWORD old_protect = 0;
    if (!VirtualProtect(target_void, 5, PAGE_EXECUTE_READWRITE, &old_protect))
    {
        report_winapi_error("VirtualProtect", GetLastError());
        VirtualFree(trampoline_void, 0, MEM_RELEASE);
        return false;
    }

    std::memcpy(trampoline, target_addr, 5);
    trampoline[5]             = 0xE9;
    const uintptr_t dest_back = reinterpret_cast<uintptr_t>(target_addr) + 5;
    const uintptr_t src_back  = reinterpret_cast<uintptr_t>(trampoline) + 10;
    const int32_t   rel_back  = static_cast<int32_t>(dest_back - src_back);
    std::memcpy(trampoline + 6, &rel_back, sizeof(rel_back));

    target_addr[0]            = 0xE9;
    const uintptr_t dest_hook = reinterpret_cast<uintptr_t>(hook_func);
    const uintptr_t src_hook  = reinterpret_cast<uintptr_t>(target_addr) + 5;
    const int32_t   rel_hook  = static_cast<int32_t>(dest_hook - src_hook);
    std::memcpy(target_addr + 1, &rel_hook, sizeof(rel_hook));

    DWORD dummy = 0;
    VirtualProtect(target_void, 5, old_protect, &dummy);
    FlushInstructionCache(GetCurrentProcess(), target_void, 5);
    FlushInstructionCache(GetCurrentProcess(), trampoline_void, 10);

    *target_real_ptr     = trampoline_void;
    hook_entry.is_active = true;
    g_installed_hooks.push_back(hook_entry);

    log_msg("hooked %s successfully at %p", func_name, target_void);
    return true;
}

// --- Hook Implementations ---

[[nodiscard]] static bool WINAPI hook_swap(HDC dc) noexcept
{
    const bool is_ui_open = imgui_ready.load(std::memory_order::acquire);

    if (is_ui_open)
    {
        if (ImGui::GetCurrentContext())
        {
            g_block_mouse_input.store(ImGui::GetIO().WantCaptureMouse,
                                      std::memory_order::relaxed);
        }
        draw_overlay();
    }

    static std::once_flag init_flag;
    if (wglGetCurrentContext())
    {
        std::call_once(
            init_flag,
            [&]()
            {
                log_msg("initializing imgui context...");
                game_window = WindowFromDC(wglGetCurrentDC());

                if (game_window)
                {
                    orig_wnd_proc = reinterpret_cast<WNDPROC>(SetWindowLongPtrA(
                        game_window,
                        GWLP_WNDPROC,
                        reinterpret_cast<LONG_PTR>(wnd_proc)));

                    ImGui::CreateContext();
                    ImGui::GetIO().FontGlobalScale = 1.2f;
                    ImGui_ImplWin32_Init(game_window);
                    ImGui_ImplOpenGL3_Init("#version 330 core");

                    imgui_ready.store(true, std::memory_order::release);
                    log_msg("imgui initialized");
                }
            });
    }

    return real_wgl_swap(dc);
}

static BOOL WINAPI hook_qpc(LARGE_INTEGER* lpPerformanceCount)
{
    if (!real_qpc)
        return FALSE;

    const BOOL result = real_qpc(lpPerformanceCount);
    if (!result)
        return FALSE;

    static LARGE_INTEGER last_real_time = {};
    static LARGE_INTEGER last_fake_time = {};
    static bool          first_call     = true;
    static std::mutex    qpc_mtx;

    std::lock_guard lock(qpc_mtx);

    if (first_call)
    {
        last_real_time = *lpPerformanceCount;
        last_fake_time = *lpPerformanceCount;
        first_call     = false;
        return TRUE;
    }

    const LONGLONG diff =
        lpPerformanceCount->QuadPart - last_real_time.QuadPart;
    last_real_time = *lpPerformanceCount;

    const double mult = static_cast<double>(
        g_speed_multiplier.load(std::memory_order::relaxed));
    const auto modified_diff =
        static_cast<LONGLONG>(static_cast<double>(diff) * mult);

    last_fake_time.QuadPart += modified_diff;
    *lpPerformanceCount = last_fake_time;

    return TRUE;
}

static BOOL WINAPI hook_set_cursor_pos(int x, int y)
{
    if (g_block_mouse_input.load(std::memory_order::relaxed))
    {
        return TRUE;
    }
    return real_set_cursor(x, y);
}

// --- Console System ---
struct console_buffer final
{
    ImGuiTextBuffer    buf;
    ImGuiTextFilter    filter;
    ImVector<int>      line_offsets;
    std::vector<int>   filtered_indices;
    bool               auto_scroll = true;
    mutable std::mutex mtx;

    console_buffer()
    {
        line_offsets.push_back(0);
        filtered_indices.reserve(1024);
    }

    void clear()
    {
        std::lock_guard lock(mtx);
        buf.clear();
        line_offsets.clear();
        line_offsets.push_back(0);
        filtered_indices.clear();
    }

    void add_log(const char* fmt, ...) IM_FMTARGS(2)
    {
        std::lock_guard lock(mtx);
        int             old_size = buf.size();
        va_list         args;
        va_start(args, fmt);
        buf.appendfv(fmt, args);
        va_end(args);

        for (int i = old_size; i < buf.size(); ++i)
        {
            if (buf[i] == '\n')
            {
                line_offsets.push_back(i + 1);
                if (filter.IsActive() && line_offsets.Size >= 2)
                {
                    const int s = line_offsets[line_offsets.Size - 2];
                    const int e = i;
                    if (filter.PassFilter(buf.begin() + s, buf.begin() + e))
                        filtered_indices.push_back(line_offsets.Size - 2);
                }
            }
        }
    }

    void draw()
    {
        if (ImGui::Button("clear"))
            clear();
        ImGui::SameLine();
        filter.Draw("filter", -100.0f);
        ImGui::Separator();

        if (ImGui::BeginChild("scrolling",
                              ImVec2(0, 0),
                              false,
                              ImGuiWindowFlags_HorizontalScrollbar))
        {
            std::lock_guard  lock(mtx);
            ImGuiListClipper clipper;
            const char*      start = buf.begin();

            if (filter.IsActive())
            {
                clipper.Begin(static_cast<int>(filtered_indices.size()));
                while (clipper.Step())
                {
                    for (int i = clipper.DisplayStart; i < clipper.DisplayEnd;
                         i++)
                    {
                        // FIX 2: Explicit static_cast<size_t>(i) for vector
                        // indexing
                        int idx = filtered_indices[static_cast<size_t>(i)];
                        ImGui::TextUnformatted(start + line_offsets[idx],
                                               start + line_offsets[idx + 1] -
                                                   1);
                    }
                }
            }
            else
            {
                clipper.Begin(line_offsets.Size - 1);
                while (clipper.Step())
                {
                    for (int i = clipper.DisplayStart; i < clipper.DisplayEnd;
                         i++)
                    {
                        ImGui::TextUnformatted(start + line_offsets[i],
                                               start + line_offsets[i + 1] - 1);
                    }
                }
            }
            if (auto_scroll && ImGui::GetScrollY() >= ImGui::GetScrollMaxY())
                ImGui::SetScrollHereY(1.0f);
        }
        ImGui::EndChild();
    }
};

static console_buffer console;

static void log_msg(const char* fmt, ...)
{
    char       buffer[2048];
    const auto now =
        std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
    tm tm_info{};
    localtime_s(&tm_info, &now);

    int offset = std::snprintf(buffer,
                               sizeof(buffer),
                               "[%02d:%02d:%02d] ",
                               tm_info.tm_hour,
                               tm_info.tm_min,
                               tm_info.tm_sec);

    if (offset < 0)
        offset = 0;
    if (static_cast<size_t>(offset) >= sizeof(buffer))
        offset = static_cast<int>(sizeof(buffer)) - 1;

    va_list args;
    va_start(args, fmt);

    // FIX 3: Explicit static_cast<size_t>(offset) for subtraction
    std::vsnprintf(buffer + offset,
                   sizeof(buffer) - static_cast<size_t>(offset),
                   fmt,
                   args);
    va_end(args);

    console.add_log("%s\n", buffer);
}

static void report_winapi_error(const char* operation, DWORD error_code)
{
    log_msg("error: %s failed with code %lu", operation, error_code);
}

// --- Main Init/Cleanup ---

void install_hook()
{
    log_msg("bass proxy: core systems loading...");

    // Hook 1: OpenGL Swap Buffers
    if (!install_inline_hook("opengl32.dll",
                             "wglSwapBuffers",
                             reinterpret_cast<void*>(hook_swap),
                             reinterpret_cast<void**>(&real_wgl_swap)))
    {
        log_msg("critical error: failed to hook wglSwapBuffers");
    }

    // Hook 2: Kernel32 Speedhack
    if (install_inline_hook("kernel32.dll",
                            "QueryPerformanceCounter",
                            reinterpret_cast<void*>(hook_qpc),
                            reinterpret_cast<void**>(&real_qpc)))
    {
        log_msg("speedhack subsystem active");
    }

    // Hook 3: User32 Input Blocking
    if (install_inline_hook("user32.dll",
                            "SetCursorPos",
                            reinterpret_cast<void*>(hook_set_cursor_pos),
                            reinterpret_cast<void**>(&real_set_cursor)))
    {
        log_msg("input blocking subsystem active");
    }

    log_msg("all hooks installed");
}

void remove_hooks()
{
    log_msg("cleanup started");
    shutting_down.store(true, std::memory_order::seq_cst);

    if (orig_wnd_proc && game_window && IsWindow(game_window))
    {
        SetWindowLongPtrA(game_window,
                          GWLP_WNDPROC,
                          reinterpret_cast<LONG_PTR>(orig_wnd_proc));
    }

    for (auto it = g_installed_hooks.rbegin(); it != g_installed_hooks.rend();
         ++it)
    {
        auto& hook = *it;
        if (hook.is_active && hook.target_addr)
        {
            DWORD old_protect;
            if (VirtualProtect(
                    hook.target_addr, 5, PAGE_EXECUTE_READWRITE, &old_protect))
            {
                std::memcpy(hook.target_addr, hook.original_bytes.data(), 5);
                DWORD dummy;
                VirtualProtect(hook.target_addr, 5, old_protect, &dummy);
                FlushInstructionCache(GetCurrentProcess(), hook.target_addr, 5);
                log_msg("restored %s!%s",
                        hook.module_name.c_str(),
                        hook.func_name.c_str());
            }
        }

        if (hook.trampoline)
        {
            VirtualFree(hook.trampoline, 0, MEM_RELEASE);
        }

        if (hook.global_real_ptr)
        {
            *hook.global_real_ptr = nullptr;
        }
    }
    g_installed_hooks.clear();

    if (imgui_ready.exchange(false))
    {
        if (ImGui::GetCurrentContext())
        {
            ImGui_ImplOpenGL3_Shutdown();
            ImGui_ImplWin32_Shutdown();
            ImGui::DestroyContext();
        }
    }

    log_msg("cleanup finished");
}

static LRESULT CALLBACK wnd_proc(HWND h, UINT m, WPARAM w, LPARAM l)
{
    if (!shutting_down.load() && ImGui_ImplWin32_WndProcHandler(h, m, w, l))
        return true;
    return CallWindowProc(orig_wnd_proc, h, m, w, l);
}

static void draw_overlay()
{
    if (!ImGui::GetCurrentContext())
        return;

    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();

    ImGui::SetNextWindowPos(ImVec2(20, 20), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(500, 400), ImGuiCond_FirstUseEver);

    if (ImGui::Begin("Bass Proxy Tools", nullptr, ImGuiWindowFlags_MenuBar))
    {
        if (ImGui::BeginMenuBar())
        {
            if (ImGui::BeginMenu("Game"))
            {
                if (ImGui::MenuItem("Exit Process"))
                    PostQuitMessage(0);
                ImGui::EndMenu();
            }
            ImGui::EndMenuBar();
        }

        if (ImGui::BeginTabBar("Tabs"))
        {
            if (ImGui::BeginTabItem("Cheats"))
            {
                ImGui::TextDisabled("Gameplay Modifiers");
                ImGui::Separator();

                ImGui::Text("Time Scale (Speedhack)");
                float speed = g_speed_multiplier.load();
                if (ImGui::SliderFloat("##speed", &speed, 0.1f, 5.0f, "%.2fx"))
                {
                    g_speed_multiplier.store(speed);
                }
                if (ImGui::Button("Reset Speed"))
                    g_speed_multiplier.store(1.0f);

                ImGui::Spacing();
                ImGui::Separator();

                ImGui::Text("Visuals");
                ImGui::Checkbox("Wireframe Mode", &wireframe);
                glPolygonMode(GL_FRONT_AND_BACK, wireframe ? GL_LINE : GL_FILL);

                ImGui::Checkbox("Force Clear Screen", &enable_clear);
                ImGui::ColorEdit4("Clear Color", &clear_color.x);

                ImGui::EndTabItem();
            }

            if (ImGui::BeginTabItem("Console"))
            {
                console.draw();
                ImGui::EndTabItem();
            }

            if (ImGui::BeginTabItem("Hooks Info"))
            {
                if (ImGui::BeginTable("Hooks",
                                      3,
                                      ImGuiTableFlags_Borders |
                                          ImGuiTableFlags_RowBg))
                {
                    ImGui::TableSetupColumn("Library");
                    ImGui::TableSetupColumn("Function");
                    ImGui::TableSetupColumn("Address");
                    ImGui::TableHeadersRow();

                    for (const auto& h : g_installed_hooks)
                    {
                        ImGui::TableNextRow();
                        ImGui::TableNextColumn();
                        ImGui::TextUnformatted(h.module_name.c_str());
                        ImGui::TableNextColumn();
                        ImGui::TextUnformatted(h.func_name.c_str());
                        ImGui::TableNextColumn();
                        ImGui::Text("%p", h.target_addr);
                    }
                    ImGui::EndTable();
                }
                ImGui::EndTabItem();
            }
            ImGui::EndTabBar();
        }
    }
    ImGui::End();

    ImGui::Render();

    if (enable_clear)
    {
        glClearColor(
            clear_color.x, clear_color.y, clear_color.z, clear_color.w);
        glClear(GL_COLOR_BUFFER_BIT);
    }

    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
}
