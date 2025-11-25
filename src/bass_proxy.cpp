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
using create_file_a_t    = HANDLE(WINAPI*)(LPCSTR, DWORD, DWORD, LPSECURITY_ATTRIBUTES, DWORD, DWORD, HANDLE);
using gl_draw_elems_t    = void(APIENTRY*)(GLenum, GLsizei, GLenum, const GLvoid*);
using gl_viewport_t      = void(APIENTRY*)(GLint, GLint, GLsizei, GLsizei);
using gl_clear_t         = void(APIENTRY*)(GLbitfield);
using gl_matrix_mode_t   = void(APIENTRY*)(GLenum);

// Game function types
using video_set_resolution_t = void(__cdecl*)(int mode);
using game_update_mouse_t    = void(__cdecl*)(int raw_x, int raw_y);
using ui_update_selection_t  = void(__cdecl*)(void);

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND, UINT, WPARAM, LPARAM);

// Game memory addresses (from Ghidra analysis)
constexpr uintptr_t ADDR_VIDEO_SET_RESOLUTION = 0x00401000;
constexpr uintptr_t ADDR_G_SCREEN_WIDTH       = 0x00441874;
constexpr uintptr_t ADDR_G_SCREEN_HEIGHT      = 0x00441878;
constexpr uintptr_t ADDR_G_VIDEO_MODE         = 0x00441870;
constexpr uintptr_t ADDR_MOUSE_SCALE          = 0x004e53e8;  // float: base_ui_width / screen_width
constexpr uintptr_t ADDR_UI_BASE_WIDTH        = 0x00438088;  // float: base UI width (800.0f)
constexpr uintptr_t ADDR_GAME_UPDATE_MOUSE    = 0x0040ad30;  // void game_update_mouse(int raw_x, int raw_y)
constexpr uintptr_t ADDR_G_MOUSE_X            = 0x004e53d0;  // int: scaled mouse X
constexpr uintptr_t ADDR_G_MOUSE_Y            = 0x004e53d4;  // int: scaled mouse Y
constexpr uintptr_t ADDR_UI_UPDATE_SELECTION  = 0x00428b20;  // void ui_update_selection(void)
constexpr uintptr_t ADDR_MOUSE_ACTIVE_FLAG    = 0x004e53f4;  // byte: flag checked before processing mouse
constexpr uintptr_t ADDR_UI_LOAD_RESOURCES    = 0x004288d0;  // void ui_load_resources(void) - recalculates mouse scale

// Base UI dimensions (the game's internal coordinate system)
constexpr float UI_BASE_WIDTH  = 800.0f;
constexpr float UI_BASE_HEIGHT = 600.0f;

struct visual_settings final {
    std::atomic<bool> disable_depth{ false };
    std::atomic<bool> wireframe{ false };
    std::atomic<bool> fog_override{ false };
    
    ImVec4 clear_color{ 0.0f, 0.0f, 0.0f, 0.0f };
    ImVec4 fog_color{ 0.5f, 0.6f, 0.7f, 1.0f };
    bool   enable_clear{ false };
};

struct resolution_settings final {
    std::atomic<bool> custom_enabled{ false };
    std::atomic<int>  custom_width{ 1920 };
    std::atomic<int>  custom_height{ 1080 };
    std::atomic<bool> applied{ false };
};

struct gameplay_settings final {
    std::atomic<float> speed_multiplier{ 1.0f };
    std::atomic<bool>  block_mouse{ false };
};

struct debug_settings final {
    std::atomic<bool> log_fs{ false };
    std::atomic<bool> log_gl_calls{ false };
    std::atomic<bool> log_mouse{ false };
    std::atomic<bool> show_metrics{ false };
    std::atomic<bool> show_demo{ false };
};

struct hot_reload_settings final {
    std::atomic<bool> enabled{ true };
    std::atomic<bool> checking{ false };
    std::string       watch_path;
    FILETIME          last_write_time{};
    HMODULE           hooks_module{ nullptr };
};

struct global_context final {
    HWND              game_window{ nullptr };
    WNDPROC           orig_wnd_proc{ nullptr };
    std::atomic<bool> imgui_ready{ false };
    std::atomic<bool> shutting_down{ false };
    std::atomic<bool> overlay_visible{ true };
    
    visual_settings      visuals;
    resolution_settings  resolution;
    gameplay_settings    gameplay;
    debug_settings       debug;
    hot_reload_settings  hot_reload;
    
    std::atomic<int> frame_count{ 0 };
    std::atomic<int> draw_call_count{ 0 };
    
    GLint  current_viewport[4]{ 0, 0, 800, 600 };
    GLenum current_matrix_mode{ GL_MODELVIEW };
    
    // For debugging mouse
    std::atomic<int> last_raw_x{ 0 };
    std::atomic<int> last_raw_y{ 0 };
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

// Hot reload helper functions
static std::string get_dll_directory() {
    char path[MAX_PATH];
    HMODULE hm = nullptr;
    if (GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                           reinterpret_cast<LPCSTR>(&get_dll_directory), &hm)) {
        GetModuleFileNameA(hm, path, MAX_PATH);
        std::string dir(path);
        size_t pos = dir.find_last_of("\\/");
        if (pos != std::string::npos) {
            return dir.substr(0, pos + 1);
        }
    }
    return "";
}

static FILETIME get_file_write_time(const std::string& path) {
    FILETIME ft{};
    HANDLE hFile = CreateFileA(path.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE,
                               nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (hFile != INVALID_HANDLE_VALUE) {
        GetFileTime(hFile, nullptr, nullptr, &ft);
        CloseHandle(hFile);
    }
    return ft;
}

static bool file_times_equal(const FILETIME& a, const FILETIME& b) {
    return a.dwLowDateTime == b.dwLowDateTime && a.dwHighDateTime == b.dwHighDateTime;
}

static bool try_hot_reload();  // Forward declaration

static void hot_reload_check() {
    if (!ctx.hot_reload.enabled.load() || ctx.hot_reload.checking.exchange(true)) {
        return;  // Already checking or disabled
    }
    
    std::string reload_path = get_dll_directory() + "bass_hooks.dll";
    
    FILETIME current_time = get_file_write_time(reload_path);
    
    // Check if file exists and has changed
    if (current_time.dwLowDateTime != 0 || current_time.dwHighDateTime != 0) {
        if (!file_times_equal(current_time, ctx.hot_reload.last_write_time)) {
            if (ctx.hot_reload.last_write_time.dwLowDateTime != 0 || 
                ctx.hot_reload.last_write_time.dwHighDateTime != 0) {
                // File changed, try to reload
                logger.log("hot_reload => detected change in bass_hooks.dll");
                try_hot_reload();
            }
            ctx.hot_reload.last_write_time = current_time;
        }
    }
    
    ctx.hot_reload.checking.store(false);
}

using hooks_func_t = void(*)();

static bool try_hot_reload() {
    std::string reload_path = get_dll_directory() + "bass_hooks.dll";
    
    // Try to load the new DLL to validate it first
    HMODULE test_module = LoadLibraryA(reload_path.c_str());
    if (!test_module) {
        DWORD error = GetLastError();
        logger.log("hot_reload => FAILED to load bass_hooks.dll (error {})", error);
        return false;
    }
    
    // Check for required exports
    auto new_install = reinterpret_cast<hooks_func_t>(GetProcAddress(test_module, "install_hooks"));
    auto new_uninstall = reinterpret_cast<hooks_func_t>(GetProcAddress(test_module, "uninstall_hooks"));
    
    if (!new_install || !new_uninstall) {
        logger.log("hot_reload => bass_hooks.dll missing 'install_hooks' or 'uninstall_hooks' export");
        FreeLibrary(test_module);
        return false;
    }
    
    // Unload old module if any
    if (ctx.hot_reload.hooks_module) {
        auto old_uninstall = reinterpret_cast<hooks_func_t>(
            GetProcAddress(ctx.hot_reload.hooks_module, "uninstall_hooks"));
        if (old_uninstall) {
            logger.log("hot_reload => calling uninstall_hooks on old module");
            old_uninstall();
        }
        
        // Small delay to let hooks fully unwind
        Sleep(100);
        
        FreeLibrary(ctx.hot_reload.hooks_module);
        ctx.hot_reload.hooks_module = nullptr;
        logger.log("hot_reload => old module unloaded");
    }
    
    // Install hooks from new module
    ctx.hot_reload.hooks_module = test_module;
    logger.log("hot_reload => calling install_hooks on new module");
    new_install();
    
    logger.log("hot_reload => SUCCESS - bass_hooks.dll reloaded");
    return true;
}

static wgl_swap_t              orig_wgl_swap              = nullptr;
static qpc_t                   orig_qpc                   = nullptr;
static set_cursor_t            orig_set_cursor            = nullptr;
static create_file_a_t         orig_create_file_a         = nullptr;
static gl_draw_elems_t         orig_gl_draw_elems         = nullptr;
static gl_viewport_t           orig_gl_viewport           = nullptr;
static gl_clear_t              orig_gl_clear              = nullptr;
static gl_matrix_mode_t        orig_gl_matrix_mode        = nullptr;
static video_set_resolution_t  orig_video_set_resolution  = nullptr;
static game_update_mouse_t     orig_game_update_mouse     = nullptr;

// Pointers to game globals
static int*   g_screen_width_ptr    = nullptr;
static int*   g_screen_height_ptr   = nullptr;
static int*   g_video_mode_ptr      = nullptr;
static float* g_mouse_scale_ptr     = nullptr;
static float* g_ui_base_width_ptr   = nullptr;
static int*   g_mouse_x_ptr         = nullptr;
static int*   g_mouse_y_ptr         = nullptr;
static char*  g_mouse_active_ptr    = nullptr;  // Flag that must be non-zero for mouse processing

// Function pointers for game UI functions  
static ui_update_selection_t ui_update_selection_fn = nullptr;
static ui_update_selection_t ui_load_resources_fn   = nullptr;  // Same signature (void)(void)

static void             draw_ui();
static LRESULT CALLBACK detour_wnd_proc(HWND h, UINT m, WPARAM w, LPARAM l);

// Hook for video_set_resolution - allows custom resolutions
static void __cdecl detour_video_set_resolution(int mode) {
    if (ctx.resolution.custom_enabled.load(std::memory_order_relaxed)) {
        const int width  = ctx.resolution.custom_width.load(std::memory_order_relaxed);
        const int height = ctx.resolution.custom_height.load(std::memory_order_relaxed);
        
        if (g_screen_width_ptr && g_screen_height_ptr) {
            *g_screen_width_ptr  = width;
            *g_screen_height_ptr = height;
            // Mouse scale will be updated by ui_load_resources after this, 
            // but we track that we want custom resolution
            logger.log("resolution => custom {}x{} applied (mode was {})", width, height, mode);
            ctx.resolution.applied.store(true, std::memory_order_relaxed);
            return;
        }
    }
    
    // Call original function
    orig_video_set_resolution(mode);
    
    if (g_screen_width_ptr && g_screen_height_ptr) {
        logger.log("resolution => mode {} set to {}x{}", mode, *g_screen_width_ptr, *g_screen_height_ptr);
    }
}

// Custom game_update_mouse hook - properly scales X and Y independently for aspect ratio correction
static void __cdecl detour_game_update_mouse(int raw_x, int raw_y) {
    // Store raw values for debugging
    ctx.last_raw_x.store(raw_x, std::memory_order_relaxed);
    ctx.last_raw_y.store(raw_y, std::memory_order_relaxed);
    
    // Check the same flag the original function checks - only process if non-zero
    if (g_mouse_active_ptr && *g_mouse_active_ptr == 0) {
        return;  // Same behavior as original when flag is 0
    }
    
    if (!g_mouse_x_ptr || !g_mouse_y_ptr || !g_screen_width_ptr || !g_screen_height_ptr) {
        // Fallback to original if pointers aren't set
        if (orig_game_update_mouse) {
            orig_game_update_mouse(raw_x, raw_y);
        }
        return;
    }
    
    // Calculate separate X and Y scales
    // The game's UI is designed for 800x600 (UI_BASE_WIDTH x UI_BASE_HEIGHT)
    // We need to map screen coordinates to UI coordinates
    float scale_x = UI_BASE_WIDTH / static_cast<float>(*g_screen_width_ptr);
    float scale_y = UI_BASE_HEIGHT / static_cast<float>(*g_screen_height_ptr);
    
    // Scale the coordinates
    int scaled_x = static_cast<int>(static_cast<float>(raw_x) * scale_x);
    int scaled_y = static_cast<int>(static_cast<float>(raw_y) * scale_y);
    
    // Write directly to game's mouse position globals
    *g_mouse_x_ptr = scaled_x;
    *g_mouse_y_ptr = scaled_y;
    
    // Debug logging
    if (ctx.debug.log_mouse.load(std::memory_order_relaxed)) {
        static int log_counter = 0;
        if (++log_counter % 30 == 0) {  // Log every 30th call to avoid spam
            logger.log("mouse => raw({},{}) scaled({},{}) screen={}x{}", 
                       raw_x, raw_y, scaled_x, scaled_y, 
                       *g_screen_width_ptr, *g_screen_height_ptr);
        }
    }
    
    // Call ui_update_selection to update UI hover state (same as original function)
    if (ui_update_selection_fn) {
        ui_update_selection_fn();
    }
}

// Apply custom resolution directly to game memory (can be called anytime)
static void apply_custom_resolution() {
    if (!g_screen_width_ptr || !g_screen_height_ptr) {
        logger.log("resolution => game pointers not initialized");
        return;
    }
    
    const int width  = ctx.resolution.custom_width.load(std::memory_order_relaxed);
    const int height = ctx.resolution.custom_height.load(std::memory_order_relaxed);
    
    *g_screen_width_ptr  = width;
    *g_screen_height_ptr = height;
    
    // Resize the window if we have a handle
    if (ctx.game_window) {
        RECT rect{};
        GetWindowRect(ctx.game_window, &rect);
        
        DWORD style = static_cast<DWORD>(GetWindowLong(ctx.game_window, GWL_STYLE));
        RECT adjusted = { 0, 0, width, height };
        AdjustWindowRect(&adjusted, style, FALSE);
        
        int new_width  = adjusted.right - adjusted.left;
        int new_height = adjusted.bottom - adjusted.top;
        
        SetWindowPos(ctx.game_window, nullptr, rect.left, rect.top, new_width, new_height, 
                     SWP_NOZORDER | SWP_NOACTIVATE);
        
        logger.log("resolution => window resized to {}x{}", new_width, new_height);
    }
    
    // Call ui_load_resources to reinitialize the game's UI (recalculates mouse scale etc)
    if (ui_load_resources_fn) {
        logger.log("resolution => calling ui_load_resources to reinitialize UI");
        ui_load_resources_fn();
    }
    
    ctx.resolution.applied.store(true, std::memory_order_relaxed);
    logger.log("resolution => applied {}x{} (scale_x={:.4f}, scale_y={:.4f})", 
               width, height, 
               UI_BASE_WIDTH / static_cast<float>(width),
               UI_BASE_HEIGHT / static_cast<float>(height));
}

static void APIENTRY detour_gl_matrix_mode(GLenum mode) {
    ctx.current_matrix_mode = mode;
    orig_gl_matrix_mode(mode);
}

static BOOL WINAPI detour_wgl_swap(HDC dc) {
    ctx.frame_count.fetch_add(1, std::memory_order_relaxed);
    
    // Check for hot reload every 60 frames (~1 second at 60fps)
    if (ctx.frame_count.load() % 60 == 0) {
        hot_reload_check();
    }
    
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

static HANDLE WINAPI detour_create_file_a(
    LPCSTR file_name, DWORD access, DWORD share, LPSECURITY_ATTRIBUTES sec, DWORD disp, DWORD attr, HANDLE temp) {
    if (ctx.debug.log_fs.load(std::memory_order_relaxed)) {
        logger.log("fs => \"{}\"", file_name ? file_name : "NULL");
    }
    return orig_create_file_a(file_name, access, share, sec, disp, attr, temp);
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

PROXY_EXPORT void install_hooks() {
    logger.log("minhook => initializing library");
    
    MH_STATUS init_status = MH_Initialize();
    if (init_status != MH_OK) {
        logger.log("minhook => MH_Initialize failed: {}", MH_StatusToString(init_status));
        return;
    }

    // Initialize game memory pointers
    g_screen_width_ptr  = reinterpret_cast<int*>(ADDR_G_SCREEN_WIDTH);
    g_screen_height_ptr = reinterpret_cast<int*>(ADDR_G_SCREEN_HEIGHT);
    g_video_mode_ptr    = reinterpret_cast<int*>(ADDR_G_VIDEO_MODE);
    g_mouse_scale_ptr   = reinterpret_cast<float*>(ADDR_MOUSE_SCALE);
    g_ui_base_width_ptr = reinterpret_cast<float*>(ADDR_UI_BASE_WIDTH);
    g_mouse_x_ptr       = reinterpret_cast<int*>(ADDR_G_MOUSE_X);
    g_mouse_y_ptr       = reinterpret_cast<int*>(ADDR_G_MOUSE_Y);
    g_mouse_active_ptr  = reinterpret_cast<char*>(ADDR_MOUSE_ACTIVE_FLAG);
    ui_update_selection_fn = reinterpret_cast<ui_update_selection_t>(ADDR_UI_UPDATE_SELECTION);
    ui_load_resources_fn   = reinterpret_cast<ui_update_selection_t>(ADDR_UI_LOAD_RESOURCES);
    
    logger.log("game => screen_width @ {:08x}, screen_height @ {:08x}, video_mode @ {:08x}",
               ADDR_G_SCREEN_WIDTH, ADDR_G_SCREEN_HEIGHT, ADDR_G_VIDEO_MODE);
    logger.log("game => mouse_x @ {:08x}, mouse_y @ {:08x}, ui_update_selection @ {:08x}",
               ADDR_G_MOUSE_X, ADDR_G_MOUSE_Y, ADDR_UI_UPDATE_SELECTION);

    bool all_ok = true;
    
    // Hook game's video_set_resolution function
    {
        void* target = reinterpret_cast<void*>(ADDR_VIDEO_SET_RESOLUTION);
        MH_STATUS status = MH_CreateHook(target, reinterpret_cast<void*>(detour_video_set_resolution),
                                         reinterpret_cast<void**>(&orig_video_set_resolution));
        if (status != MH_OK) {
            logger.log("minhook => create_hook failed: video_set_resolution ({})", MH_StatusToString(status));
            all_ok = false;
        } else {
            logger.log("minhook => created hook: video_set_resolution @ {:08x}", ADDR_VIDEO_SET_RESOLUTION);
        }
    }
    
    // Hook game's game_update_mouse function for proper aspect ratio mouse scaling
    {
        void* target = reinterpret_cast<void*>(ADDR_GAME_UPDATE_MOUSE);
        MH_STATUS status = MH_CreateHook(target, reinterpret_cast<void*>(detour_game_update_mouse),
                                         reinterpret_cast<void**>(&orig_game_update_mouse));
        if (status != MH_OK) {
            logger.log("minhook => create_hook failed: game_update_mouse ({})", MH_StatusToString(status));
            all_ok = false;
        } else {
            logger.log("minhook => created hook: game_update_mouse @ {:08x}", ADDR_GAME_UPDATE_MOUSE);
        }
    }
    
    all_ok &= create_hook_checked(L"opengl32.dll", "wglSwapBuffers", (void*)detour_wgl_swap, &orig_wgl_swap);
    all_ok &= create_hook_checked(L"opengl32.dll", "glDrawElements", (void*)detour_gl_draw_elems, &orig_gl_draw_elems);
    all_ok &= create_hook_checked(L"opengl32.dll", "glViewport", (void*)detour_gl_viewport, &orig_gl_viewport);
    all_ok &= create_hook_checked(L"opengl32.dll", "glClear", (void*)detour_gl_clear, &orig_gl_clear);
    all_ok &= create_hook_checked(L"opengl32.dll", "glMatrixMode", (void*)detour_gl_matrix_mode, &orig_gl_matrix_mode);
    all_ok &= create_hook_checked(L"kernel32.dll", "QueryPerformanceCounter", (void*)detour_qpc, &orig_qpc);
    all_ok &= create_hook_checked(L"kernel32.dll", "CreateFileA", (void*)detour_create_file_a, &orig_create_file_a);
    all_ok &= create_hook_checked(L"user32.dll", "SetCursorPos", (void*)detour_set_cursor, &orig_set_cursor);

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

PROXY_EXPORT void uninstall_hooks() {
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

            if (ImGui::BeginTabItem("Resolution")) {
                ImGui::SeparatorText("Custom Resolution");
                
                bool custom_enabled = ctx.resolution.custom_enabled.load();
                if (ImGui::Checkbox("Enable Custom Resolution", &custom_enabled)) {
                    ctx.resolution.custom_enabled.store(custom_enabled);
                    if (custom_enabled) {
                        apply_custom_resolution();
                    }
                }
                
                ImGui::TextDisabled("Override game's built-in resolution options");
                
                int width  = ctx.resolution.custom_width.load();
                int height = ctx.resolution.custom_height.load();
                
                ImGui::SetNextItemWidth(120);
                if (ImGui::InputInt("Width", &width, 1, 100)) {
                    if (width < 320) width = 320;
                    if (width > 7680) width = 7680;
                    ctx.resolution.custom_width.store(width);
                }
                ImGui::SameLine();
                ImGui::SetNextItemWidth(120);
                if (ImGui::InputInt("Height", &height, 1, 100)) {
                    if (height < 240) height = 240;
                    if (height > 4320) height = 4320;
                    ctx.resolution.custom_height.store(height);
                }
                
                ImGui::Spacing();
                ImGui::Text("Presets:");
                
                auto preset_button = [&](const char* label, int w, int h) {
                    if (ImGui::Button(label)) {
                        ctx.resolution.custom_width.store(w);
                        ctx.resolution.custom_height.store(h);
                        if (ctx.resolution.custom_enabled.load()) {
                            apply_custom_resolution();
                        }
                    }
                };
                
                preset_button("640x480", 640, 480);
                ImGui::SameLine();
                preset_button("800x600", 800, 600);
                ImGui::SameLine();
                preset_button("1024x768", 1024, 768);
                ImGui::SameLine();
                preset_button("1280x720", 1280, 720);
                
                preset_button("1280x1024", 1280, 1024);
                ImGui::SameLine();
                preset_button("1366x768", 1366, 768);
                ImGui::SameLine();
                preset_button("1600x900", 1600, 900);
                ImGui::SameLine();
                preset_button("1920x1080", 1920, 1080);
                
                preset_button("2560x1440", 2560, 1440);
                ImGui::SameLine();
                preset_button("3840x2160", 3840, 2160);
                
                ImGui::Spacing();
                
                if (ImGui::Button("Apply Now")) {
                    ctx.resolution.custom_enabled.store(true);
                    apply_custom_resolution();
                }
                ImGui::SameLine();
                ImGui::TextDisabled("(Apply changes immediately)");
                
                ImGui::Spacing();
                
                if (ImGui::Button("Reinitialize UI")) {
                    if (ui_load_resources_fn) {
                        logger.log("resolution => manual ui_load_resources call");
                        ui_load_resources_fn();
                    }
                }
                ImGui::SameLine();
                ImGui::TextDisabled("Calls game's ui_load_resources()");
                
                ImGui::SeparatorText("Current State");
                
                if (g_screen_width_ptr && g_screen_height_ptr) {
                    ImGui::Text("Game Resolution: %dx%d", *g_screen_width_ptr, *g_screen_height_ptr);
                } else {
                    ImGui::TextColored(ImVec4(1,0,0,1), "Game pointers not available");
                }
                
                if (g_video_mode_ptr) {
                    ImGui::Text("Video Mode Index: %d", *g_video_mode_ptr);
                }
                
                if (g_screen_width_ptr && g_screen_height_ptr) {
                    float scale_x = UI_BASE_WIDTH / static_cast<float>(*g_screen_width_ptr);
                    float scale_y = UI_BASE_HEIGHT / static_cast<float>(*g_screen_height_ptr);
                    ImGui::Text("Mouse Scale X: %.6f, Y: %.6f", scale_x, scale_y);
                }
                
                ImGui::Text("Raw Mouse: %d, %d", ctx.last_raw_x.load(), ctx.last_raw_y.load());
                
                if (g_mouse_x_ptr && g_mouse_y_ptr) {
                    ImGui::Text("Scaled Mouse (UI): %d, %d", *g_mouse_x_ptr, *g_mouse_y_ptr);
                }
                
                bool applied = ctx.resolution.applied.load();
                ImGui::Text("Custom Applied: %s", applied ? "Yes" : "No");
                
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
                
                bool log_mouse = ctx.debug.log_mouse.load();
                if (ImGui::Checkbox("Log Mouse Coordinates", &log_mouse)) {
                    ctx.debug.log_mouse.store(log_mouse);
                }
                
                ImGui::SeparatorText("Hot Reload");
                
                bool hr_enabled = ctx.hot_reload.enabled.load();
                if (ImGui::Checkbox("Enable Hot Reload", &hr_enabled)) {
                    ctx.hot_reload.enabled.store(hr_enabled);
                }
                ImGui::TextDisabled("Place 'bass_hooks.dll' in game folder");
                ImGui::TextDisabled("Must export: install_hooks(), uninstall_hooks()");
                
                if (ctx.hot_reload.hooks_module) {
                    ImGui::TextColored(ImVec4(0,1,0,1), "bass_hooks.dll: LOADED");
                } else {
                    ImGui::Text("bass_hooks.dll: not loaded");
                }
                
                if (ImGui::Button("Force Reload Now")) {
                    try_hot_reload();
                }
                ImGui::SameLine();
                if (ImGui::Button("Unload Hooks")) {
                    if (ctx.hot_reload.hooks_module) {
                        auto old_uninstall = reinterpret_cast<hooks_func_t>(
                            GetProcAddress(ctx.hot_reload.hooks_module, "uninstall_hooks"));
                        if (old_uninstall) old_uninstall();
                        FreeLibrary(ctx.hot_reload.hooks_module);
                        ctx.hot_reload.hooks_module = nullptr;
                        logger.log("hot_reload => manually unloaded");
                    }
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
