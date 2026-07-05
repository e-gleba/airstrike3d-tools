#include "sdk/core/hooks.hpp"

#include "sdk/core/context.hpp"
#include "sdk/core/contract.hpp"
#include "sdk/core/detail/context_state.hpp"
#include "sdk/core/detail/win32_util.hpp"
#include "sdk/core/logging.hpp"
#include "sdk/graphics/detail/gl_hooks.hpp"
#include "sdk/overlay/overlay.hpp"
#include "sdk/scripting/engine.hpp"

#include <array>
#include <cctype>
#include <cstring>
#include <format>
#include <memory>
#include <ranges>
#include <safetyhook.hpp>
#include <string_view>
#include <windows.h>

namespace sdk
{

static safetyhook::InlineHook g_ll_a_hook;
static safetyhook::InlineHook g_ll_w_hook;

static std::unique_ptr<scripting::engine> g_script_engine;

namespace
{

[[nodiscard]] bool str_contains_i(std::string_view haystack, std::string_view needle)
{
    if (haystack.empty() || needle.empty())
    {
        return false;
    }

    return std::ranges::search(
        haystack,
        needle,
        [](char a, char b) {
            return std::tolower(static_cast<unsigned char>(a)) ==
                   std::tolower(static_cast<unsigned char>(b));
        }).begin() != haystack.end();
}

[[nodiscard]] bool is_dx_dll_name(const char* name)
{
    if (name == nullptr)
    {
        return false;
    }

    static constexpr std::array<std::string_view, 6> k_patterns = {{
        "d3d8", "d3d9", "ddraw", "dxgi", "d3d11", "d3d12",
    }};

    return std::ranges::any_of(k_patterns, [name](std::string_view pattern) {
        return str_contains_i(name, pattern);
    });
}

void on_dx_detected()
{
    auto expected = render_api::unknown;
    if (!g_ctx.detected_api.compare_exchange_strong(expected, render_api::directx))
    {
        return;
    }

    g_ctx.overlay_available.store(false, std::memory_order_release);

    sdk::log_warn("");
    sdk::log_warn("╔══════════════════════════════════════════════════════╗");
    sdk::log_warn("║  DirectX renderer detected                          ║");
    sdk::log_warn("║  Overlay: DISABLED                                  ║");
    sdk::log_warn("║  Scripting plugins & input hooks: ACTIVE            ║");
    sdk::log_warn("╚══════════════════════════════════════════════════════╝");
    sdk::log_warn("");
}

void on_gl_confirmed()
{
    auto expected = render_api::unknown;
    if (!g_ctx.detected_api.compare_exchange_strong(expected, render_api::opengl))
    {
        return;
    }

    g_ctx.overlay_available.store(true, std::memory_order_release);

    sdk::log_info("");
    sdk::log_info("╔══════════════════════════════════════════════════════╗");
    sdk::log_info("║  OpenGL renderer confirmed                          ║");
    sdk::log_info("║  Full overlay + plugins: ACTIVE                       ║");
    sdk::log_info("╚══════════════════════════════════════════════════════╝");
    sdk::log_info("");
}

HMODULE WINAPI hk_load_library_a(LPCSTR name)
{
    using fn_t = decltype(&LoadLibraryA);
    const auto orig = reinterpret_cast<fn_t>(g_ll_a_hook.trampoline().address());

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
    const auto orig = reinterpret_cast<fn_t>(g_ll_w_hook.trampoline().address());

    HMODULE result = orig(name);

    if ((result != nullptr) && (name != nullptr))
    {
        std::array<char, 128> buf{};
        WideCharToMultiByte(CP_ACP,
                            0,
                            name,
                            -1,
                            buf.data(),
                            static_cast<int>(buf.size()),
                            nullptr,
                            nullptr);
        if (is_dx_dll_name(buf.data()))
        {
            on_dx_detected();
        }
    }

    return result;
}

BOOL WINAPI hk_wgl_swap(HDC dc)
{
    if (g_ctx.detected_api.load(std::memory_order::relaxed) == render_api::unknown)
    {
        if ((wglGetCurrentContext() != nullptr) && (GetPixelFormat(dc) != 0))
        {
            on_gl_confirmed();
        }
    }

    if (g_ctx.overlay_available.load(std::memory_order::acquire))
    {
        overlay::init(reinterpret_cast<std::uintptr_t>(dc));

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
    return detail::call_orig<wgl_swap_fn>(detail::g_state.hooks.wgl_swap)(dc);
}

[[nodiscard]] bool try_install_inline_hook(safetyhook::InlineHook& target,
                                           const wchar_t*          dll,
                                           const char*             proc,
                                           void*                   detour)
{
    const auto addr = detail::proc_addr(dll, proc);
    if (addr == nullptr)
    {
        sdk::log_warn(std::format("install_hooks: export '{}' not found (skipped)", proc));
        return false;
    }

    target = safetyhook::create_inline(addr, detour);
    if (!static_cast<bool>(target))
    {
        sdk::log_warn(std::format("install_hooks: failed to hook '{}' (skipped)", proc));
        return false;
    }

    return true;
}

} // namespace

void install_hooks()
{
    sdk::log_info("detecting render API...");

    static constexpr std::array<const wchar_t*, 6> k_dx_dlls = {{
        L"d3d8.dll", L"d3d9.dll",  L"ddraw.dll",
        L"dxgi.dll", L"d3d11.dll", L"d3d12.dll",
    }};

    if (std::ranges::any_of(k_dx_dlls, [](const wchar_t* dll) {
            return GetModuleHandleW(dll) != nullptr;
        }))
    {
        on_dx_detected();
    }

    g_ll_a_hook =
        safetyhook::create_inline(reinterpret_cast<void*>(LoadLibraryA),
                                  reinterpret_cast<void*>(hk_load_library_a));
    ensure(static_cast<bool>(g_ll_a_hook),
           "install_hooks: failed to hook LoadLibraryA");

    g_ll_w_hook =
        safetyhook::create_inline(reinterpret_cast<void*>(LoadLibraryW),
                                  reinterpret_cast<void*>(hk_load_library_w));
    ensure(static_cast<bool>(g_ll_w_hook),
           "install_hooks: failed to hook LoadLibraryW");

    struct hook_def final
    {
        safetyhook::InlineHook& target;
        const wchar_t*          dll;
        const char*             proc;
        void*                   detour;
    };

    const auto hooks = std::array{
        hook_def{ .target = detail::g_state.hooks.wgl_swap,
                  .dll    = L"opengl32.dll",
                  .proc   = "wglSwapBuffers",
                  .detour = reinterpret_cast<void*>(hk_wgl_swap) },
        hook_def{ .target = detail::g_state.hooks.gl_matrix_mode,
                  .dll    = L"opengl32.dll",
                  .proc   = "glMatrixMode",
                  .detour = reinterpret_cast<void*>(gl::hk_gl_matrix_mode) },
        hook_def{ .target = detail::g_state.hooks.gl_load_identity,
                  .dll    = L"opengl32.dll",
                  .proc   = "glLoadIdentity",
                  .detour = reinterpret_cast<void*>(gl::hk_gl_load_identity) },
        hook_def{ .target = detail::g_state.hooks.glu_look_at,
                  .dll    = L"glu32.dll",
                  .proc   = "gluLookAt",
                  .detour = reinterpret_cast<void*>(gl::hk_glu_look_at) },
    };

    for (const auto& [target, dll, proc, detour] : hooks)
    {
        static_cast<void>(try_install_inline_hook(target, dll, proc, detour));
    }

    sdk::log_info("hooks installed");

    try
    {
        sdk::log_info("creating script engine...");
        g_script_engine = std::make_unique<scripting::engine>();
        sdk::log_info("script engine created");

        sdk::log_info("loading plugins...");
        g_script_engine->load_plugins();
        sdk::log_info("plugins loaded");
    }
    catch (const std::exception& e)
    {
        sdk::log_error(std::format("script engine initialization failed: {}", e.what()));
        g_script_engine.reset();
    }
    catch (...)
    {
        sdk::log_error("script engine initialization failed: unknown exception");
        g_script_engine.reset();
    }
}

void uninstall_hooks()
{
    sdk::log_info("uninstalling...");
    g_ctx.should_unload.store(true);

    if (g_script_engine)
    {
        g_script_engine->unload_plugins();
        g_script_engine.reset();
    }

    if (g_ctx.overlay_available.load(std::memory_order::acquire))
    {
        overlay::shutdown();

        if ((detail::g_state.window != nullptr) &&
            (detail::g_state.original_wnd_proc != nullptr))
        {
            SetWindowLongPtrA(
                detail::g_state.window,
                GWLP_WNDPROC,
                reinterpret_cast<LONG_PTR>(detail::g_state.original_wnd_proc));
        }
    }

    g_ll_a_hook.reset();
    g_ll_w_hook.reset();
    detail::g_state.hooks.reset();
    sdk::log_info("shutdown complete");
}

} // namespace sdk
