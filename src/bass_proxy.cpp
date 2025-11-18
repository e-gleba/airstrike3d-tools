#define WIN32_LEAN_AND_MEAN

#include "bass_proxy.hpp"

#include <GL/gl.h>

#include <imgui_impl_opengl3.h>
#include <imgui_impl_win32.h>

#include <array>
#include <atomic>
#include <chrono>
#include <cstdarg>
#include <cstddef>
#include <cstdio>
#include <imgui.h>
#include <iostream>
#include <mutex>
#include <psapi.h>
#include <string>
#include <utility>
#include <vector>

#include <psapi.h>
#include <windows.h>

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND,
                                                             UINT,
                                                             WPARAM,
                                                             LPARAM);

using wgl_swap_t = BOOL(WINAPI*)(HDC);

static wgl_swap_t        real_wgl_swap = nullptr;
static HWND              game_window   = nullptr;
static WNDPROC           orig_wnd_proc = nullptr;
static std::atomic<bool> imgui_ready   = false;
static std::atomic<bool> shutting_down = false;
static BYTE              original_bytes[5];
static ImVec4            clear_color  = ImVec4(0, 0, 0, 0);
static bool              enable_clear = false;
static bool              wireframe    = false;

struct hook_entry final
{
    std::string module, function;
    void *      original_addr, *hook_addr;
    bool        active;
};

static std::vector<hook_entry> g_hooks;

struct console_buffer final
{
    ImGuiTextBuffer buf;
    ImGuiTextFilter filter;
    ImVector<int>
        line_offsets; // ImVector is POD-like, but std::vector is often safer
                      // for C++. keeping ImVector to satisfy "interface" look.

    // Optimization: Cache indices of lines that match the filter
    // to avoid scanning the whole string buffer every frame.
    std::vector<int> filtered_indices;

    bool auto_scroll = true;
    mutable std::mutex
        mtx; // Mutable to allow locking in const functions if needed

    // Constructor to ensure initial state is valid
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
        // OPTIMIZATION: Early reserve if likely to grow
        // (ImGuiTextBuffer doesn't expose reserve, but we can reserve our
        // vectors)

        std::lock_guard lock(mtx);

        int old_size = buf.size();

        va_list args;
        va_start(args, fmt);
        buf.appendfv(fmt, args);
        va_end(args);

        const int   new_size     = buf.size();
        const char* buffer_start = buf.begin();

        // OPTIMIZATION: Fast scan for newlines only in the appended chunk
        for (int i = old_size; i < new_size; ++i)
        {
            if (buffer_start[i] == '\n')
            {
                line_offsets.push_back(i + 1);

                // OPTIMIZATION: Incremental filter update.
                // If filter is active, check ONLY this new line immediately.
                if (filter.IsActive())
                {
                    // The line starts at the *previous* offset (which was the
                    // end of the last line) The last entry in line_offsets is
                    // the *start* of the *next* line (i+1). So the current line
                    // started at line_offsets[size - 2].
                    if (line_offsets.Size >= 2)
                    {
                        const int line_start_idx =
                            line_offsets[line_offsets.Size - 2];
                        const int line_end_idx = i; // exclude \n

                        if (filter.PassFilter(buffer_start + line_start_idx,
                                              buffer_start + line_end_idx))
                        {
                            // Store the index of the line in line_offsets
                            filtered_indices.push_back(line_offsets.Size - 2);
                        }
                    }
                }
            }
        }
    }

    void draw()
    {
        // UI State handling (no lock needed yet)
        if (ImGui::BeginPopup("options"))
        {
            ImGui::Checkbox("auto-scroll", &auto_scroll);
            ImGui::EndPopup();
        }

        if (ImGui::Button("options"))
            ImGui::OpenPopup("options");
        ImGui::SameLine();

        const bool clear_pressed = ImGui::Button("clear");
        ImGui::SameLine();
        const bool copy = ImGui::Button("copy");
        ImGui::SameLine();

        // Filter Draw returns true if the text changed.
        const bool filter_changed = filter.Draw("filter", -100.0f);
        ImGui::Separator();

        if (ImGui::BeginChild("scrolling",
                              ImVec2(0, 0),
                              false,
                              ImGuiWindowFlags_HorizontalScrollbar))
        {
            // Lock specifically for data access/manipulation
            std::lock_guard lock(mtx);

            if (clear_pressed)
            {
                buf.clear();
                line_offsets.clear();
                line_offsets.push_back(0);
                filtered_indices.clear();
            }

            if (copy)
                ImGui::LogToClipboard();

            // Rebuild cache if filter text changed
            if (filter_changed)
            {
                filtered_indices.clear();
                if (filter.IsActive())
                {
                    const char* buf_begin = buf.begin();

                    // Reserve to prevent reallocations during loop
                    filtered_indices.reserve(
                        static_cast<size_t>(line_offsets.Size));

                    for (int i = 0; i < line_offsets.Size - 1; i++)
                    {
                        const char* line_start = buf_begin + line_offsets[i];
                        const char* line_end =
                            buf_begin + line_offsets[i + 1] - 1;

                        if (filter.PassFilter(line_start, line_end))
                        {
                            filtered_indices.push_back(i);
                        }
                    }
                }
            }

            // RENDER LOGIC
            ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 0));
            const char* buf_begin = buf.begin();

            // OPTIMIZATION: Use Clipper for BOTH filtered and unfiltered views.
            // The previous code did not clip filtered views, causing massive
            // slowdowns on large logs.
            ImGuiListClipper clipper;

            if (filter.IsActive())
            {
                clipper.Begin(static_cast<int>(filtered_indices.size()));
                while (clipper.Step())
                {
                    for (int i = clipper.DisplayStart; i < clipper.DisplayEnd;
                         i++)
                    {
                        const int line_idx =
                            filtered_indices[static_cast<size_t>(i)];
                        const char* line_start =
                            buf_begin + line_offsets[line_idx];
                        const char* line_end =
                            buf_begin + line_offsets[line_idx + 1] - 1;

                        ImGui::TextUnformatted(line_start, line_end);
                    }
                }
            }
            else
            {
                // Size - 1 because the last offset is the "next write position"
                clipper.Begin(line_offsets.Size - 1);
                while (clipper.Step())
                {
                    for (int i = clipper.DisplayStart; i < clipper.DisplayEnd;
                         i++)
                    {
                        const char* line_start = buf_begin + line_offsets[i];
                        const char* line_end =
                            buf_begin + line_offsets[i + 1] - 1;

                        ImGui::TextUnformatted(line_start, line_end);
                    }
                }
            }
            clipper.End();

            ImGui::PopStyleVar();

            // Auto-scroll logic
            // Only auto-scroll if we are at the bottom AND the user hasn't
            // scrolled up manually
            if (auto_scroll && ImGui::GetScrollY() >= ImGui::GetScrollMaxY())
                [[likely]]
            {
                ImGui::SetScrollHereY(1.0f);
            }
        }
        ImGui::EndChild();
    }
};

static console_buffer console;

static void log_msg(const char* fmt, ...)
{
    char buffer[2048];

    const auto now   = std::chrono::system_clock::now();
    const auto now_t = std::chrono::system_clock::to_time_t(now);
    tm         tm_info{};

    localtime_s(&tm_info, &now_t);

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

    va_list args{};
    va_start(args, fmt);

    const size_t remaining_size = sizeof(buffer) - static_cast<size_t>(offset);

    std::vsnprintf(buffer + offset, remaining_size, fmt, args);
    va_end(args);

    console.add_log("%s\n", buffer);
}

static void report_winapi_error(const char* operation, DWORD error_code)
{
    LPSTR message_buffer = nullptr;

    const DWORD format_result = FormatMessageA(
        FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM |
            FORMAT_MESSAGE_IGNORE_INSERTS,
        nullptr,
        error_code,
        MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
        reinterpret_cast<LPSTR>(&message_buffer),
        0,
        nullptr);

    if (format_result && message_buffer)
    {
        log_msg("error: %s: %s (%lu)", operation, message_buffer, error_code);
        LocalFree(message_buffer);
    }
    else
    {
        log_msg(
            "error: %s: error code %lu (format failed)", operation, error_code);
    }
}

struct system_info final
{
    std::string cpu_arch, os_version;
    uint64_t    total_ram;
} g_sysinfo;

static void init_system_info()
{
    log_msg("initializing system info...");

    constexpr unsigned long long mb_divisor = 1024 * 1024;

    SYSTEM_INFO si;
    GetNativeSystemInfo(&si);

    switch (si.wProcessorArchitecture)
    {
        case PROCESSOR_ARCHITECTURE_AMD64:
            g_sysinfo.cpu_arch = "x64";
            break;
        case PROCESSOR_ARCHITECTURE_INTEL:
            g_sysinfo.cpu_arch = "x86";
            break;
        case PROCESSOR_ARCHITECTURE_ARM64:
            g_sysinfo.cpu_arch = "arm64";
            break;
        default:
            g_sysinfo.cpu_arch = "unknown";
            break;
    }

    log_msg("cpu architecture: %s", g_sysinfo.cpu_arch.c_str());

    // DWORD is unsigned long
    log_msg("processor count: %lu", si.dwNumberOfProcessors);
    log_msg("page size: %lu bytes", si.dwPageSize);

    // 2. RAM Info
    MEMORYSTATUSEX ms{};
    ms.dwLength = sizeof(ms);

    if (GlobalMemoryStatusEx(&ms))
    {
        // Calculate in MB using strict casting to prevent warnings
        g_sysinfo.total_ram = ms.ullTotalPhys / mb_divisor;

        const auto avail_mb = ms.ullAvailPhys / mb_divisor;

        log_msg("total ram: %lluMB", g_sysinfo.total_ram);
        log_msg("available ram: %lluMB", avail_mb);
        log_msg("memory load: %lu%%", ms.dwMemoryLoad);
    }
    else
    {
        report_winapi_error("GlobalMemoryStatusEx", GetLastError());
        g_sysinfo.total_ram = 0;
    }

    // 3. OS Version (Advanced Lookup)
    // We use RtlGetVersion from ntdll to bypass manifest lies.
    HMODULE ntdll = GetModuleHandleA("ntdll.dll");
    if (ntdll)
    {
        // Define function signature locally
        using RtlGetVersionPtr = LONG(WINAPI*)(PRTL_OSVERSIONINFOW);
        auto rtl_get_version   = reinterpret_cast<RtlGetVersionPtr>(
            reinterpret_cast<void*>(GetProcAddress(ntdll, "RtlGetVersion")));

        if (rtl_get_version)
        {
            RTL_OSVERSIONINFOW rovi{};
            rovi.dwOSVersionInfoSize = sizeof(rovi);
            if (rtl_get_version(&rovi) == 0) // STATUS_SUCCESS
            {
                char ver_buf[64];
                std::snprintf(ver_buf,
                              sizeof(ver_buf),
                              "Windows %lu.%lu (Build %lu)",
                              rovi.dwMajorVersion,
                              rovi.dwMinorVersion,
                              rovi.dwBuildNumber);
                g_sysinfo.os_version = ver_buf;
            }
        }
    }

    // Fallback if ntdll failed
    if (g_sysinfo.os_version.empty())
    {
        g_sysinfo.os_version = "Windows (Unknown)";
    }
    log_msg("os version: %s", g_sysinfo.os_version.c_str());

    // 4. Process Memory Info
    const HANDLE            process = GetCurrentProcess();
    PROCESS_MEMORY_COUNTERS pmc;

    if (GetProcessMemoryInfo(process, &pmc, sizeof(pmc)))
    {
        // SIZE_T can be 32 or 64 bit. We cast to explicit u64 for math safety,
        // then to u32 (unsigned long) for printing since MB usually fits in 32
        // bits.
        const auto working_set_mb = static_cast<unsigned long>(
            static_cast<unsigned long long>(pmc.WorkingSetSize) / mb_divisor);

        const auto peak_working_set_mb = static_cast<unsigned long>(
            static_cast<unsigned long long>(pmc.PeakWorkingSetSize) /
            mb_divisor);

        log_msg("process working set: %luMB", working_set_mb);
        log_msg("process peak working set: %luMB", peak_working_set_mb);
    }

    // 5. Process ID & Path
    const DWORD process_id = GetCurrentProcessId();
    log_msg("process id: %lu", process_id);

    char module_path[MAX_PATH] = { 0 };
    // Returns length excluding null-terminator.
    const DWORD len = GetModuleFileNameA(nullptr, module_path, MAX_PATH);
    if (len > 0 && len < MAX_PATH)
    {
        log_msg("module path: %s", module_path);
    }
    else
    {
        // Handle potential truncation or error
        if (GetLastError() == ERROR_INSUFFICIENT_BUFFER)
            log_msg("module path: <too long>");
        else
            log_msg("module path: <unknown>");
    }

    log_msg("system info initialized");
}

static LRESULT CALLBACK wnd_proc(HWND h, UINT m, WPARAM w, LPARAM l)
{
    if (!shutting_down.load())
    {
        ImGui_ImplWin32_WndProcHandler(h, m, w, l);
    }
    return CallWindowProc(orig_wnd_proc, h, m, w, l);
}

static void draw_overlay();

[[nodiscard]] static bool WINAPI hook_swap(HDC dc) noexcept
{
    if (imgui_ready.load(std::memory_order::acquire)) [[likely]]
    {
        draw_overlay();
        return real_wgl_swap(dc);
    }

    static std::once_flag init_flag;

    if (wglGetCurrentContext())
    {
        std::call_once(
            init_flag,
            [&]()
            {
                log_msg("initializing opengl hook...");

                // C++26: constexpr array for viewport size
                constexpr int viewport_size           = 4;
                GLint         viewport[viewport_size] = { 0 };
                glGetIntegerv(GL_VIEWPORT, viewport);

                log_msg("opengl viewport: %dx%d at (%d,%d)",
                        viewport[2],
                        viewport[3],
                        viewport[0],
                        viewport[1]);

                auto get_gl_string = [](GLenum name) -> const char*
                {
                    const auto* res =
                        reinterpret_cast<const char*>(glGetString(name));
                    return res ? res : "unknown";
                };

                log_msg("opengl version: %s", get_gl_string(GL_VERSION));
                log_msg("opengl vendor: %s", get_gl_string(GL_VENDOR));
                log_msg("opengl renderer: %s", get_gl_string(GL_RENDERER));

                game_window = WindowFromDC(wglGetCurrentDC());
                if (!game_window)
                {
                    log_msg("error: WindowFromDC failed");
                    return; // Abort init, try again next frame
                }

                RECT window_rect{};
                if (GetWindowRect(game_window, &window_rect))
                {
                    log_msg("window rect: %ldx%ld at (%ld,%ld)",
                            window_rect.right - window_rect.left,
                            window_rect.bottom - window_rect.top,
                            window_rect.left,
                            window_rect.top);
                }

                // Stack buffer for title is fine, explicit zero-init for safety
                char window_title[256] = { 0 };
                if (GetWindowTextA(
                        game_window, window_title, sizeof(window_title) - 1))
                {
                    log_msg("window title: %s", window_title);
                }

                // Hook Window Procedure
                // Using SetWindowLongPtrA with robust casting
                orig_wnd_proc = reinterpret_cast<WNDPROC>(
                    SetWindowLongPtrA(game_window,
                                      GWLP_WNDPROC,
                                      reinterpret_cast<LONG_PTR>(wnd_proc)));

                if (!orig_wnd_proc)
                {
                    // Retrieve error once
                    const DWORD err = GetLastError();
                    report_winapi_error("SetWindowLongPtrA", err);
                }
                else
                {
                    log_msg("hooked window procedure at %p",
                            reinterpret_cast<void*>(orig_wnd_proc));
                }

                // Initialize ImGui
                log_msg("initializing imgui...");

                ImGui::CreateContext();
                ImGuiIO& io        = ImGui::GetIO();
                io.FontGlobalScale = 1.2f;
                log_msg("imgui context created");

                if (!ImGui_ImplWin32_Init(game_window))
                {
                    log_msg("error: ImGui_ImplWin32_Init failed");
                    return;
                }

                log_msg("imgui win32 backend initialized");

                if (!ImGui_ImplOpenGL3_Init("#version 330 core"))
                {
                    log_msg("error: ImGui_ImplOpenGL3_Init failed");
                    return;
                }

                log_msg("imgui opengl3 backend initialized");

                // Setup system info now that ImGui is ready
                init_system_info();

                // Register the hook logic for safe unhooking later
                // Using GetModuleHandleA instead of LoadLibrary to avoid loader
                // lock issues
                HMODULE opengl_mod = GetModuleHandleA("opengl32.dll");
                if (opengl_mod)
                {
                    const auto orig_addr = reinterpret_cast<void*>(
                        GetProcAddress(opengl_mod, "wglSwapBuffers"));

                    if (orig_addr)
                    {
                        g_hooks.push_back(
                            { "opengl32.dll",
                              "wglSwapBuffers",
                              orig_addr,
                              reinterpret_cast<void*>(
                                  &hook_swap), // Address of this function
                              true });
                        log_msg("registered hook: wglSwapBuffers at %p",
                                orig_addr);
                    }
                }

                // Release barrier: Ensure all initialization above is visible
                // before other threads see imgui_ready = true.
                imgui_ready.store(true, std::memory_order::release);
            });
    }

    // Double-check atomic after init attempt to draw immediately on first frame
    if (imgui_ready.load(std::memory_order::acquire))
    {
        draw_overlay();
    }

    return real_wgl_swap(dc);
}

void install_hook()
{
    log_msg("bass proxy dll loaded");
    log_msg("installing opengl hook...");

    // 1. Locate Module
    // Use GetModuleHandleA to avoid incrementing the ref count (optimization)
    const HMODULE opengl_module = GetModuleHandleA("opengl32.dll");
    if (!opengl_module)
    {
        report_winapi_error("GetModuleHandleA(opengl32.dll)", GetLastError());
        return;
    }
    log_msg("opengl32.dll module handle: %p",
            static_cast<void*>(opengl_module));

    // 2. Locate Function
    // Strict cast to void* first, then byte pointer for arithmetic
    void* target_void = reinterpret_cast<void*>(
        GetProcAddress(opengl_module, "wglSwapBuffers"));

    if (!target_void)
    {
        report_winapi_error("GetProcAddress(wglSwapBuffers)", GetLastError());
        return;
    }

    // Use uint8_t for byte-level access (modern C++ standard practice)
    auto* target_addr = static_cast<uint8_t*>(target_void);

    log_msg("wglSwapBuffers target address: %p", target_void);
    log_msg("original bytes: %02x %02x %02x %02x %02x",
            target_addr[0],
            target_addr[1],
            target_addr[2],
            target_addr[3],
            target_addr[4]);

    // Safety Check: Detect if already hooked (starts with JMP 0xE9)
    if (target_addr[0] == 0xE9)
    {
        log_msg("warning: wglSwapBuffers appears to be already hooked!");
    }

    // 3. Alloc Trampoline
    // We allocate a small executable block.
    // MEM_COMMIT | MEM_RESERVE is standard. PAGE_EXECUTE_READWRITE is required
    // to write to it. Ideally, we would remap to PAGE_EXECUTE_READ after
    // writing, but keeping it RWX simplifies the trampoline logic for this
    // specific use case.
    void* trampoline_void = VirtualAlloc(
        nullptr, 64, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);

    if (!trampoline_void)
    {
        report_winapi_error("VirtualAlloc", GetLastError());
        return;
    }

    auto* trampoline = static_cast<uint8_t*>(trampoline_void);
    log_msg("trampoline allocated at: %p", trampoline_void);

    // 4. Modify Target Protection
    DWORD old_protect = 0;
    if (!VirtualProtect(target_void, 5, PAGE_EXECUTE_READWRITE, &old_protect))
    {
        report_winapi_error("VirtualProtect", GetLastError());
        // Leak trampoline memory here is acceptable as we are aborting critical
        // init
        return;
    }
    log_msg("memory protection changed, old protection: 0x%lx", old_protect);

    // 5. Build Trampoline
    // Copy original stolen bytes
    std::memcpy(trampoline, target_addr, 5);     // Copy to trampoline
    std::memcpy(original_bytes, target_addr, 5); // Backup globally

    // Write JMP back to original code
    // Trampoline layout: [5 bytes stolen] [1 byte 0xE9] [4 bytes offset]
    trampoline[5] = 0xE9;

    // Calculate offset: Destination - (Source + 5)
    // Destination: target_addr + 5 (instruction after the hook)
    // Source: trampoline + 5 (the JMP instruction) -> JMP relative is
    // calculated from next instruction (trampoline + 10)
    const uintptr_t dest_back = reinterpret_cast<uintptr_t>(target_addr) + 5;
    const uintptr_t src_back  = reinterpret_cast<uintptr_t>(trampoline) + 10;
    const int32_t   rel_back  = static_cast<int32_t>(dest_back - src_back);

    std::memcpy(trampoline + 6, &rel_back, sizeof(rel_back));
    log_msg("trampoline back jump offset: 0x%x", rel_back);

    // 6. Install Hook on Target
    // Instruction: JMP <hook_swap>
    target_addr[0] = 0xE9;

    // Calculate offset: Destination - (Source + 5)
    const uintptr_t dest_hook = reinterpret_cast<uintptr_t>(hook_swap);
    const uintptr_t src_hook  = reinterpret_cast<uintptr_t>(target_addr) + 5;
    const int32_t   rel_hook  = static_cast<int32_t>(dest_hook - src_hook);

    std::memcpy(target_addr + 1, &rel_hook, sizeof(rel_hook));
    log_msg("installed jump instruction, relative offset: 0x%x", rel_hook);

    // 7. Restore Protection & Flush Cache
    DWORD dummy = 0;
    if (!VirtualProtect(target_void, 5, old_protect, &dummy))
    {
        report_winapi_error("VirtualProtect restore", GetLastError());
    }
    else
    {
        log_msg("memory protection restored");
    }

    // CRITICAL: Flush instruction cache.
    // Modern CPUs separate Data and Instruction caches. We just wrote data that
    // is code. Without this, the CPU might execute the old cached instructions
    // or garbage.
    FlushInstructionCache(GetCurrentProcess(), target_void, 5);
    FlushInstructionCache(GetCurrentProcess(), trampoline_void, 10);

    // 8. Assign Global Pointer
    // Cast the trampoline address to the function signature
    real_wgl_swap = reinterpret_cast<wgl_swap_t>(trampoline);

    log_msg("hook installation completed successfully");
}

void remove_hooks()
{
    log_msg("cleanup started");

    // ATOMICITY: Signal shutdown immediately.
    shutting_down.store(true, std::memory_order::seq_cst);

    // 1. Restore Window Procedure
    if (orig_wnd_proc && game_window && IsWindow(game_window))
    {
        SetWindowLongPtrA(game_window,
                          GWLP_WNDPROC,
                          reinterpret_cast<LONG_PTR>(orig_wnd_proc));

        orig_wnd_proc = nullptr;
        log_msg("window procedure restored");
    }

    // 2. Restore OpenGL Hook
    const HMODULE opengl_module = GetModuleHandleA("opengl32.dll");
    if (opengl_module)
    {
        void* proc_addr = reinterpret_cast<void*>(
            GetProcAddress(opengl_module, "wglSwapBuffers"));

        if (proc_addr)
        {
            DWORD            old_protect = 0;
            constexpr size_t patch_size  = 5;

            if (VirtualProtect(proc_addr,
                               patch_size,
                               PAGE_EXECUTE_READWRITE,
                               &old_protect))
            {
                std::memcpy(proc_addr, original_bytes, patch_size);

                DWORD dummy = 0;
                VirtualProtect(proc_addr, patch_size, old_protect, &dummy);

                // Flush instruction cache to ensure CPU executes restored bytes
                FlushInstructionCache(
                    GetCurrentProcess(), proc_addr, patch_size);

                log_msg("original bytes restored");
            }
            else
            {
                report_winapi_error("VirtualProtect", GetLastError());
            }
        }
    }

    // 3. Free Trampoline
    // FIX: Added reinterpret_cast<LPVOID> to satisfy VirtualFree signature.
    if (auto* trampoline = std::exchange(real_wgl_swap, nullptr); trampoline)
    {
        if (!VirtualFree(reinterpret_cast<LPVOID>(trampoline), 0, MEM_RELEASE))
        {
            report_winapi_error("VirtualFree", GetLastError());
        }
        else
        {
            log_msg("trampoline memory freed");
        }
    }

    // 4. ImGui Shutdown
    if (imgui_ready.exchange(false, std::memory_order::acq_rel))
    {
        if (ImGui::GetCurrentContext())
        {
            ImGui_ImplOpenGL3_Shutdown();
            ImGui_ImplWin32_Shutdown();
            ImGui::DestroyContext();
            log_msg("imgui shutdown completed");
        }
    }

    log_msg("cleanup completed");
}

static void draw_overlay()
{
    if (!ImGui::GetCurrentContext()) [[unlikely]]
        return;

    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();

    // SAFETY: Thread-safe one-time style initialization.
    static std::once_flag init_style_flag;
    std::call_once(init_style_flag,
                   []()
                   {
                       ImGui::StyleColorsDark();
                       auto& style          = ImGui::GetStyle();
                       style.WindowRounding = 6.0f;
                       style.FrameRounding  = 4.0f;
                       style.WindowPadding  = ImVec2(8.0f, 8.0f);
                       style.FramePadding   = ImVec2(6.0f, 4.0f);
                       style.ItemSpacing    = ImVec2(6.0f, 4.0f);
                       log_msg("imgui style initialized");
                   });

    constexpr float win_alpha = 0.9f;

    ImGui::SetNextWindowBgAlpha(win_alpha);
    ImGui::SetNextWindowPos(ImVec2(10, 10), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(600, 450), ImGuiCond_FirstUseEver);

    if (ImGui::Begin("bass proxy overlay", nullptr))
    {
        if (ImGui::BeginTabBar("main_tabs"))
        {
            // --- Console Tab ---
            if (ImGui::BeginTabItem("console"))
            {
                console.draw();
                ImGui::EndTabItem();
            }

            // --- Render Tab ---
            if (ImGui::BeginTabItem("render"))
            {
                // Handle Wireframe Logic
                // Using static to track state allows us to only call
                // glPolygonMode when changed.
                static bool current_wireframe_state = false;

                // Sync internal state if changed externally
                if (wireframe != current_wireframe_state)
                {
                    current_wireframe_state = wireframe;
                }

                ImGui::Checkbox("clear screen", &enable_clear);
                ImGui::SameLine();

                // SAFETY: Access .x directly. &clear_color (struct) -> float*
                // is technically UB.
                ImGui::ColorEdit3("clear color",
                                  &clear_color.x,
                                  ImGuiColorEditFlags_NoInputs |
                                      ImGuiColorEditFlags_DisplayRGB);

                // Optimization: Only call GL command if the checkbox was
                // actually clicked
                if (ImGui::Checkbox("wireframe", &wireframe))
                {
                    if (wireframe != current_wireframe_state)
                    {
                        glPolygonMode(GL_FRONT_AND_BACK,
                                      wireframe ? GL_LINE : GL_FILL);
                        current_wireframe_state = wireframe;
                        log_msg("wireframe mode: %s",
                                wireframe ? "enabled" : "disabled");
                    }
                }

                ImGui::Separator();
                ImGui::TextUnformatted("opengl state:");

                // OPTIMIZATION: Only query GL state when this tab is actually
                // visible. These calls are expensive (driver overhead).
                constexpr int viewport_count = 4;
                GLint         viewport[viewport_count];
                glGetIntegerv(GL_VIEWPORT, viewport);

                // C++20/26 formatted output helper within ImGui
                ImGui::Text("viewport: %dx%d at (%d,%d)",
                            viewport[2],
                            viewport[3],
                            viewport[0],
                            viewport[1]);

                GLboolean depth_test = GL_FALSE;
                glGetBooleanv(GL_DEPTH_TEST, &depth_test);
                ImGui::Text("depth test: %s",
                            depth_test ? "enabled" : "disabled");

                GLboolean blend = GL_FALSE;
                glGetBooleanv(GL_BLEND, &blend);
                ImGui::Text("blend: %s", blend ? "enabled" : "disabled");

                ImGui::EndTabItem();
            }

            // --- Hooks Tab ---
            if (ImGui::BeginTabItem("hooks"))
            {
                static ImGuiTextFilter filter;
                filter.Draw("filter", -100.0f);

                // Table Setup
                constexpr int column_count = 4;
                if (ImGui::BeginTable("hooks_table",
                                      column_count,
                                      ImGuiTableFlags_Borders |
                                          ImGuiTableFlags_RowBg |
                                          ImGuiTableFlags_ScrollY))
                {
                    ImGui::TableSetupColumn("module");
                    ImGui::TableSetupColumn("function");
                    ImGui::TableSetupColumn("original");
                    ImGui::TableSetupColumn("status");
                    ImGui::TableHeadersRow();

                    // OPTIMIZATION: ImGuiListClipper
                    // If g_hooks has 1000 entries, we only render the ~20
                    // visible ones. This massively reduces CPU usage for lists.
                    ImGuiListClipper clipper;
                    clipper.Begin(static_cast<int>(g_hooks.size()));

                    while (clipper.Step())
                    {
                        for (int i = clipper.DisplayStart;
                             i < clipper.DisplayEnd;
                             i++)
                        {
                            const auto& hook = g_hooks[static_cast<size_t>(i)];

                            // Filter check
                            if (!filter.PassFilter(hook.module.c_str()) &&
                                !filter.PassFilter(hook.function.c_str()))
                            {
                                continue;
                            }

                            ImGui::TableNextRow();

                            ImGui::TableNextColumn();
                            ImGui::TextUnformatted(hook.module.c_str());

                            ImGui::TableNextColumn();
                            ImGui::TextUnformatted(hook.function.c_str());

                            ImGui::TableNextColumn();
                            ImGui::Text("%p", hook.original_addr);

                            ImGui::TableNextColumn();
                            if (hook.active)
                            {
                                ImGui::TextColored(
                                    ImVec4(0.2f, 0.8f, 0.3f, 1.0f), "active");
                            }
                            else
                            {
                                ImGui::TextColored(
                                    ImVec4(0.8f, 0.2f, 0.2f, 1.0f), "inactive");
                            }
                        }
                    }
                    // clipper.End() is called automatically by destructor

                    ImGui::EndTable();
                }
                ImGui::EndTabItem();
            }

            ImGui::EndTabBar();
        }

        // --- Exit Button ---
        ImGui::Separator();

        // Red button styling
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.8f, 0.2f, 0.2f, 0.8f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered,
                              ImVec4(0.9f, 0.3f, 0.3f, 0.9f));

        if (ImGui::Button("exit"))
        {
            log_msg("exit requested by user");
            // Use 0 as exit code, safer than undefined
            PostQuitMessage(0);
        }

        ImGui::PopStyleColor(2);
    }
    ImGui::End();

    ImGui::Render();

    // Clear Logic
    if (enable_clear) [[unlikely]]
    {
        glClearColor(
            clear_color.x, clear_color.y, clear_color.z, clear_color.w);
        glClear(GL_COLOR_BUFFER_BIT);
    }

    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
}