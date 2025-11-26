
/*
 * bass_proxy.cpp
 * C++23 | spdlog 1.16 | MinHook | Feature Complete
 */

#define WIN32_LEAN_AND_MEAN
#define CRT_SECURE_NO_WARNINGS
#define SPDLOG_ACTIVE_LEVEL SPDLOG_LEVEL_TRACE

#include "bass_proxy.hpp"

#include <GL/gl.h>
#include <psapi.h>
#include <windows.h>

#include <MinHook.h>
#include <imgui.h>
#include <imgui_impl_opengl3.h>
#include <imgui_impl_win32.h>
#include <spdlog/sinks/base_sink.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/spdlog.h>

#include <atomic>
#include <cmath>
#include <expected>
#include <format>
#include <mutex>
#include <source_location>
#include <string>
#include <vector>

// -----------------------------------------------------------------------------
// Types & Constants
// -----------------------------------------------------------------------------

using wgl_swap_t      = BOOL(WINAPI*)(HDC);
using qpc_t           = BOOL(WINAPI*)(LARGE_INTEGER*);
using set_cursor_t    = BOOL(WINAPI*)(int, int);
using create_file_a_t = HANDLE(WINAPI*)(
    LPCSTR, DWORD, DWORD, LPSECURITY_ATTRIBUTES, DWORD, DWORD, HANDLE);
using gl_draw_elems_t = void(APIENTRY*)(GLenum, GLsizei, GLenum, const GLvoid*);
using gl_viewport_t   = void(APIENTRY*)(GLint, GLint, GLsizei, GLsizei);
using gl_clear_t      = void(APIENTRY*)(GLbitfield);
using video_set_resolution_t = void(__cdecl*)(int);
using game_update_mouse_t    = void(__cdecl*)(int, int);
using ui_func_t              = void(__cdecl*)(void);

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND,
                                                             UINT,
                                                             WPARAM,
                                                             LPARAM);

namespace game_addr
{
constexpr uintptr_t video_set_res = 0x00401000;
constexpr uintptr_t screen_width  = 0x00441874;
constexpr uintptr_t screen_height = 0x00441878;
constexpr uintptr_t video_mode    = 0x00441870;
constexpr uintptr_t update_mouse  = 0x0040ad30;
constexpr uintptr_t mouse_x       = 0x004e53d0;
constexpr uintptr_t mouse_y       = 0x004e53d4;
constexpr uintptr_t update_sel    = 0x00428b20;
constexpr uintptr_t mouse_active  = 0x004e53f4;
constexpr uintptr_t load_res      = 0x004288d0;
} // namespace game_addr

constexpr float ui_base_w = 800.0F;
constexpr float ui_base_h = 600.0F;

// -----------------------------------------------------------------------------
// Logger (Enhanced)
// -----------------------------------------------------------------------------

template <typename mutex>
class imgui_sink final : public spdlog::sinks::base_sink<mutex>
{
    ImGuiTextBuffer buf;
    ImGuiTextFilter filter;
    bool            scroll_to_bottom = true;

protected:
    void sink_it_(const spdlog::details::log_msg& msg) override
    {
        spdlog::memory_buf_t formatted;
        spdlog::sinks::base_sink<mutex>::formatter_->format(msg, formatted);
        buf.append(std::string_view(formatted.data(), formatted.size()).data());
        scroll_to_bottom = true;
    }

    void flush_() override {}

public:
    void draw()
    {
        std::lock_guard lock(spdlog::sinks::base_sink<mutex>::mutex_);

        if (ImGui::Button("Clear"))
        {
            buf.clear();
        }
        ImGui::SameLine();
        if (ImGui::Button("Copy"))
        {
            ImGui::SetClipboardText(buf.c_str());
        }
        ImGui::SameLine();
        filter.Draw("Filter", -100.0F);

        ImGui::Separator();

        if (ImGui::BeginChild("LogScroll",
                              ImVec2(0, 0),
                              0,
                              ImGuiWindowFlags_HorizontalScrollbar))
        {
            if (filter.IsActive())
            {
                const char* line = buf.begin();
                while (line != nullptr)
                {
                    const char* line_end = strchr(line, '\n');
                    if (filter.PassFilter(line, line_end))
                    {
                        ImGui::TextUnformatted(line, line_end);
                    }
                    line = ((line_end != nullptr) && (line_end[1] != 0))
                               ? line_end + 1
                               : nullptr;
                }
            }
            else
            {
                ImGui::TextUnformatted(buf.begin());
            }

            if (scroll_to_bottom &&
                ImGui::GetScrollY() >= ImGui::GetScrollMaxY())
            {
                ImGui::SetScrollHereY(1.0F);
                scroll_to_bottom = false;
            }
        }
        ImGui::EndChild();
    }
};

using imgui_sink_mt = imgui_sink<std::mutex>;

// -----------------------------------------------------------------------------
// Global Context
// -----------------------------------------------------------------------------

struct global_context final
{
    HWND                           hwnd{ nullptr };
    WNDPROC                        orig_wnd_proc{ nullptr };
    std::atomic_bool               imgui_ready{ false };
    std::atomic_bool               shutting_down{ false };
    std::atomic_bool               overlay_visible{ true };
    std::shared_ptr<imgui_sink_mt> log_sink;

    struct
    {
        std::atomic_bool disable_depth{ false };
        std::atomic_bool wireframe{ false };
        std::atomic_bool fog_override{ false };
        std::atomic_bool enable_clear{ false };
        ImVec4           clear_col{ 0.F, 0.F, 0.F, 0.F };
        ImVec4           fog_col{ 0.5F, 0.6F, 0.7F, 1.0F };
    } visuals;

    struct
    {
        std::atomic_bool enabled{ false };
        std::atomic_bool applied{ false };
        std::atomic_int  w{ 1920 };
        std::atomic_int  h{ 1080 };
    } res;

    struct
    {
        std::atomic<float> speed{ 1.0F };
        std::atomic_bool   block_input{ false };
    } gameplay;

    struct
    {
        std::atomic_bool log_fs{ false };
        std::atomic_bool log_gl{ false };
        std::atomic_bool log_mouse{ false };
    } debug;

    std::atomic_int frame_cnt{ 0 };
    std::atomic_int draw_cnt{ 0 };

    GLint           viewport[4]{ 0, 0, 800, 600 };
    std::atomic_int raw_mouse_x{ 0 };
    std::atomic_int raw_mouse_y{ 0 };
};

static global_context ctx;

// -----------------------------------------------------------------------------
// Pointers & Hooks
// -----------------------------------------------------------------------------

static wgl_swap_t             o_wgl_swap      = nullptr;
static qpc_t                  o_qpc           = nullptr;
static set_cursor_t           o_set_cursor    = nullptr;
static create_file_a_t        o_create_file_a = nullptr;
static gl_draw_elems_t        o_gl_draw_elems = nullptr;
static gl_viewport_t          o_gl_viewport   = nullptr;
static gl_clear_t             o_gl_clear      = nullptr;
static video_set_resolution_t o_set_res       = nullptr;
static game_update_mouse_t    o_update_mouse  = nullptr;

static int*      p_scr_w       = nullptr;
static int*      p_scr_h       = nullptr;
static int*      p_vid_mode    = nullptr;
static int*      p_mse_x       = nullptr;
static int*      p_mse_y       = nullptr;
static char*     p_mse_active  = nullptr;
static ui_func_t fn_update_sel = nullptr;
static ui_func_t fn_load_res   = nullptr;

static void draw_ui();

static void __cdecl h_set_res(int mode)
{
    if (ctx.res.enabled.load(std::memory_order_relaxed))
    {
        if (p_scr_w != nullptr)
        {
            *p_scr_w = ctx.res.w.load();
            *p_scr_h = ctx.res.h.load();
            ctx.res.applied.store(true);
            spdlog::info("res => override {}x{}", *p_scr_w, *p_scr_h);
            return;
        }
    }
    o_set_res(mode);
}

static void __cdecl h_update_mouse(int raw_x, int raw_y)
{
    ctx.raw_mouse_x.store(raw_x, std::memory_order_relaxed);
    ctx.raw_mouse_y.store(raw_y, std::memory_order_relaxed);

    if ((p_mse_active != nullptr) && *p_mse_active == 0)
    {
        return;
    }
    if ((p_mse_x == nullptr) || (p_scr_w == nullptr))
    {
        if (o_update_mouse != nullptr)
        {
            o_update_mouse(raw_x, raw_y);
        }
        return;
    }

    const float sx = ui_base_w / static_cast<float>(*p_scr_w);
    const float sy = ui_base_h / static_cast<float>(*p_scr_h);

    *p_mse_x = static_cast<int>(static_cast<float>(raw_x) * sx);
    *p_mse_y = static_cast<int>(static_cast<float>(raw_y) * sy);

    if (ctx.debug.log_mouse.load(std::memory_order_relaxed))
    {
        static int throttle = 0;
        if (++throttle % 60 == 0)
        {
            spdlog::debug("mouse => raw({},{}) -> ui({},{})",
                          raw_x,
                          raw_y,
                          *p_mse_x,
                          *p_mse_y);
        }
    }
    if (fn_update_sel != nullptr)
    {
        fn_update_sel();
    }
}

static void APIENTRY h_gl_viewport(GLint x, GLint y, GLsizei w, GLsizei h)
{
    ctx.viewport[0] = x;
    ctx.viewport[1] = y;
    ctx.viewport[2] = w;
    ctx.viewport[3] = h;
    o_gl_viewport(x, y, w, h);
}

static BOOL WINAPI h_set_cursor(int x, int y)
{
    return ctx.gameplay.block_input.load(std::memory_order_relaxed)
               ? TRUE
               : o_set_cursor(x, y);
}

static HANDLE WINAPI h_create_file_a(LPCSTR                fn,
                                     DWORD                 acc,
                                     DWORD                 shr,
                                     LPSECURITY_ATTRIBUTES sec,
                                     DWORD                 disp,
                                     DWORD                 attr,
                                     HANDLE                tmp)
{
    if (ctx.debug.log_fs.load(std::memory_order_relaxed))
    {
        spdlog::trace("fs => open \"{}\"", (fn != nullptr) ? fn : "null");
    }
    return o_create_file_a(fn, acc, shr, sec, disp, attr, tmp);
}

static BOOL WINAPI h_qpc(LARGE_INTEGER* val)
{
    if ((o_qpc == nullptr) || (o_qpc(val) == 0))
    {
        return FALSE;
    }

    static LARGE_INTEGER last_real{};
    static LARGE_INTEGER last_fake{};
    static bool          first = true;
    static std::mutex    mtx;

    float mul = ctx.gameplay.speed.load(std::memory_order_relaxed);
    if (std::abs(mul - 1.0F) < 0.001F && !first)
    {
        return TRUE;
    }

    std::scoped_lock lock(mtx);
    if (first)
    {
        last_real = last_fake = *val;
        first                 = false;
        return TRUE;
    }

    LONGLONG diff = val->QuadPart - last_real.QuadPart;
    last_real     = *val;
    last_fake.QuadPart += static_cast<LONGLONG>(static_cast<double>(diff) *
                                                static_cast<double>(mul));
    *val = last_fake;
    return TRUE;
}

static BOOL WINAPI h_wgl_swap(HDC dc)
{
    ctx.frame_cnt.fetch_add(1, std::memory_order_relaxed);

    if (!ctx.imgui_ready.load(std::memory_order_acquire))
    {
        if (HWND win = WindowFromDC(dc); win)
        {
            ctx.hwnd          = win;
            ctx.orig_wnd_proc = reinterpret_cast<WNDPROC>(SetWindowLongPtrA(
                win,
                GWLP_WNDPROC,
                reinterpret_cast<LONG_PTR>(
                    +[](HWND h, UINT m, WPARAM w, LPARAM l) -> LRESULT
                    {
                        if (m == WM_KEYDOWN && w == VK_INSERT)
                        {
                            bool v = ctx.overlay_visible.load();
                            ctx.overlay_visible.store(!v);
                            return 0;
                        }
                        if (!ctx.shutting_down.load() &&
                            ctx.overlay_visible.load() &&
                            ImGui_ImplWin32_WndProcHandler(h, m, w, l))
                        {
                            return true;
                        }
                        return CallWindowProc(ctx.orig_wnd_proc, h, m, w, l);
                    })));

            ImGui::CreateContext();
            ImGui::GetIO().IniFilename = "bass_proxy.ini";
            ImGui::StyleColorsDark();
            ImGui_ImplWin32_Init(win);
            ImGui_ImplOpenGL3_Init("#version 110");
            ctx.imgui_ready.store(true, std::memory_order_release);
        }
    }

    if (ctx.imgui_ready.load(std::memory_order_acquire) &&
        ctx.overlay_visible.load())
    {
        draw_ui();
        ctx.gameplay.block_input.store(ImGui::GetIO().WantCaptureMouse,
                                       std::memory_order_relaxed);
    }

    ctx.draw_cnt.store(0, std::memory_order_relaxed);
    return o_wgl_swap(dc);
}

static void APIENTRY h_gl_draw_elems(GLenum        m,
                                     GLsizei       c,
                                     GLenum        t,
                                     const GLvoid* i)
{
    ctx.draw_cnt.fetch_add(1, std::memory_order_relaxed);
    if (ctx.debug.log_gl.load(std::memory_order_relaxed))
    {
        spdlog::trace("gl => draw {} count={}", m, c);
    }

    bool no_depth = ctx.visuals.disable_depth.load(std::memory_order_relaxed);
    if (no_depth)
    {
        glDisable(GL_DEPTH_TEST);
        glDepthMask(GL_FALSE);
    }
    if (ctx.visuals.wireframe.load(std::memory_order_relaxed))
    {
        glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
    }

    o_gl_draw_elems(m, c, t, i);

    if (no_depth)
    {
        glEnable(GL_DEPTH_TEST);
        glDepthMask(GL_TRUE);
    }
    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
}

static void APIENTRY h_gl_clear(GLbitfield mask)
{
    if (ctx.visuals.enable_clear.load(std::memory_order_relaxed))
    {
        glClearColor(ctx.visuals.clear_col.x,
                     ctx.visuals.clear_col.y,
                     ctx.visuals.clear_col.z,
                     ctx.visuals.clear_col.w);
    }

    if (ctx.visuals.fog_override.load(std::memory_order_relaxed))
    {
        GLfloat c[] = { ctx.visuals.fog_col.x,
                        ctx.visuals.fog_col.y,
                        ctx.visuals.fog_col.z,
                        ctx.visuals.fog_col.w };
        glFogfv(GL_FOG_COLOR, c);
        glEnable(GL_FOG);
    }
    o_gl_clear(mask);
}

// -----------------------------------------------------------------------------
// Initialization
// -----------------------------------------------------------------------------

using hook_result = std::expected<void, std::string>;

template <typename t>
hook_result create_hook(
    LPCWSTR              mod,
    LPCSTR               proc,
    void*                det,
    t**                  orig,
    std::source_location l = std::source_location::current())
{
    if (MH_CreateHookApi(mod, proc, det, reinterpret_cast<LPVOID*>(orig)) !=
        MH_OK)
    {
        return std::unexpected(std::format(
            "{} hook failed {}:{}", proc, l.function_name(), l.line()));
    }
    return {};
}

template <typename t>
hook_result create_hook_addr(uintptr_t addr, void* det, t** orig)
{
    if (MH_CreateHook(reinterpret_cast<void*>(addr),
                      det,
                      reinterpret_cast<void**>(orig)) != MH_OK)
    {
        return std::unexpected(std::format("addr {:x} hook failed", addr));
    }
    return {};
}

void apply_resolution()
{
    if (p_scr_w == nullptr)
    {
        return;
    }
    int w    = ctx.res.w.load();
    int h    = ctx.res.h.load();
    *p_scr_w = w;
    *p_scr_h = h;
    if (ctx.hwnd != nullptr)
    {
        RECT r{ 0, 0, w, h };
        AdjustWindowRect(
            &r, static_cast<DWORD>(GetWindowLongA(ctx.hwnd, GWL_STYLE)), FALSE);
        SetWindowPos(ctx.hwnd,
                     nullptr,
                     0,
                     0,
                     r.right - r.left,
                     r.bottom - r.top,
                     SWP_NOMOVE | SWP_NOZORDER);
    }
    if (fn_load_res != nullptr)
    {
        fn_load_res();
    }
    ctx.res.applied.store(true);
    spdlog::info("res => applied {}x{}", w, h);
}

void install_hooks()
{
    try
    {
        ctx.log_sink = std::make_shared<imgui_sink_mt>();
        auto logger  = std::make_shared<spdlog::logger>(
            "main",
            spdlog::sinks_init_list{
                std::make_shared<spdlog::sinks::basic_file_sink_mt>(
                    "bass_proxy.log", true),
                ctx.log_sink });
        logger->set_pattern("[%H:%M:%S] %v");
        logger->set_level(spdlog::level::trace);
        spdlog::set_default_logger(logger);
    }
    catch (...)
    {
    }

    MH_Initialize();
    p_scr_w       = (int*)game_addr::screen_width;
    p_scr_h       = (int*)game_addr::screen_height;
    p_vid_mode    = (int*)game_addr::video_mode;
    p_mse_x       = (int*)game_addr::mouse_x;
    p_mse_y       = (int*)game_addr::mouse_y;
    p_mse_active  = (char*)game_addr::mouse_active;
    fn_update_sel = (ui_func_t)game_addr::update_sel;
    fn_load_res   = (ui_func_t)game_addr::load_res;

    std::vector<hook_result> results;
    results.push_back(create_hook_addr(
        game_addr::video_set_res, (void*)h_set_res, &o_set_res));
    results.push_back(create_hook_addr(
        game_addr::update_mouse, (void*)h_update_mouse, &o_update_mouse));
    results.push_back(create_hook(
        L"opengl32.dll", "wglSwapBuffers", (void*)h_wgl_swap, &o_wgl_swap));
    results.push_back(create_hook(L"opengl32.dll",
                                  "glDrawElements",
                                  (void*)h_gl_draw_elems,
                                  &o_gl_draw_elems));
    results.push_back(create_hook(
        L"opengl32.dll", "glViewport", (void*)h_gl_viewport, &o_gl_viewport));
    results.push_back(create_hook(
        L"opengl32.dll", "glClear", (void*)h_gl_clear, &o_gl_clear));
    results.push_back(create_hook(
        L"kernel32.dll", "QueryPerformanceCounter", (void*)h_qpc, &o_qpc));
    results.push_back(create_hook(L"kernel32.dll",
                                  "CreateFileA",
                                  (void*)h_create_file_a,
                                  &o_create_file_a));
    results.push_back(create_hook(
        L"user32.dll", "SetCursorPos", (void*)h_set_cursor, &o_set_cursor));

    for (auto& r : results)
    {
        if (!r)
        {
            spdlog::error("hook => {}", r.error());
        }
    }
    if (MH_EnableHook(MH_ALL_HOOKS) == MH_OK)
    {
        spdlog::info("core => hooks enabled");
    }
}

void uninstall_hooks()
{
    ctx.shutting_down.store(true);
    if ((ctx.hwnd != nullptr) && (ctx.orig_wnd_proc != nullptr))
    {
        SetWindowLongPtrA(ctx.hwnd, GWLP_WNDPROC, (LONG_PTR)ctx.orig_wnd_proc);
    }
    MH_DisableHook(MH_ALL_HOOKS);
    MH_Uninitialize();
    if (ctx.imgui_ready.exchange(false))
    {
        ImGui_ImplOpenGL3_Shutdown();
        ImGui_ImplWin32_Shutdown();
        ImGui::DestroyContext();
    }
    spdlog::shutdown();
}

static void draw_ui()
{
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();
    if (ImGui::Begin(
            "Airstrike 3D II [Proxy]", nullptr, ImGuiWindowFlags_MenuBar))
    {
        if (ImGui::BeginMenuBar())
        {
            if (ImGui::BeginMenu("Menu"))
            {
                if (ImGui::MenuItem("Unload"))
                {
                    std::thread([] { uninstall_hooks(); }).detach();
                }
                ImGui::EndMenu();
            }
            ImGui::EndMenuBar();
        }
        if (ImGui::BeginTabBar("Tabs"))
        {
            if (ImGui::BeginTabItem("Gfx"))
            {
                bool d = ctx.visuals.disable_depth.load();
                bool w = ctx.visuals.wireframe.load();
                bool f = ctx.visuals.fog_override.load();
                if (ImGui::Checkbox("Wallhack", &d))
                {
                    ctx.visuals.disable_depth.store(d);
                }
                if (ImGui::Checkbox("Wireframe", &w))
                {
                    ctx.visuals.wireframe.store(w);
                }
                if (ImGui::Checkbox("Fog", &f))
                {
                    ctx.visuals.fog_override.store(f);
                }
                if (f)
                {
                    ImGui::ColorEdit4("Fog Col", &ctx.visuals.fog_col.x);
                }
                bool lgl = ctx.debug.log_gl.load();
                if (ImGui::Checkbox("Log GL", &lgl))
                {
                    ctx.debug.log_gl.store(lgl);
                }
                ImGui::EndTabItem();
            }
            if (ImGui::BeginTabItem("Res"))
            {
                bool e = ctx.res.enabled.load();
                if (ImGui::Checkbox("Enable Custom", &e))
                {
                    ctx.res.enabled.store(e);
                }
                int w = ctx.res.w.load();
                int h = ctx.res.h.load();
                if (ImGui::InputInt("W", &w))
                {
                    ctx.res.w.store(std::clamp(w, 320, 7680));
                }
                if (ImGui::InputInt("H", &h))
                {
                    ctx.res.h.store(std::clamp(h, 240, 4320));
                }
                if (ImGui::Button("Apply"))
                {
                    apply_resolution();
                }
                ImGui::Text("Game: %dx%d | Mouse: %d,%d",
                            (p_scr_w != nullptr) ? *p_scr_w : 0,
                            (p_scr_h != nullptr) ? *p_scr_h : 0,
                            ctx.raw_mouse_x.load(),
                            ctx.raw_mouse_y.load());
                ImGui::EndTabItem();
            }
            if (ImGui::BeginTabItem("Game"))
            {
                float s = ctx.gameplay.speed.load();
                if (ImGui::SliderFloat("Speed", &s, 0.1F, 10.0F))
                {
                    ctx.gameplay.speed.store(s);
                }
                if (ImGui::Button("0.5x"))
                {
                    ctx.gameplay.speed.store(0.5F);
                }
                ImGui::SameLine();
                if (ImGui::Button("1.0x"))
                {
                    ctx.gameplay.speed.store(1.0F);
                }
                ImGui::SameLine();
                if (ImGui::Button("2.0x"))
                {
                    ctx.gameplay.speed.store(2.0F);
                }
                bool lfs = ctx.debug.log_fs.load();
                if (ImGui::Checkbox("Log FileSys", &lfs))
                {
                    ctx.debug.log_fs.store(lfs);
                }
                ImGui::EndTabItem();
            }
            if (ImGui::BeginTabItem("Stats"))
            {
                ImGui::Text("FPS: %.1f (%.3f ms)",
                            ImGui::GetIO().Framerate,
                            1000.0F / ImGui::GetIO().Framerate);
                ImGui::Text("Draws: %d | Frames: %d",
                            ctx.draw_cnt.load(),
                            ctx.frame_cnt.load());
                ImGui::Text(
                    "Viewport: %dx%d", ctx.viewport[2], ctx.viewport[3]);
                ImGui::Text("GPU: %s", glGetString(GL_RENDERER));
                ImGui::EndTabItem();
            }
            if (ImGui::BeginTabItem("Logs"))
            {
                if (ctx.log_sink)
                {
                    ctx.log_sink->draw();
                }
                ImGui::EndTabItem();
            }
            ImGui::EndTabBar();
        }
    }
    ImGui::End();
    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
}