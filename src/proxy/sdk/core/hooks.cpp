#include "hooks.hpp"

#include "sdk/core/context.hpp"
#include "sdk/core/logging.hpp"
#include "sdk/gl/gl_hooks.hpp"
#include "sdk/lua/lua_state.hpp"
#include "sdk/overlay/overlay.hpp"
#include "sdk/util/win32.hpp"

#include <array>
#include <cctype>
#include <cstring>
#include <memory>
#include <safetyhook.hpp>

namespace sdk
{

// ─── LoadLibrary hooks (catch late DirectX DLL loads) ────────────────────────

static safetyhook::InlineHook g_ll_a_hook;
static safetyhook::InlineHook g_ll_w_hook;

// ─── Lua state (RAII managed) ───────────────────────────────────────────────

static std::unique_ptr<lua::LuaState> g_lua_state;

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

void on_dx_detected()
{
    auto expected = render_api::unknown;
    if (!g_ctx.detected_api.compare_exchange_strong(expected,
                                                    render_api::directx))
    {
        return;
    }

    g_ctx.overlay_available.store(false, std::memory_order::release);

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
    auto expected = render_api::unknown;
    if (!g_ctx.detected_api.compare_exchange_strong(expected,
                                                    render_api::opengl))
    {
        return;
    }

    g_ctx.overlay_available.store(true, std::memory_order::release);

    sdk::log_info("");
    sdk::log_info("╔══════════════════════════════════════════════════════╗");
    sdk::log_info("║  OpenGL renderer confirmed                          ║");
    sdk::log_info("║  Full overlay + cheats + plugins: ACTIVE            ║");
    sdk::log_info("╚══════════════════════════════════════════════════════╝");
    sdk::log_info("");
}

HMODULE WINAPI hk_load_library_a(LPCSTR name)
{
    using fn_t = decltype(&LoadLibraryA);
    auto orig  = reinterpret_cast<fn_t>(g_ll_a_hook.trampoline().address());

    HMODULE result = orig(name);

    if ((result != nullptr) && is_dx_dll_name(name))
    {
        on_dx_detected();
    }

    return result;
}

HMODULE WINAPI hk_load_library_w(LPCWSTR name)
{
    using fn_t = decltype(&LoadLibraryW);
    auto orig  = reinterpret_cast<fn_t>(g_ll_w_hook.trampoline().address());

    HMODULE result = orig(name);

    if ((result != nullptr) && (name != nullptr))
    {
        char buf[128]{};
        WideCharToMultiByte(CP_ACP,
                            0,
                            name,
                            -1,
                            buf,
                            static_cast<int>(sizeof(buf)),
                            nullptr,
                            nullptr);
        if (is_dx_dll_name(buf))
        {
            on_dx_detected();
        }
    }

    return result;
}

// ─── wglSwapBuffers hook — lazy GL detection + overlay entry point ───────────

static BOOL WINAPI hk_wgl_swap(HDC dc)
{
    // Lazy detection: first valid GL frame confirms OpenGL
    if (g_ctx.detected_api.load(std::memory_order::relaxed) ==
        render_api::unknown)
    {
        if ((wglGetCurrentContext() != nullptr) && (GetPixelFormat(dc) != 0))
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
    sdk::log_info("detecting render API...");

    // 1. Check for already-loaded DirectX DLLs (d3d8, d3d9, ddraw, etc.)
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
    g_ll_a_hook =
        safetyhook::create_inline(reinterpret_cast<void*>(LoadLibraryA),
                                  reinterpret_cast<void*>(hk_load_library_a));

    g_ll_w_hook =
        safetyhook::create_inline(reinterpret_cast<void*>(LoadLibraryW),
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
                  .detour = reinterpret_cast<void*>(gl::hk_gl_load_identity) },
        hook_def{ .target = g_ctx.hooks.glu_look_at,
                  .dll    = L"glu32.dll",
                  .proc   = "gluLookAt",
                  .detour = reinterpret_cast<void*>(gl::hk_glu_look_at) },
    };

    for (auto& [target, dll, proc, detour] : hooks)
    {
        target = safetyhook::create_inline(proc_addr(dll, proc), detour);
    }

    sdk::log_info("hooks installed");

    // Initialize Lua state with error handling
    try
    {
        sdk::log_info("creating Lua state...");
        g_lua_state = std::make_unique<lua::LuaState>();
        sdk::log_info("Lua state created");

        sdk::log_info("loading plugins...");
        g_lua_state->load_plugins();
        sdk::log_info("plugins loaded");
    }
    catch (const std::exception& e)
    {
        sdk::log_error("Lua initialization failed: {}", e.what());
        g_lua_state.reset();
    }
    catch (...)
    {
        sdk::log_error("Lua initialization failed: unknown exception");
        g_lua_state.reset();
    }
}

void uninstall_hooks()
{
    sdk::log_info("uninstalling...");
    g_ctx.should_unload.store(true);

    // Clean up Lua state (RAII - destructor handles cleanup)
    if (g_lua_state)
    {
        g_lua_state->unload_plugins();
        g_lua_state.reset();
    }

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

    g_ll_a_hook.reset();
    g_ll_w_hook.reset();
    g_ctx.hooks.reset();
    sdk::log_info("shutdown complete");
}

} // namespace sdk
