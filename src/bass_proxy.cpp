#define WIN32_LEAN_AND_MEAN
#define _CRT_SECURE_NO_WARNINGS

#include "bass_proxy.hpp"

#include <GL/gl.h>
#include <imgui.h>
#include <imgui_impl_opengl3.h>
#include <imgui_impl_win32.h>

#include <psapi.h>
#include <windows.h>

#include <array>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <format>
#include <fstream>
#include <iostream>
#include <memory>
#include <mutex>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

using wgl_swap_t      = BOOL(WINAPI*)(HDC);
using qpc_t           = BOOL(WINAPI*)(LARGE_INTEGER*);
using set_cursor_t    = BOOL(WINAPI*)(int, int);
using sleep_t         = VOID(WINAPI*)(DWORD);
using create_file_a_t = HANDLE(WINAPI*)(
    LPCSTR, DWORD, DWORD, LPSECURITY_ATTRIBUTES, DWORD, DWORD, HANDLE);
using rand_t          = int(__cdecl*)();
using gl_draw_elems_t = void(APIENTRY*)(GLenum, GLsizei, GLenum, const GLvoid*);

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND,
                                                             UINT,
                                                             WPARAM,
                                                             LPARAM);

struct cheat_settings final
{
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

struct global_context final
{
    HWND              game_window{ nullptr };
    WNDPROC           orig_wnd_proc{ nullptr };
    std::atomic<bool> imgui_ready{ false };
    std::atomic<bool> shutting_down{ false };
    cheat_settings    settings;
};

static global_context ctx;

class logger final
{
public:
    logger()
    {
        log_file.open("bass_proxy_log.txt", std::ios::out | std::ios::trunc);
    }

    ~logger()
    {
        if (log_file.is_open())
        {
            log_file.close();
        }
    }

    template <typename... Args>
    void log(std::format_string<Args...> fmt, Args&&... args)
    {
        try
        {
            const std::string msg =
                std::format(fmt, std::forward<Args>(args)...);

            // Get time (using system_clock compatible with older mingw runtimes
            // if chrono::current_zone is missing)
            auto    now      = std::chrono::system_clock::now();
            auto    sys_time = std::chrono::system_clock::to_time_t(now);
            std::tm local_tm{};
#ifdef _WIN32
            localtime_s(&local_tm, &sys_time);
#else
            localtime_r(&sys_time, &local_tm);
#endif

            const std::string timestamped =
                std::format("[{:02}:{:02}:{:02}] {}",
                            local_tm.tm_hour,
                            local_tm.tm_min,
                            local_tm.tm_sec,
                            msg);

            {
                std::scoped_lock lock(file_mtx);
                if (log_file.is_open())
                {
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
        catch (...)
        {
        }
    }

    void draw_console()
    {
        std::scoped_lock lock(console_mtx);

        if (ImGui::Button("Clear"))
        {
            console_buf.clear();
        }
        ImGui::SameLine();
        filter.Draw("Filter", -100.0f);
        ImGui::Separator();

        if (ImGui::BeginChild("LogScroll",
                              ImVec2(0, 0),
                              false,
                              ImGuiWindowFlags_HorizontalScrollbar))
        {
            if (filter.IsActive())
            {
                const char* buf_begin = console_buf.begin();
                const char* line      = buf_begin;
                while (line != nullptr)
                {
                    const char* line_end = strchr(line, '\n');
                    if (filter.PassFilter(line, line_end))
                    {
                        ImGui::TextUnformatted(line, line_end);
                    }
                    line = (line_end && line_end[1]) ? line_end + 1 : nullptr;
                }
            }
            else
            {
                ImGui::TextUnformatted(console_buf.begin());
            }

            if (should_scroll && ImGui::GetScrollY() >= ImGui::GetScrollMaxY())
            {
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

class trampoline_hook final
{
public:
    std::string            name;
    void*                  original_func   = nullptr;
    void*                  trampoline      = nullptr;
    void**                 global_real_ptr = nullptr;
    std::array<uint8_t, 5> original_bytes{};
    bool                   active = false;

    trampoline_hook(std::string_view module,
                    std::string_view func,
                    void*            detour,
                    void**           real_ptr)
        : name(std::format("{}!{}", module, func))
        , global_real_ptr(real_ptr)
    {
        const HMODULE mod = GetModuleHandleA(module.data());
        if (!mod)
        {
            logger.log("Module not found: {}", module);
            return;
        }

        original_func =
            reinterpret_cast<void*>(GetProcAddress(mod, func.data()));
        if (!original_func)
        {
            logger.log("Function not found: {}", func);
            return;
        }

        auto target = static_cast<uint8_t*>(original_func);

        // Check for existing hook (JMP 0xE9)
        if (target[0] == 0xE9)
        {
            logger.log("Warning: {} appears already hooked.", name);
        }

        std::memcpy(original_bytes.data(), target, 5);

        // Allocate trampoline within 32-bit reach if possible, though
        // VirtualAlloc usually works fine on 32-bit
        trampoline = VirtualAlloc(
            nullptr, 64, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
        if (!trampoline)
        {
            logger.log("VirtualAlloc failed for {}", name);
            return;
        }

        const auto t_bytes = static_cast<uint8_t*>(trampoline);

        std::memcpy(t_bytes, original_bytes.data(), 5);

        // JMP back to original + 5
        t_bytes[5]       = 0xE9;
        const auto src_t = reinterpret_cast<uintptr_t>(trampoline) + 10;
        const auto dst_t = reinterpret_cast<uintptr_t>(original_func) + 5;

        // Arithmetic on uintptr_t is unsigned; casting to int32_t interprets as
        // relative offset
        auto rel_t = static_cast<int32_t>(dst_t - src_t);
        std::memcpy(t_bytes + 6, &rel_t, sizeof(rel_t));

        // Install JMP at target
        DWORD old_protect{};
        if (VirtualProtect(
                original_func, 5, PAGE_EXECUTE_READWRITE, &old_protect))
        {
            target[0]        = 0xE9;
            const auto src_h = reinterpret_cast<uintptr_t>(original_func) + 5;
            const auto dst_h = reinterpret_cast<uintptr_t>(detour);
            const auto rel_h = static_cast<int32_t>(dst_h - src_h);

            std::memcpy(target + 1, &rel_h, sizeof(rel_h));

            DWORD dummy{};
            VirtualProtect(original_func, 5, old_protect, &dummy);

            FlushInstructionCache(GetCurrentProcess(), original_func, 5);
            FlushInstructionCache(GetCurrentProcess(), trampoline, 64);

            *global_real_ptr = trampoline;
            active           = true;
            logger.log("Hooked {} @ {:p}", name, original_func);
        }
        else
        {
            logger.log("VirtualProtect failed for {}", name);
            VirtualFree(trampoline, 0, MEM_RELEASE);
            trampoline = nullptr;
        }
    }

    void uninstall()
    {
        if (!active || !original_func)
        {
            return;
        }

        DWORD old_protect{};
        if (VirtualProtect(
                original_func, 5, PAGE_EXECUTE_READWRITE, &old_protect))
        {
            std::memcpy(original_func, original_bytes.data(), 5);
            DWORD dummy{};
            VirtualProtect(original_func, 5, old_protect, &dummy);
            FlushInstructionCache(GetCurrentProcess(), original_func, 5);
            logger.log("Restored {}", name);
        }

        if (trampoline)
        {
            VirtualFree(trampoline, 0, MEM_RELEASE);
            trampoline = nullptr;
        }

        if (global_real_ptr)
        {
            *global_real_ptr = nullptr;
        }
        active = false;
    }
};

static std::vector<std::unique_ptr<trampoline_hook>> g_hooks;

static wgl_swap_t      real_wgl_swap      = nullptr;
static qpc_t           real_qpc           = nullptr;
static set_cursor_t    real_set_cursor    = nullptr;
static sleep_t         real_sleep         = nullptr;
static create_file_a_t real_create_file_a = nullptr;
static rand_t          real_rand          = nullptr;
static gl_draw_elems_t real_gl_draw_elems = nullptr;

static void             draw_ui();
static LRESULT CALLBACK detour_wnd_proc(HWND h, UINT m, WPARAM w, LPARAM l);

static BOOL WINAPI detour_wgl_swap(HDC dc)
{
    if (ctx.imgui_ready.load(std::memory_order_acquire))
    {
        if (ImGui::GetCurrentContext())
        {
            ctx.settings.block_mouse.store(ImGui::GetIO().WantCaptureMouse,
                                           std::memory_order_relaxed);
        }
        draw_ui();
    }

    static std::once_flag init_flag;
    if (wglGetCurrentContext())
    {
        std::call_once(
            init_flag,
            [&]()
            {
                logger.log("Initializing ImGui Context...");
                ctx.game_window = WindowFromDC(dc);
                if (ctx.game_window)
                {
                    ctx.orig_wnd_proc =
                        reinterpret_cast<WNDPROC>(SetWindowLongPtrA(
                            ctx.game_window,
                            GWLP_WNDPROC,
                            reinterpret_cast<LONG_PTR>(detour_wnd_proc)));

                    ImGui::CreateContext();
                    ImGuiIO& io        = ImGui::GetIO();
                    io.FontGlobalScale = 1.2f;
                    io.IniFilename     = nullptr; // Disable ini file generation

                    ImGui_ImplWin32_Init(ctx.game_window);
                    ImGui_ImplOpenGL3_Init("#version 330 core");

                    SetWindowTextA(ctx.game_window, "Airstrike 3D II [DEBUG]");
                    ctx.imgui_ready.store(true, std::memory_order_release);
                    logger.log("ImGui Init Complete.");
                }
            });
    }
    return real_wgl_swap(dc);
}

static BOOL WINAPI detour_qpc(LARGE_INTEGER* lp_performance_count)
{
    if (!real_qpc || !real_qpc(lp_performance_count))
    {
        return FALSE;
    }

    static LARGE_INTEGER last_real = {};
    static LARGE_INTEGER last_fake = {};
    static bool          first     = true;
    static std::mutex    qpc_mtx;

    const float mult =
        ctx.settings.speed_multiplier.load(std::memory_order_relaxed);

    // Optimization: If speed is normal, don't lock
    if (std::abs(mult - 1.0f) < 0.0001f && !first)
    {
        // If we just switched back to 1.0, we technically need to resync to
        // avoid jumps, but for a simple hack, running real time is usually
        // acceptable. However, to keep time continuous, we continue the fake
        // time logic.
    }

    std::scoped_lock lock(qpc_mtx);
    if (first)
    {
        last_real = *lp_performance_count;
        last_fake = *lp_performance_count;
        first     = false;
        return TRUE;
    }

    const LONGLONG diff = lp_performance_count->QuadPart - last_real.QuadPart;
    last_real           = *lp_performance_count;

    // Handle double->int64 conversion safely
    const double scaled_diff =
        static_cast<double>(diff) * static_cast<double>(mult);
    last_fake.QuadPart += static_cast<LONGLONG>(scaled_diff);

    *lp_performance_count = last_fake;
    return TRUE;
}

static BOOL WINAPI detour_set_cursor(int x, int y)
{
    if (ctx.settings.block_mouse.load(std::memory_order_relaxed))
    {
        return TRUE;
    }
    return real_set_cursor(x, y);
}

static VOID WINAPI detour_sleep(DWORD dw_milliseconds)
{
    if (ctx.settings.no_sleep.load(std::memory_order_relaxed))
    {
        return real_sleep(0);
    }
    return real_sleep(dw_milliseconds);
}

static HANDLE WINAPI detour_create_file_a(LPCSTR                file_name,
                                          DWORD                 access,
                                          DWORD                 share,
                                          LPSECURITY_ATTRIBUTES sec,
                                          DWORD                 disp,
                                          DWORD                 attr,
                                          HANDLE                temp)
{
    if (ctx.settings.log_fs.load(std::memory_order_relaxed))
    {
        logger.log("FS_Access: {}", file_name ? file_name : "NULL");
    }
    return real_create_file_a(file_name, access, share, sec, disp, attr, temp);
}

static int __cdecl detour_rand()
{
    if (ctx.settings.freeze_rng.load(std::memory_order_relaxed))
    {
        return 0;
    }
    return real_rand();
}

static void APIENTRY detour_gl_draw_elems(GLenum        mode,
                                          GLsizei       count,
                                          GLenum        type,
                                          const GLvoid* indices)
{
    bool depth = ctx.settings.disable_depth.load(std::memory_order_relaxed);
    if (depth)
    {
        glDisable(GL_DEPTH_TEST);
        glDepthMask(GL_FALSE);
    }

    if (ctx.settings.wireframe.load(std::memory_order_relaxed))
    {
        glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
    }

    real_gl_draw_elems(mode, count, type, indices);

    if (depth)
    {
        glEnable(GL_DEPTH_TEST);
        glDepthMask(GL_TRUE);
    }
    // Always restore fill to prevent UI breaking
    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
}

static LRESULT CALLBACK detour_wnd_proc(HWND h, UINT m, WPARAM w, LPARAM l)
{
    if (!ctx.shutting_down.load() && ImGui_ImplWin32_WndProcHandler(h, m, w, l))
    {
        return true;
    }
    return CallWindowProc(ctx.orig_wnd_proc, h, m, w, l);
}

void install_hooks()
{
    logger.log("Starting hook installation...");

    auto add = [](const char* mod, const char* func, void* hook, void** real)
    {
        g_hooks.push_back(
            std::make_unique<trampoline_hook>(mod, func, hook, real));
    };

    add("opengl32.dll",
        "wglSwapBuffers",
        (void*)detour_wgl_swap,
        (void**)&real_wgl_swap);
    add("opengl32.dll",
        "glDrawElements",
        (void*)detour_gl_draw_elems,
        (void**)&real_gl_draw_elems);
    add("kernel32.dll",
        "QueryPerformanceCounter",
        (void*)detour_qpc,
        (void**)&real_qpc);
    add("kernel32.dll", "Sleep", (void*)detour_sleep, (void**)&real_sleep);
    add("kernel32.dll",
        "CreateFileA",
        (void*)detour_create_file_a,
        (void**)&real_create_file_a);
    add("user32.dll",
        "SetCursorPos",
        (void*)detour_set_cursor,
        (void**)&real_set_cursor);
    add("msvcrt.dll", "rand", (void*)detour_rand, (void**)&real_rand);
}

void uninstall_hooks()
{
    logger.log("Uninstall requested.");
    ctx.shutting_down.store(true);

    if (ctx.game_window && ctx.orig_wnd_proc)
    {
        SetWindowLongPtrA(ctx.game_window,
                          GWLP_WNDPROC,
                          reinterpret_cast<LONG_PTR>(ctx.orig_wnd_proc));
        SetWindowTextA(ctx.game_window, "Airstrike 3D II");
    }

    for (auto it = g_hooks.rbegin(); it != g_hooks.rend(); ++it)
    {
        (*it)->uninstall();
    }
    g_hooks.clear();

    if (ctx.imgui_ready.exchange(false))
    {
        // Note: Calling GL shutdown from a non-render thread is risky but
        // needed for clean detach ideally we would queue this, but for now we
        // just rely on driver robustness.
        ImGui_ImplOpenGL3_Shutdown();
        ImGui_ImplWin32_Shutdown();
        ImGui::DestroyContext();
    }
    logger.log("Uninstallation complete.");
}

static void draw_ui()
{
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();

    ImGui::SetNextWindowSize(ImVec2(450, 400), ImGuiCond_FirstUseEver);

    if (ImGui::Begin("Bass Proxy Tools", nullptr, ImGuiWindowFlags_MenuBar))
    {

        if (ImGui::BeginMenuBar())
        {
            if (ImGui::BeginMenu("Menu"))
            {
                if (ImGui::MenuItem("Unload"))
                {
                    // Detach in a separate thread to avoid deadlock in
                    // SwapBuffers
                    std::thread([] { uninstall_hooks(); }).detach();
                }
                if (ImGui::MenuItem("Exit Game"))
                {
                    PostQuitMessage(0);
                }
                ImGui::EndMenu();
            }
            ImGui::EndMenuBar();
        }

        if (ImGui::BeginTabBar("Tabs"))
        {
            if (ImGui::BeginTabItem("Visuals"))
            {
                bool depth = ctx.settings.disable_depth.load();
                if (ImGui::Checkbox("Disable Depth", &depth))
                {
                    ctx.settings.disable_depth.store(depth);
                }

                bool wire = ctx.settings.wireframe.load();
                if (ImGui::Checkbox("Wireframe", &wire))
                {
                    ctx.settings.wireframe.store(wire);
                }

                ImGui::Separator();
                ImGui::Checkbox("Clear Screen", &ctx.settings.enable_clear);
                ImGui::ColorEdit4("Color", &ctx.settings.clear_color.x);
                ImGui::EndTabItem();
            }

            if (ImGui::BeginTabItem("Gameplay"))
            {
                float spd = ctx.settings.speed_multiplier.load();
                if (ImGui::SliderFloat("Speed", &spd, 0.1f, 10.0f, "%.2fx"))
                {
                    ctx.settings.speed_multiplier.store(spd);
                }
                if (ImGui::Button("Reset Speed"))
                {
                    ctx.settings.speed_multiplier.store(1.0f);
                }

                bool ns = ctx.settings.no_sleep.load();
                if (ImGui::Checkbox("No Sleep (FPS Uncap)", &ns))
                {
                    ctx.settings.no_sleep.store(ns);
                }

                bool rng = ctx.settings.freeze_rng.load();
                if (ImGui::Checkbox("Freeze RNG", &rng))
                {
                    ctx.settings.freeze_rng.store(rng);
                }
                ImGui::EndTabItem();
            }

            if (ImGui::BeginTabItem("Logs"))
            {
                bool log_fs = ctx.settings.log_fs.load();
                if (ImGui::Checkbox("Log Filesystem", &log_fs))
                {
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

    if (ctx.settings.enable_clear)
    {
        glClearColor(ctx.settings.clear_color.x,
                     ctx.settings.clear_color.y,
                     ctx.settings.clear_color.z,
                     ctx.settings.clear_color.w);
        glClear(GL_COLOR_BUFFER_BIT);
    }

    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
}