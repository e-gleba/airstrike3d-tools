/// @file render/detail/render_hooks.cpp
/// @brief Render hook subsystem implementation.
///
/// This is the **only** file that owns safetyhook instances,
/// OpenGL/DirectX types, imgui overlay state, and wndproc hook.

#include "sdk/render/render_hooks.hpp"
#include "sdk/core/context.hpp"
#include "sdk/core/logging.hpp"
#include "sdk/util/win32.hpp"

#include <GL/gl.h>
#include <imgui.h>
#include <imgui_impl_opengl3.h>
#include <imgui_impl_win32.h>
#include <safetyhook.hpp>

#include <array>
#include <cctype>
#include <cstring>
#include <format>
#include <mutex>
#include <vector>
#include <windows.h>

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(
    HWND, UINT, WPARAM, LPARAM);

namespace sdk::render
{

namespace
{

// ── Helpers ──────────────────────────────────────────────────────────────

template <typename fn_ptr>
[[nodiscard]] auto call_orig(safetyhook::InlineHook& hook) -> fn_ptr
{
    return reinterpret_cast<fn_ptr>(hook.trampoline().address());
}

constexpr auto k_glsl_version   = "#version 110";
constexpr auto k_ui_toggle_key  = VK_INSERT;

// ── String helpers ──────────────────────────────────────────────────────

bool str_contains_i(const char* haystack, const char* needle)
{
    if ((haystack == nullptr) || (needle == nullptr))
        return false;

    for (const char* h = haystack; *h != '\0'; ++h)
    {
        const char* n   = needle;
        const char* cur = h;
        while (*n != '\0' && *cur != '\0' &&
               std::tolower(static_cast<unsigned char>(*cur)) ==
                   std::tolower(static_cast<unsigned char>(*n)))
        {
            ++cur;
            ++n;
        }
        if (*n == '\0')
            return true;
    }
    return false;
}

bool is_dx_dll_name(const char* name)
{
    if (name == nullptr)
        return false;

    static constexpr const char* k_patterns[] = {
        "d3d8", "d3d9", "ddraw", "dxgi", "d3d11", "d3d12",
    };

    for (const auto* p : k_patterns)
    {
        if (str_contains_i(name, p))
            return true;
    }
    return false;
}

} // anonymous namespace

// ── impl ────────────────────────────────────────────────────────────────

struct HookSystem::impl
{
    static inline impl* self = nullptr;

    // ── Detection state ─────────────────────────────────────────────────

    std::atomic<api>  detected{ api::unknown };
    std::atomic<bool> overlay_avail{ false };
    std::atomic<bool> imgui_init{ false };
    std::atomic<bool> show_ui{ true };
    std::atomic<bool> should_unload{ false };

    // ── GL state ────────────────────────────────────────────────────────

    GLenum current_matrix_mode{ GL_MODELVIEW };

    // ── Hooks ───────────────────────────────────────────────────────────

    safetyhook::InlineHook ll_a_hook;
    safetyhook::InlineHook ll_w_hook;
    safetyhook::InlineHook wgl_swap_hook;
    safetyhook::InlineHook gl_matrix_mode_hook;
    safetyhook::InlineHook gl_load_identity_hook;
    safetyhook::InlineHook glu_look_at_hook;

    // ── Wndproc ─────────────────────────────────────────────────────────

    HWND    window{};
    WNDPROC original_wnd_proc{};

    // ── Callbacks ───────────────────────────────────────────────────────

    std::recursive_mutex       cb_mutex;
    std::vector<void_fn>       frame_cbs;
    std::vector<void_fn>       overlay_cbs;
    std::vector<key_fn>        key_cbs;
    std::vector<identity_fn>   identity_cbs;
    std::vector<lookat_fn>     lookat_cbs;

    // ── Construction / destruction ──────────────────────────────────────

    impl() { self = this; }
    ~impl() { self = nullptr; }

    // ── Callback invocation helpers ─────────────────────────────────────

    void invoke_frame()
    {
        std::lock_guard lk{ cb_mutex };
        for (auto& fn : frame_cbs) fn();
    }

    void invoke_overlay()
    {
        std::lock_guard lk{ cb_mutex };
        for (auto& fn : overlay_cbs) fn();
    }

    [[nodiscard]] bool invoke_key_down(int vk)
    {
        std::lock_guard lk{ cb_mutex };
        for (auto& fn : key_cbs)
            if (fn(vk)) return true;
        return false;
    }

    void invoke_gl_identity(uint32_t mode)
    {
        std::lock_guard lk{ cb_mutex };
        for (auto& fn : identity_cbs) fn(mode);
    }

    [[nodiscard]] bool invoke_glu_lookat(double ex, double ey, double ez,
                                          double cx, double cy, double cz,
                                          double ux, double uy, double uz)
    {
        std::lock_guard lk{ cb_mutex };
        for (auto& fn : lookat_cbs)
            if (fn(ex, ey, ez, cx, cy, cz, ux, uy, uz)) return true;
        return false;
    }

    // ── Hook detours (static — access via self) ─────────────────────────

    static void APIENTRY hk_gl_matrix_mode(GLenum mode)
    {
        self->current_matrix_mode = mode;
        using fn_t = void(APIENTRY*)(GLenum);
        auto orig = call_orig<fn_t>(self->gl_matrix_mode_hook);
        if (orig) orig(mode);
    }

    static void APIENTRY hk_gl_load_identity()
    {
        using fn_t = void(APIENTRY*)();
        auto orig = call_orig<fn_t>(self->gl_load_identity_hook);
        if (orig) orig();

        if (self->current_matrix_mode == GL_MODELVIEW)
        {
            self->invoke_gl_identity(
                static_cast<uint32_t>(self->current_matrix_mode));
        }
    }

    static void APIENTRY hk_glu_look_at(
        GLdouble ex, GLdouble ey, GLdouble ez,
        GLdouble cx, GLdouble cy, GLdouble cz,
        GLdouble ux, GLdouble uy, GLdouble uz)
    {
        auto consumed = self->invoke_glu_lookat(
            ex, ey, ez, cx, cy, cz, ux, uy, uz);
        if (!consumed)
        {
            using fn_t = void(APIENTRY*)(GLdouble, GLdouble, GLdouble,
                                          GLdouble, GLdouble, GLdouble,
                                          GLdouble, GLdouble, GLdouble);
            auto orig = call_orig<fn_t>(self->glu_look_at_hook);
            if (orig) orig(ex, ey, ez, cx, cy, cz, ux, uy, uz);
        }
    }

    static BOOL WINAPI hk_wgl_swap(HDC dc)
    {
        // Lazy detection: first valid GL frame confirms OpenGL
        if (self->detected.load(std::memory_order::relaxed) == api::unknown)
        {
            if ((wglGetCurrentContext() != nullptr) &&
                (GetPixelFormat(dc) != 0))
            {
                self->on_gl_confirmed();
            }
        }

        // Init overlay on first valid GL frame
        if (self->overlay_avail.load(std::memory_order::acquire))
        {
            self->init_overlay(dc);

            if (self->imgui_init.load(std::memory_order::acquire))
            {
                self->invoke_frame();

                if (self->show_ui.load(std::memory_order::relaxed))
                {
                    self->render_overlay();
                }
            }
        }

        using wgl_swap_fn = BOOL(WINAPI*)(HDC);
        return call_orig<wgl_swap_fn>(self->wgl_swap_hook)(dc);
    }

    static HMODULE WINAPI hk_load_library_a(LPCSTR name)
    {
        using fn_t = decltype(&LoadLibraryA);
        auto orig = reinterpret_cast<fn_t>(
            self->ll_a_hook.trampoline().address());
        HMODULE result = orig(name);

        if ((result != nullptr) && is_dx_dll_name(name))
            self->on_dx_detected();

        return result;
    }

    static HMODULE WINAPI hk_load_library_w(LPCWSTR name)
    {
        using fn_t = decltype(&LoadLibraryW);
        auto orig = reinterpret_cast<fn_t>(
            self->ll_w_hook.trampoline().address());
        HMODULE result = orig(name);

        if ((result != nullptr) && (name != nullptr))
        {
            char buf[128]{};
            WideCharToMultiByte(CP_ACP, 0, name, -1, buf,
                                static_cast<int>(sizeof(buf)),
                                nullptr, nullptr);
            if (is_dx_dll_name(buf))
                self->on_dx_detected();
        }

        return result;
    }

    static LRESULT CALLBACK hk_wnd_proc(HWND h, UINT m, WPARAM w, LPARAM l)
    {
        if (m == WM_KEYDOWN) [[unlikely]]
        {
            if (w == k_ui_toggle_key) [[unlikely]]
            {
                self->show_ui = !self->show_ui;
                return 0;
            }
            if (self->invoke_key_down(static_cast<int>(w)))
                return 0;
        }

        if (!self->should_unload.load(std::memory_order::relaxed) &&
            self->show_ui.load(std::memory_order::relaxed) &&
            ImGui_ImplWin32_WndProcHandler(h, m, w, l))
        {
            return 1;
        }

        return CallWindowProc(self->original_wnd_proc, h, m, w, l);
    }

    // ── Detection callbacks ─────────────────────────────────────────────

    void on_dx_detected()
    {
        auto expected = api::unknown;
        if (!detected.compare_exchange_strong(expected, api::directx))
            return;

        overlay_avail.store(false, std::memory_order::release);

        sdk::log_warn("");
        sdk::log_warn("╔══════════════════════════════════════════════════════╗");
        sdk::log_warn("║  DirectX renderer detected                          ║");
        sdk::log_warn("║  ImGui overlay: DISABLED                            ║");
        sdk::log_warn("║  Lua plugins & input hooks: ACTIVE                  ║");
        sdk::log_warn("╚══════════════════════════════════════════════════════╝");
        sdk::log_warn("");
    }

    void on_gl_confirmed()
    {
        auto expected = api::unknown;
        if (!detected.compare_exchange_strong(expected, api::opengl))
            return;

        overlay_avail.store(true, std::memory_order::release);

        sdk::log_info("");
        sdk::log_info("╔══════════════════════════════════════════════════════╗");
        sdk::log_info("║  OpenGL renderer confirmed                          ║");
        sdk::log_info("║  Full overlay + cheats + plugins: ACTIVE            ║");
        sdk::log_info("╚══════════════════════════════════════════════════════╝");
        sdk::log_info("");
    }

    // ── Overlay management ──────────────────────────────────────────────

    void init_overlay(HDC dc)
    {
        static std::once_flag flag;

        if (wglGetCurrentContext() == nullptr)
            return;

        std::call_once(flag, [this, dc]()
        {
            window = WindowFromDC(dc);
            if (window == nullptr)
                return;

            // Store in shared context for Lua bindings
            g_ctx.window = window;

            original_wnd_proc = reinterpret_cast<WNDPROC>(SetWindowLongPtrA(
                window, GWLP_WNDPROC,
                reinterpret_cast<LONG_PTR>(hk_wnd_proc)));

            ImGui::CreateContext();
            auto& io = ImGui::GetIO();
            io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
            io.IniFilename = nullptr;
            ImGui::StyleColorsDark();
            io.FontAllowUserScaling = true;

            ImGui_ImplWin32_Init(window);
            ImGui_ImplOpenGL3_Init(k_glsl_version);

            imgui_init = true;
            sdk::log_info("imgui initialized (classic dark, 2x scale)");
        });
    }

    void render_overlay()
    {
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();

        invoke_overlay();

        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
    }

    void shutdown_overlay()
    {
        if (imgui_init)
        {
            ImGui_ImplOpenGL3_Shutdown();
            ImGui_ImplWin32_Shutdown();
            ImGui::DestroyContext();
        }
    }

    // ── Install / uninstall ─────────────────────────────────────────────

    void do_install()
    {
        sdk::log_info("detecting render API...");

        // 1. Check for already-loaded DirectX DLLs
        static constexpr std::array<const wchar_t*, 6> k_dx_dlls = {
            L"d3d8.dll", L"d3d9.dll",  L"ddraw.dll",
            L"dxgi.dll", L"d3d11.dll", L"d3d12.dll",
        };

        for (const auto* dll : k_dx_dlls)
        {
            if (GetModuleHandleW(dll) != nullptr)
            {
                on_dx_detected();
                break;
            }
        }

        // 2. Hook LoadLibrary to catch late DirectX DLL loads
        ll_a_hook = safetyhook::create_inline(
            reinterpret_cast<void*>(LoadLibraryA),
            reinterpret_cast<void*>(hk_load_library_a));

        ll_w_hook = safetyhook::create_inline(
            reinterpret_cast<void*>(LoadLibraryW),
            reinterpret_cast<void*>(hk_load_library_w));

        // 3. Hook GL functions — validate at call time, not at init.
        auto proc = [](const wchar_t* dll, const char* fn) -> void*
        {
            return reinterpret_cast<void*>(
                GetProcAddress(GetModuleHandleW(dll), fn));
        };

        wgl_swap_hook = safetyhook::create_inline(
            proc(L"opengl32.dll", "wglSwapBuffers"),
            reinterpret_cast<void*>(hk_wgl_swap));

        gl_matrix_mode_hook = safetyhook::create_inline(
            proc(L"opengl32.dll", "glMatrixMode"),
            reinterpret_cast<void*>(hk_gl_matrix_mode));

        gl_load_identity_hook = safetyhook::create_inline(
            proc(L"opengl32.dll", "glLoadIdentity"),
            reinterpret_cast<void*>(hk_gl_load_identity));

        glu_look_at_hook = safetyhook::create_inline(
            proc(L"glu32.dll", "gluLookAt"),
            reinterpret_cast<void*>(hk_glu_look_at));

        sdk::log_info("hooks installed");
    }

    void do_uninstall()
    {
        sdk::log_info("uninstalling render hooks...");
        should_unload.store(true);

        shutdown_overlay();

        if ((window != nullptr) && (original_wnd_proc != nullptr))
        {
            SetWindowLongPtrA(window, GWLP_WNDPROC,
                reinterpret_cast<LONG_PTR>(original_wnd_proc));
        }

        ll_a_hook.reset();
        ll_w_hook.reset();
        wgl_swap_hook.reset();
        gl_matrix_mode_hook.reset();
        gl_load_identity_hook.reset();
        glu_look_at_hook.reset();

        sdk::log_info("render hooks shutdown complete");
    }
};

// ── HookSystem public API ───────────────────────────────────────────────

HookSystem::HookSystem() : pimpl_(std::make_unique<impl>()) {}
HookSystem::~HookSystem() = default;
HookSystem::HookSystem(HookSystem&&) noexcept = default;
HookSystem& HookSystem::operator=(HookSystem&&) noexcept = default;

void HookSystem::install() { pimpl_->do_install(); }
void HookSystem::uninstall() { pimpl_->do_uninstall(); }

api HookSystem::detected_api() const noexcept
{
    return pimpl_->detected.load(std::memory_order::acquire);
}

bool HookSystem::overlay_available() const noexcept
{
    return pimpl_->overlay_avail.load(std::memory_order::acquire);
}

void HookSystem::on_frame(void_fn fn)
{
    std::lock_guard lk{ pimpl_->cb_mutex };
    pimpl_->frame_cbs.push_back(std::move(fn));
}

void HookSystem::on_overlay(void_fn fn)
{
    std::lock_guard lk{ pimpl_->cb_mutex };
    pimpl_->overlay_cbs.push_back(std::move(fn));
}

void HookSystem::on_key_down(key_fn fn)
{
    std::lock_guard lk{ pimpl_->cb_mutex };
    pimpl_->key_cbs.push_back(std::move(fn));
}

void HookSystem::on_gl_identity(identity_fn fn)
{
    std::lock_guard lk{ pimpl_->cb_mutex };
    pimpl_->identity_cbs.push_back(std::move(fn));
}

void HookSystem::on_glu_lookat(lookat_fn fn)
{
    std::lock_guard lk{ pimpl_->cb_mutex };
    pimpl_->lookat_cbs.push_back(std::move(fn));
}

void HookSystem::clear_callbacks()
{
    std::lock_guard lk{ pimpl_->cb_mutex };
    pimpl_->frame_cbs.clear();
    pimpl_->overlay_cbs.clear();
    pimpl_->key_cbs.clear();
    pimpl_->identity_cbs.clear();
    pimpl_->lookat_cbs.clear();
}

void HookSystem::call_orig_glu_lookat(
    double ex, double ey, double ez,
    double cx, double cy, double cz,
    double ux, double uy, double uz)
{
    using fn_t = void(APIENTRY*)(GLdouble, GLdouble, GLdouble,
                                  GLdouble, GLdouble, GLdouble,
                                  GLdouble, GLdouble, GLdouble);
    auto orig = call_orig<fn_t>(pimpl_->glu_look_at_hook);
    if (orig) orig(ex, ey, ez, cx, cy, cz, ux, uy, uz);
}

} // namespace sdk::render
