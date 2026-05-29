#include "hooks.hpp"

#include "sdk/core/context.hpp"
#include "sdk/core/fallback_overlay.hpp"
#include "sdk/dx8/dx8_hooks.hpp"
#include "sdk/gl/gl_hooks.hpp"
#include "sdk/lua/lua_engine.hpp"
#include "sdk/overlay/overlay.hpp"
#include "sdk/util/win32.hpp"

#include <array>
#include <cctype>
#include <cstring>
#include <safetyhook.hpp>
#include <spdlog/spdlog.h>

namespace sdk
{

// ─── LoadLibrary hooks (catch late DirectX DLL loads) ────────────────────────

static safetyhook::InlineHook g_ll_a_hook;
static safetyhook::InlineHook g_ll_w_hook;

namespace
{

bool str_contains_i(const char* haystack, const char* needle)
{
    if ((haystack == nullptr) || (needle == nullptr))
    {
        return false;
    }

    for (const char* h = haystack; *h != '\0'; ++h)
    {
        const char* n     = needle;
        const char* cur   = h;
        while (*n != '\0' && *cur != '\0'
               && std::tolower(static_cast<unsigned char>(*cur))
                      == std::tolower(static_cast<unsigned char>(*n)))
        {
            ++cur;
            ++n;
        }
        if (*n == '\0')
        {
            return true;
        }
    }
    return false;
}

bool is_dx_dll_name(const char* name)
{
    if (name == nullptr)
    {
        return false;
    }

    static constexpr const char* k_patterns[] = {
        "d3d8", "d3d9", "ddraw", "dxgi", "d3d11", "d3d12",
    };

    for (const auto* p : k_patterns)
    {
        if (str_contains_i(name, p))
        {
            return true;
        }
    }
    return false;
}

// ─── DX8 hook installation (lazy — only when d3d8.dll is actually present) ───

/// Attempts to install the Direct3DCreate8 inline hook.
/// Safe to call at any time — no-ops if already installed or if d3d8.dll
/// isn't loaded yet.  All resolution is dynamic (GetProcAddress) so there
/// is zero static import dependency on d3d8.dll.
static void try_install_dx8_hooks()
{
    if (g_ctx.hooks.d3d8_create)
    {
        return; // Already installed
    }

    HMODULE d3d8 = GetModuleHandleW(L"d3d8.dll");
    if (d3d8 == nullptr)
    {
        return; // d3d8.dll not loaded yet — will retry when it loads
    }

    void* addr = reinterpret_cast<void*>(
        GetProcAddress(d3d8, "Direct3DCreate8"));
    if (addr == nullptr)
    {
        spdlog::warn("[dx8] d3d8.dll loaded but Direct3DCreate8 not found");
        return;
    }

    g_ctx.hooks.d3d8_create = safetyhook::create_inline(
        addr,
        reinterpret_cast<void*>(dx8::hk_direct3d_create8));

    spdlog::info("[dx8] Direct3DCreate8 hooked — waiting for device creation");
}

// ─── Render API detection callbacks ──────────────────────────────────────────

void on_dx_detected()
{
    auto expected = render_api::unknown;
    if (!g_ctx.detected_api.compare_exchange_strong(expected,
                                                     render_api::directx))
    {
        return;
    }

    g_ctx.overlay_available.store(false, std::memory_order::release);

    // Try to hook Direct3DCreate8 — only succeeds if d3d8.dll is loaded.
    // If not loaded yet, the LoadLibrary hooks will retry when it appears.
    try_install_dx8_hooks();

    spdlog::warn("");
    spdlog::warn("╔══════════════════════════════════════════════════════╗");
    spdlog::warn("║  DirectX renderer detected                          ║");
    spdlog::warn("║  Lua plugins & input hooks: ACTIVE                  ║");
    spdlog::warn("║  DX8 hooks: installed if d3d8.dll present           ║");
    spdlog::warn("╚══════════════════════════════════════════════════════╝");
    spdlog::warn("");

    fallback_overlay::install();
}

void on_gl_confirmed()
{
    auto expected = render_api::unknown;
    if (!g_ctx.detected_api.compare_exchange_strong(expected,
                                                     render_api::opengl))
    {
        return;
    }

    g_ctx.overlay_available.store(true, std::memory_order::release);

    spdlog::info("");
    spdlog::info("╔══════════════════════════════════════════════════════╗");
    spdlog::info("║  OpenGL renderer confirmed                          ║");
    spdlog::info("║  Full overlay + cheats + plugins: ACTIVE            ║");
    spdlog::info("╚══════════════════════════════════════════════════════╝");
    spdlog::info("");
}

// ─── LoadLibrary hooks ───────────────────────────────────────────────────────

HMODULE WINAPI hk_load_library_a(LPCSTR name)
{
    using fn_t = decltype(&LoadLibraryA);
    auto orig =
        reinterpret_cast<fn_t>(g_ll_a_hook.trampoline().address());

    HMODULE result = orig(name);

    if ((result != nullptr) && is_dx_dll_name(name))
    {
        on_dx_detected();

        // If d3d8.dll just loaded, install the Direct3DCreate8 hook now
        if (str_contains_i(name, "d3d8"))
        {
            try_install_dx8_hooks();
        }
    }

    return result;
}

HMODULE WINAPI hk_load_library_w(LPCWSTR name)
{
    using fn_t = decltype(&LoadLibraryW);
    auto orig =
        reinterpret_cast<fn_t>(g_ll_w_hook.trampoline().address());

    HMODULE result = orig(name);

    if ((result != nullptr) && (name != nullptr))
    {
        char buf[128]{};
        WideCharToMultiByte(CP_ACP, 0, name, -1, buf,
                            static_cast<int>(sizeof(buf)), nullptr, nullptr);
        if (is_dx_dll_name(buf))
        {
            on_dx_detected();

            if (str_contains_i(buf, "d3d8"))
            {
                try_install_dx8_hooks();
            }
        }
    }

    return result;
}

// ─── wglSwapBuffers hook — lazy GL detection + overlay entry point ───────────

static BOOL WINAPI hk_wgl_swap(HDC dc)
{
    // Lazy detection: first valid GL frame confirms OpenGL
    if (g_ctx.detected_api.load(std::memory_order::relaxed)
        == render_api::unknown)
    {
        if ((wglGetCurrentContext() != nullptr)
            && (GetPixelFormat(dc) != 0))
        {
            on_gl_confirmed();
        }
    }

    // Init overlay on first valid GL frame
    if (g_ctx.overlay_available.load(std::memory_order::acquire))
    {
        overlay::init(dc);

        if (g_ctx.imgui_initialized.load(std::memory_order::acquire))
        {
            g_ctx.cb.on_frame.invoke();

            if (g_ctx.show_ui.load(std::memory_order::relaxed))
            {
                overlay::render();
            }
        }
    }

    using wgl_swap_fn = BOOL(WINAPI*)(HDC);
    return call_orig<wgl_swap_fn>(g_ctx.hooks.wgl_swap)(dc);
}

} // namespace

// ─── Public API ──────────────────────────────────────────────────────────────

void install_hooks()
{
    spdlog::info("[sdk] detecting render API...");

    // 1. Check for already-loaded DirectX DLLs (d3d8, d3d9, ddraw, etc.)
    static constexpr std::array<const wchar_t*, 6> k_dx_dlls = {
        L"d3d8.dll",  L"d3d9.dll",   L"ddraw.dll",
        L"dxgi.dll",  L"d3d11.dll",  L"d3d12.dll",
    };

    for (const auto* dll : k_dx_dlls)
    {
        if (GetModuleHandleW(dll) != nullptr)
        {
            on_dx_detected();

            // If d3d8.dll is the one that's loaded, install its hooks now
            if (std::wcscmp(dll, L"d3d8.dll") == 0)
            {
                try_install_dx8_hooks();
            }
            break;
        }
    }

    // 2. Hook LoadLibrary to catch late DirectX DLL loads
    g_ll_a_hook = safetyhook::create_inline(
        reinterpret_cast<void*>(LoadLibraryA),
        reinterpret_cast<void*>(hk_load_library_a));

    g_ll_w_hook = safetyhook::create_inline(
        reinterpret_cast<void*>(LoadLibraryW),
        reinterpret_cast<void*>(hk_load_library_w));

    // 3. Always hook GL functions — validate at call time, not at init.
    //    Safe even for DX games: hooks are no-ops until API is confirmed.
    using namespace win32;

    struct hook_def final
    {
        safetyhook::InlineHook& target;
        const wchar_t*          dll;
        const char*             proc;
        void*                   detour;
    };

    auto hooks = std::array{
        hook_def{ .target = g_ctx.hooks.wgl_swap,
                  .dll    = L"opengl32.dll",
                  .proc   = "wglSwapBuffers",
                  .detour = reinterpret_cast<void*>(hk_wgl_swap) },
        hook_def{ .target = g_ctx.hooks.gl_matrix_mode,
                  .dll    = L"opengl32.dll",
                  .proc   = "glMatrixMode",
                  .detour = reinterpret_cast<void*>(gl::hk_gl_matrix_mode) },
        hook_def{ .target = g_ctx.hooks.gl_load_identity,
                  .dll    = L"opengl32.dll",
                  .proc   = "glLoadIdentity",
                  .detour =
                      reinterpret_cast<void*>(gl::hk_gl_load_identity) },
        hook_def{ .target = g_ctx.hooks.glu_look_at,
                  .dll    = L"glu32.dll",
                  .proc   = "gluLookAt",
                  .detour =
                      reinterpret_cast<void*>(gl::hk_glu_look_at) },
    };

    for (auto& [target, dll, proc, detour] : hooks)
    {
        target = safetyhook::create_inline(proc_addr(dll, proc), detour);
    }

    spdlog::info("[sdk] hooks installed, loading plugins...");
    lua::load_plugins();
}

void uninstall_hooks()
{
    spdlog::info("[sdk] uninstalling...");
    g_ctx.should_unload.store(true);

    lua::unload_plugins();

    // Remove DX8 vtable hooks before tearing down inline hooks
    dx8::remove_device_hooks();

    if (g_ctx.overlay_available.load(std::memory_order::acquire))
    {
        overlay::shutdown();

        if ((g_ctx.window != nullptr) && (g_ctx.original_wnd_proc != nullptr))
        {
            SetWindowLongPtrA(
                g_ctx.window,
                GWLP_WNDPROC,
                reinterpret_cast<LONG_PTR>(g_ctx.original_wnd_proc));
        }
    }
    else
    {
        fallback_overlay::uninstall();
    }

    g_ll_a_hook.reset();
    g_ll_w_hook.reset();
    g_ctx.hooks.reset();
    spdlog::info("[sdk] shutdown complete");
}

} // namespace sdk
