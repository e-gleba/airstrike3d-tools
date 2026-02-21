// plugins.cpp — Modding SDK core: API provider + Lua plugin manager
// This is the single translation unit. Hook install/uninstall is called from
// DllMain (bass_proxy entry/exit).

#define WIN32_LEAN_AND_MEAN
#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE

#include "bass_proxy.hpp"

#include <GL/gl.h>
#include <windows.h>

#include <algorithm>
#include <safetyhook.hpp>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <imgui.h>
#include <imgui_impl_opengl3.h>
#include <imgui_impl_win32.h>

#include <spdlog/spdlog.h>

#include <atomic>
#include <filesystem>
#include <functional>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include <sol/sol.hpp>

// ---------------------------------------------------------------------------
// Forward declarations
// ---------------------------------------------------------------------------
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND,
                                                             UINT,
                                                             WPARAM,
                                                             LPARAM);

// ---------------------------------------------------------------------------
// Type aliases for hooked functions
// ---------------------------------------------------------------------------
using wgl_swap_t         = BOOL(WINAPI*)(HDC);
using gl_load_identity_t = void(APIENTRY*)();
using gl_matrix_mode_t   = void(APIENTRY*)(GLenum);
using glu_look_at_t      = void(APIENTRY*)(GLdouble,
                                      GLdouble,
                                      GLdouble,
                                      GLdouble,
                                      GLdouble,
                                      GLdouble,
                                      GLdouble,
                                      GLdouble,
                                      GLdouble);

// ---------------------------------------------------------------------------
// Constants
// ---------------------------------------------------------------------------
static constexpr auto kUiToggleKey = VK_INSERT;
static constexpr auto kGlslVersion = "#version 110";
static constexpr auto kPluginDir   = "plugins";

// ---------------------------------------------------------------------------
// Hook registry
// ---------------------------------------------------------------------------
struct hook_registry final
{
    safetyhook::InlineHook wgl_swap;
    safetyhook::InlineHook gl_matrix_mode;
    safetyhook::InlineHook gl_load_identity;
    safetyhook::InlineHook glu_look_at;

    void reset() { *this = hook_registry{}; }
};

// ---------------------------------------------------------------------------
// SDK state — the single global context
// ---------------------------------------------------------------------------
struct sdk_context final
{
    // Window
    HWND              window{ nullptr };
    WNDPROC           original_wnd_proc{ nullptr };
    std::atomic<bool> imgui_initialized{ false };
    std::atomic<bool> should_unload{ false };
    std::atomic<bool> show_ui{ true };

    // OpenGL state tracked by hooks
    GLenum current_matrix_mode{ GL_MODELVIEW };

    // Hooks
    hook_registry hooks;

    // Lua
    std::unique_ptr<sol::state> lua;
    std::recursive_mutex        lua_mutex;

    // Plugin callbacks (populated by Lua scripts via sdk.on_*)
    struct plugin_callbacks
    {
        std::vector<sol::protected_function> on_frame; // called every swap
        std::vector<sol::protected_function>
            on_overlay; // called inside ImGui frame
        std::vector<sol::protected_function>
            on_gl_identity; // called on glLoadIdentity (modelview)
        std::vector<sol::protected_function>
            on_glu_lookat; // called on gluLookAt
        std::vector<sol::protected_function>
            on_key_down; // (vk_code) -> bool (consumed)
        std::vector<sol::protected_function>
            on_load; // called once after plugin load
        std::vector<sol::protected_function> on_unload; // called on shutdown
    } callbacks;
};

static sdk_context g_ctx;

// ---------------------------------------------------------------------------
// Utility: call original through trampoline
// ---------------------------------------------------------------------------
template <typename T> static auto call_orig(safetyhook::InlineHook& hook) -> T
{
    return reinterpret_cast<T>(hook.trampoline().address());
}

// ---------------------------------------------------------------------------
// Utility: safe Lua callback invocation
// ---------------------------------------------------------------------------
static void invoke_callbacks(std::vector<sol::protected_function>& cbs)
{
    for (auto& fn : cbs)
    {
        auto result = fn();
        if (!result.valid())
        {
            sol::error err = result;
            spdlog::error("[sdk] lua callback error: {}", err.what());
        }
    }
}

template <typename... Args>
static void invoke_callbacks(std::vector<sol::protected_function>& cbs,
                             Args&&... args)
{
    for (auto& fn : cbs)
    {
        auto result = fn(std::forward<Args>(args)...);
        if (!result.valid())
        {
            sol::error err = result;
            spdlog::error("[sdk] lua callback error: {}", err.what());
        }
    }
}

// ---------------------------------------------------------------------------
// Lua API: register all SDK bindings
// ---------------------------------------------------------------------------
static void register_lua_api(sol::state& lua)
{
    // ---- sdk table ----
    auto sdk = lua.create_named_table("sdk");

    // -- Callback registration --
    sdk.set_function("on_frame",
                     [](sol::protected_function fn)
                     {
                         std::lock_guard lk(g_ctx.lua_mutex);
                         g_ctx.callbacks.on_frame.push_back(std::move(fn));
                     });
    sdk.set_function("on_overlay",
                     [](sol::protected_function fn)
                     {
                         std::lock_guard lk(g_ctx.lua_mutex);
                         g_ctx.callbacks.on_overlay.push_back(std::move(fn));
                     });
    sdk.set_function("on_gl_identity",
                     [](sol::protected_function fn)
                     {
                         std::lock_guard lk(g_ctx.lua_mutex);
                         g_ctx.callbacks.on_gl_identity.push_back(
                             std::move(fn));
                     });
    sdk.set_function("on_glu_lookat",
                     [](sol::protected_function fn)
                     {
                         std::lock_guard lk(g_ctx.lua_mutex);
                         g_ctx.callbacks.on_glu_lookat.push_back(std::move(fn));
                     });
    sdk.set_function("on_key_down",
                     [](sol::protected_function fn)
                     {
                         std::lock_guard lk(g_ctx.lua_mutex);
                         g_ctx.callbacks.on_key_down.push_back(std::move(fn));
                     });
    sdk.set_function("on_load",
                     [](sol::protected_function fn)
                     {
                         std::lock_guard lk(g_ctx.lua_mutex);
                         g_ctx.callbacks.on_load.push_back(std::move(fn));
                     });
    sdk.set_function("on_unload",
                     [](sol::protected_function fn)
                     {
                         std::lock_guard lk(g_ctx.lua_mutex);
                         g_ctx.callbacks.on_unload.push_back(std::move(fn));
                     });

    // -- Input --
    sdk.set_function("is_key_down",
                     [](int vk) -> bool
                     { return (GetAsyncKeyState(vk) & 0x8000) != 0; });
    sdk.set_function("get_cursor_pos",
                     []() -> std::tuple<int, int>
                     {
                         POINT p;
                         GetCursorPos(&p);
                         return { p.x, p.y };
                     });
    sdk.set_function("set_cursor_pos",
                     [](int x, int y) { SetCursorPos(x, y); });
    sdk.set_function("show_cursor",
                     [](bool show) { ShowCursor(show ? TRUE : FALSE); });
    sdk.set_function("get_window_rect",
                     []() -> std::tuple<int, int, int, int>
                     {
                         RECT r{};
                         if (g_ctx.window)
                             GetWindowRect(g_ctx.window, &r);
                         return { r.left, r.top, r.right, r.bottom };
                     });

    // -- Window messages / cheat codes --
    sdk.set_function(
        "send_chars",
        [](const std::string& text)
        {
            if (!g_ctx.window)
                return;
            for (char c : text)
                PostMessageA(g_ctx.window, WM_CHAR, static_cast<WPARAM>(c), 0);
        });

    // -- OpenGL helpers (raw calls, use from gl_identity / glu_lookat hooks) --
    sdk.set_function("gl_mult_matrix_d",
                     [](sol::table t)
                     {
                         if (t.size() < 16)
                             return;
                         GLdouble m[16];
                         for (int i = 0; i < 16; ++i)
                             m[i] = t[i + 1].get<double>();
                         glMultMatrixd(m);
                     });

    sdk.set_function("gl_load_matrix_d",
                     [](sol::table t)
                     {
                         if (t.size() < 16)
                             return;
                         GLdouble m[16];
                         for (int i = 0; i < 16; ++i)
                             m[i] = t[i + 1].get<double>();
                         glLoadMatrixd(m);
                     });

    // Convenience: build a lookAt matrix and multiply it onto the current GL
    // matrix stack. All doubles.
    sdk.set_function("gl_apply_lookat",
                     [](double ex,
                        double ey,
                        double ez,
                        double cx,
                        double cy,
                        double cz,
                        double ux,
                        double uy,
                        double uz)
                     {
                         glm::dmat4 view = glm::lookAt(glm::dvec3(ex, ey, ez),
                                                       glm::dvec3(cx, cy, cz),
                                                       glm::dvec3(ux, uy, uz));
                         glMultMatrixd(glm::value_ptr(view));
                     });

    // -- GLM math exposed to Lua --
    auto math = lua.create_named_table("gmath");

    math.set_function("radians", [](double deg) { return glm::radians(deg); });
    math.set_function("cos", [](double v) { return std::cos(v); });
    math.set_function("sin", [](double v) { return std::sin(v); });
    math.set_function("clamp",
                      [](double v, double lo, double hi)
                      { return glm::clamp(v, lo, hi); });
    math.set_function("mod", [](double v, double m) { return glm::mod(v, m); });

    // normalize(x,y,z) -> x,y,z
    math.set_function(
        "normalize",
        [](double x, double y, double z) -> std::tuple<double, double, double>
        {
            auto n = glm::normalize(glm::dvec3(x, y, z));
            return { n.x, n.y, n.z };
        });

    // cross(ax,ay,az, bx,by,bz) -> x,y,z
    math.set_function(
        "cross",
        [](double ax, double ay, double az, double bx, double by, double bz)
            -> std::tuple<double, double, double>
        {
            auto c = glm::cross(glm::dvec3(ax, ay, az), glm::dvec3(bx, by, bz));
            return { c.x, c.y, c.z };
        });

    // lookat_matrix(ex,ey,ez, cx,cy,cz, ux,uy,uz) -> table[16]
    math.set_function("lookat_matrix",
                      [&lua](double ex,
                             double ey,
                             double ez,
                             double cx,
                             double cy,
                             double cz,
                             double ux,
                             double uy,
                             double uz) -> sol::table
                      {
                          glm::dmat4    m = glm::lookAt(glm::dvec3(ex, ey, ez),
                                                     glm::dvec3(cx, cy, cz),
                                                     glm::dvec3(ux, uy, uz));
                          const double* p = glm::value_ptr(m);
                          sol::table    t = lua.create_table(16, 0);
                          for (int i = 0; i < 16; ++i)
                              t[i + 1] = p[i];
                          return t;
                      });

    // -- ImGui thin wrappers (enough for basic UI) --
    auto ui = lua.create_named_table("ui");

    ui.set_function("begin_window",
                    [](const std::string& title) -> bool
                    { return ImGui::Begin(title.c_str()); });
    ui.set_function("end_window", []() { ImGui::End(); });
    ui.set_function("text",
                    [](const std::string& t) { ImGui::Text("%s", t.c_str()); });
    ui.set_function(
        "text_colored",
        [](float r, float g, float b, float a, const std::string& t)
        { ImGui::TextColored(ImVec4(r, g, b, a), "%s", t.c_str()); });
    ui.set_function("text_wrapped",
                    [](const std::string& t)
                    { ImGui::TextWrapped("%s", t.c_str()); });
    ui.set_function("text_disabled",
                    [](const std::string& t)
                    { ImGui::TextDisabled("%s", t.c_str()); });
    ui.set_function("button",
                    [](const std::string& label) -> bool
                    { return ImGui::Button(label.c_str(), { -1, 0 }); });
    ui.set_function("button_sized",
                    [](const std::string& label, float w, float h) -> bool
                    { return ImGui::Button(label.c_str(), { w, h }); });
    ui.set_function(
        "checkbox",
        [](const std::string& label, bool v) -> std::tuple<bool, bool>
        {
            bool changed = ImGui::Checkbox(label.c_str(), &v);
            return { v, changed };
        });
    ui.set_function(
        "drag_float",
        [](const std::string& label, float v, float speed, float mn, float mx)
            -> std::tuple<float, bool>
        {
            bool changed = ImGui::DragFloat(label.c_str(), &v, speed, mn, mx);
            return { v, changed };
        });
    ui.set_function("slider_float",
                    [](const std::string& label, float v, float mn, float mx)
                        -> std::tuple<float, bool>
                    {
                        bool changed =
                            ImGui::SliderFloat(label.c_str(), &v, mn, mx);
                        return { v, changed };
                    });
    ui.set_function("slider_int",
                    [](const std::string& label, int v, int mn, int mx)
                        -> std::tuple<int, bool>
                    {
                        bool changed =
                            ImGui::SliderInt(label.c_str(), &v, mn, mx);
                        return { v, changed };
                    });
    ui.set_function("separator", []() { ImGui::Separator(); });
    ui.set_function("same_line", []() { ImGui::SameLine(); });
    ui.set_function("spacing", []() { ImGui::Spacing(); });
    ui.set_function("collapsing_header",
                    [](const std::string& label, bool default_open) -> bool
                    {
                        int flags =
                            default_open ? ImGuiTreeNodeFlags_DefaultOpen : 0;
                        return ImGui::CollapsingHeader(label.c_str(), flags);
                    });
    ui.set_function(
        "set_next_window_pos",
        [](float x, float y)
        { ImGui::SetNextWindowPos({ x, y }, ImGuiCond_FirstUseEver); });
    ui.set_function(
        "set_next_window_size",
        [](float w, float h)
        { ImGui::SetNextWindowSize({ w, h }, ImGuiCond_FirstUseEver); });
    ui.set_function("get_delta_time",
                    []() -> float { return ImGui::GetIO().DeltaTime; });

    // -- Logging --
    sdk.set_function("log_info",
                     [](const std::string& msg)
                     { spdlog::info("[lua] {}", msg); });
    sdk.set_function("log_warn",
                     [](const std::string& msg)
                     { spdlog::warn("[lua] {}", msg); });
    sdk.set_function("log_error",
                     [](const std::string& msg)
                     { spdlog::error("[lua] {}", msg); });

    // -- Virtual key constants --
    auto vk       = lua.create_named_table("VK");
    vk["SHIFT"]   = VK_SHIFT;
    vk["CONTROL"] = VK_CONTROL;
    vk["SPACE"]   = VK_SPACE;
    vk["INSERT"]  = VK_INSERT;
    vk["LBUTTON"] = VK_LBUTTON;
    vk["RBUTTON"] = VK_RBUTTON;
    vk["ESCAPE"]  = VK_ESCAPE;
    vk["TAB"]     = VK_TAB;
    vk["RETURN"]  = VK_RETURN;
    // Letters A-Z
    for (char c = 'A'; c <= 'Z'; ++c)
    {
        char name[2] = { c, '\0' };
        vk[name]     = static_cast<int>(c);
    }
    // Digits 0-9
    for (char c = '0'; c <= '9'; ++c)
    {
        char name[2] = { c, '\0' };
        vk[name]     = static_cast<int>(c);
    }
    // F-keys
    for (int i = 1; i <= 12; ++i)
    {
        std::string name = "F" + std::to_string(i);
        vk[name]         = VK_F1 + (i - 1);
    }

    // -- GL constants --
    auto gl          = lua.create_named_table("GL");
    gl["MODELVIEW"]  = static_cast<int>(GL_MODELVIEW);
    gl["PROJECTION"] = static_cast<int>(GL_PROJECTION);
}

// ---------------------------------------------------------------------------
// Plugin loader: scan plugins/ directory for .lua files
// ---------------------------------------------------------------------------
static void load_plugins()
{
    namespace fs = std::filesystem;

    g_ctx.lua = std::make_unique<sol::state>();
    g_ctx.lua->open_libraries(sol::lib::base,
                              sol::lib::math,
                              sol::lib::string,
                              sol::lib::table,
                              sol::lib::io,
                              sol::lib::os,
                              sol::lib::package);

    register_lua_api(*g_ctx.lua);

    const fs::path plugin_dir = fs::path(".") / kPluginDir;
    if (!fs::exists(plugin_dir))
    {
        spdlog::warn("[sdk] plugins directory '{}' not found, creating it",
                     plugin_dir.string());
        fs::create_directories(plugin_dir);
        return;
    }

    std::vector<fs::path> scripts;
    for (auto& entry : fs::directory_iterator(plugin_dir))
    {
        if (entry.is_regular_file() && entry.path().extension() == ".lua")
        {
            scripts.push_back(entry.path());
        }
    }

    std::ranges::sort(scripts);

    for (auto& path : scripts)
    {
        spdlog::info("[sdk] loading plugin: {}", path.filename().string());
        auto result = g_ctx.lua->safe_script_file(path.string(),
                                                  sol::script_pass_on_error);
        if (!result.valid())
        {
            sol::error err = result;
            spdlog::error("[sdk] failed to load {}: {}",
                          path.filename().string(),
                          err.what());
        }
    }

    // Fire on_load callbacks
    invoke_callbacks(g_ctx.callbacks.on_load);
    spdlog::info("[sdk] all plugins loaded ({} scripts)", scripts.size());
}

static void unload_plugins()
{
    std::lock_guard lk(g_ctx.lua_mutex);
    invoke_callbacks(g_ctx.callbacks.on_unload);

    // Clear all callbacks
    g_ctx.callbacks = {};
    g_ctx.lua.reset();
    spdlog::info("[sdk] plugins unloaded");
}

// ---------------------------------------------------------------------------
// Hook implementations
// ---------------------------------------------------------------------------
static void APIENTRY hk_gl_matrix_mode(GLenum mode)
{
    g_ctx.current_matrix_mode = mode;
    if (g_ctx.hooks.gl_matrix_mode)
        call_orig<gl_matrix_mode_t>(g_ctx.hooks.gl_matrix_mode)(mode);
}

static void APIENTRY hk_gl_load_identity()
{
    if (g_ctx.hooks.gl_load_identity)
        call_orig<gl_load_identity_t>(g_ctx.hooks.gl_load_identity)();

    if (g_ctx.current_matrix_mode == GL_MODELVIEW)
    {
        std::lock_guard lk(g_ctx.lua_mutex);
        invoke_callbacks(g_ctx.callbacks.on_gl_identity);
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
    bool consumed = false;
    {
        std::lock_guard lk(g_ctx.lua_mutex);
        if (!g_ctx.callbacks.on_glu_lookat.empty())
        {
            // Pass original params to Lua; if any callback exists, we let Lua
            // decide whether to call the original or apply its own transform.
            for (auto& fn : g_ctx.callbacks.on_glu_lookat)
            {
                auto result = fn(ex, ey, ez, cx, cy, cz, ux, uy, uz);
                if (result.valid())
                {
                    sol::optional<bool> ret = result;
                    if (ret.has_value() && ret.value())
                        consumed = true;
                }
            }
        }
    }

    if (!consumed && g_ctx.hooks.glu_look_at)
    {
        call_orig<glu_look_at_t>(g_ctx.hooks.glu_look_at)(
            ex, ey, ez, cx, cy, cz, ux, uy, uz);
    }
}

static void render_overlay()
{
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();

    {
        std::lock_guard lk(g_ctx.lua_mutex);
        invoke_callbacks(g_ctx.callbacks.on_overlay);
    }

    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
}

static LRESULT CALLBACK hk_wnd_proc(HWND h, UINT m, WPARAM w, LPARAM l);

static BOOL WINAPI hk_wgl_swap(HDC dc)
{
    // One-time ImGui init
    static std::once_flag init_once;
    if (wglGetCurrentContext() != nullptr)
    {
        std::call_once(
            init_once,
            [&](HDC hdc)
            {
                g_ctx.window = WindowFromDC(hdc);
                if (g_ctx.window)
                {
                    g_ctx.original_wnd_proc =
                        reinterpret_cast<WNDPROC>(SetWindowLongPtrA(
                            g_ctx.window,
                            GWLP_WNDPROC,
                            reinterpret_cast<LONG_PTR>(hk_wnd_proc)));

                    ImGui::CreateContext();
                    ImGuiIO& io = ImGui::GetIO();
                    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
                    io.IniFilename = nullptr;

                    ImGui::StyleColorsDark();
                    ImGui_ImplWin32_Init(g_ctx.window);
                    ImGui_ImplOpenGL3_Init(kGlslVersion);

                    g_ctx.imgui_initialized.store(true,
                                                  std::memory_order_release);
                    spdlog::info("[sdk] imgui initialized");
                }
            },
            dc);
    }

    if (g_ctx.imgui_initialized.load(std::memory_order_acquire))
    {
        // Per-frame Lua callbacks
        {
            std::lock_guard lk(g_ctx.lua_mutex);
            invoke_callbacks(g_ctx.callbacks.on_frame);
        }

        if (g_ctx.show_ui.load(std::memory_order_relaxed))
            render_overlay();
    }

    return call_orig<wgl_swap_t>(g_ctx.hooks.wgl_swap)(dc);
}

static LRESULT CALLBACK hk_wnd_proc(HWND h, UINT m, WPARAM w, LPARAM l)
{
    // UI toggle
    if (m == WM_KEYDOWN && w == kUiToggleKey)
    {
        g_ctx.show_ui.store(!g_ctx.show_ui.load());
        return 0;
    }

    // Forward key events to Lua
    if (m == WM_KEYDOWN)
    {
        std::lock_guard lk(g_ctx.lua_mutex);
        for (auto& fn : g_ctx.callbacks.on_key_down)
        {
            auto result = fn(static_cast<int>(w));
            if (result.valid())
            {
                sol::optional<bool> consumed = result;
                if (consumed.has_value() && consumed.value())
                    return 0;
            }
        }
    }

    // ImGui input
    if (!g_ctx.should_unload.load() && g_ctx.show_ui.load() &&
        ImGui_ImplWin32_WndProcHandler(h, m, w, l) != 0)
    {
        return 1;
    }

    return CallWindowProc(g_ctx.original_wnd_proc, h, m, w, l);
}

// ---------------------------------------------------------------------------
// Public API: called from DllMain (bass_proxy)
// ---------------------------------------------------------------------------
void install_hooks()
{
    spdlog::info("[sdk] installing hooks...");

    auto get_addr = [](const wchar_t* mod, const char* func) -> void*
    {
        return reinterpret_cast<void*>(
            GetProcAddress(GetModuleHandleW(mod), func));
    };

    g_ctx.hooks.wgl_swap =
        safetyhook::create_inline(get_addr(L"opengl32.dll", "wglSwapBuffers"),
                                  reinterpret_cast<void*>(hk_wgl_swap));

    g_ctx.hooks.gl_matrix_mode =
        safetyhook::create_inline(get_addr(L"opengl32.dll", "glMatrixMode"),
                                  reinterpret_cast<void*>(hk_gl_matrix_mode));

    g_ctx.hooks.gl_load_identity =
        safetyhook::create_inline(get_addr(L"opengl32.dll", "glLoadIdentity"),
                                  reinterpret_cast<void*>(hk_gl_load_identity));

    g_ctx.hooks.glu_look_at =
        safetyhook::create_inline(get_addr(L"glu32.dll", "gluLookAt"),
                                  reinterpret_cast<void*>(hk_glu_look_at));

    spdlog::info("[sdk] hooks installed, loading plugins...");
    load_plugins();
}

void uninstall_hooks()
{
    spdlog::info("[sdk] uninstalling...");
    g_ctx.should_unload.store(true);

    unload_plugins();

    if (g_ctx.imgui_initialized.load())
    {
        ImGui_ImplOpenGL3_Shutdown();
        ImGui_ImplWin32_Shutdown();
        ImGui::DestroyContext();
    }

    if (g_ctx.window && g_ctx.original_wnd_proc)
    {
        SetWindowLongPtrA(g_ctx.window,
                          GWLP_WNDPROC,
                          reinterpret_cast<LONG_PTR>(g_ctx.original_wnd_proc));
    }

    g_ctx.hooks.reset();
    spdlog::info("[sdk] shutdown complete");
}