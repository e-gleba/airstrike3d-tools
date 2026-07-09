#include "sdk/core/hooks.hpp"

#include "sdk/core/context.hpp"
#include "sdk/core/contract.hpp"
#include "sdk/core/detail/context_state.hpp"
#include "sdk/core/detail/win32_util.hpp"
#include "sdk/core/logging.hpp"
#include "sdk/graphics/detail/d3d8_hooks.hpp"
#include "sdk/graphics/detail/gl_hooks.hpp"
#include "sdk/graphics/detail/opengl_state.hpp"
#include "sdk/graphics/rendering.hpp"
#include "sdk/overlay/overlay.hpp"
#include "sdk/scripting/engine.hpp"

#include <array>
#include <cctype>
#include <format>
#include <memory>
#include <mutex>
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

[[nodiscard]] bool str_contains_i(std::string_view haystack,
                                  std::string_view needle)
{
    if (haystack.empty() || needle.empty())
    {
        return false;
    }

    return std::ranges::search(
               haystack,
               needle,
               [](char a, char b)
               {
                   return std::tolower(static_cast<unsigned char>(a)) ==
                          std::tolower(static_cast<unsigned char>(b));
               })
               .begin() != haystack.end();
}

[[nodiscard]] bool is_d3d8_dll_name(const char* name)
{
    return name != nullptr && str_contains_i(name, "d3d8");
}

void on_gl_confirmed()
{
    auto expected = render_api::unknown;
    if (!g_ctx.detected_api.compare_exchange_strong(expected,
                                                    render_api::opengl))
    {
        return;
    }

    g_ctx.overlay_available.store(true, std::memory_order_release);
    graphics::detail::set_active_backend(render_api::opengl);

    sdk::log_info("");
    sdk::log_info("╔══════════════════════════════════════════════════════╗");
    sdk::log_info("║  OpenGL renderer confirmed                          ║");
    sdk::log_info("║  Full overlay + plugins: ACTIVE                       ║");
    sdk::log_info("╚══════════════════════════════════════════════════════╝");
    sdk::log_info("");
}

void install_opengl_hooks();

HMODULE WINAPI hk_load_library_a(LPCSTR name)
{
    using fn_t = decltype(&LoadLibraryA);
    const auto orig =
        reinterpret_cast<fn_t>(g_ll_a_hook.trampoline().address());

    HMODULE result = orig(name);

    if ((result != nullptr) && is_d3d8_dll_name(name))
    {
        static_cast<void>(d3d8::install_hooks());
    }
    if ((result != nullptr) && name != nullptr &&
        (str_contains_i(name, "opengl32") || str_contains_i(name, "glu32")))
    {
        install_opengl_hooks();
    }

    return result;
}

HMODULE WINAPI hk_load_library_w(LPCWSTR name)
{
    using fn_t = decltype(&LoadLibraryW);
    const auto orig =
        reinterpret_cast<fn_t>(g_ll_w_hook.trampoline().address());

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
        if (is_d3d8_dll_name(buf.data()))
        {
            static_cast<void>(d3d8::install_hooks());
        }
        if (str_contains_i(buf.data(), "opengl32") ||
            str_contains_i(buf.data(), "glu32"))
        {
            install_opengl_hooks();
        }
    }

    return result;
}

BOOL WINAPI hk_wgl_swap(HDC dc)
{
    if (g_ctx.detected_api.load(std::memory_order::relaxed) ==
        render_api::unknown)
    {
        if ((wglGetCurrentContext() != nullptr) && (GetPixelFormat(dc) != 0))
        {
            on_gl_confirmed();
        }
    }

    gl::finish_frame();

    if (g_ctx.detected_api.load(std::memory_order::acquire) ==
        render_api::opengl)
    {
        overlay::init_opengl(reinterpret_cast<std::uintptr_t>(dc));

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
    return gl::detail::call_orig<wgl_swap_fn>(gl::detail::g_hooks.wgl_swap)(dc);
}

[[nodiscard]] bool try_install_inline_hook(safetyhook::InlineHook& target,
                                           const wchar_t*          dll,
                                           const char*             proc,
                                           void*                   detour)
{
    const auto addr = detail::proc_addr(dll, proc);
    if (addr == nullptr)
    {
        sdk::log_warn(std::format(
            "install_hooks: export '{}' not found (skipped)", proc));
        return false;
    }

    target = safetyhook::create_inline(addr, detour);
    if (!static_cast<bool>(target))
    {
        sdk::log_warn(
            std::format("install_hooks: failed to hook '{}' (skipped)", proc));
        return false;
    }

    return true;
}

void install_opengl_hooks()
{
    static std::mutex     install_mutex;
    const std::lock_guard lock{ install_mutex };

    struct hook_def final
    {
        safetyhook::InlineHook& target;
        const wchar_t*          dll;
        const char*             proc;
        void*                   detour;
    };

    const auto hooks = std::array{
        hook_def{ .target = gl::detail::g_hooks.wgl_swap,
                  .dll    = L"opengl32.dll",
                  .proc   = "wglSwapBuffers",
                  .detour = reinterpret_cast<void*>(hk_wgl_swap) },
        hook_def{ .target = gl::detail::g_hooks.gl_matrix_mode,
                  .dll    = L"opengl32.dll",
                  .proc   = "glMatrixMode",
                  .detour = reinterpret_cast<void*>(gl::hk_gl_matrix_mode) },
        hook_def{ .target = gl::detail::g_hooks.gl_load_identity,
                  .dll    = L"opengl32.dll",
                  .proc   = "glLoadIdentity",
                  .detour = reinterpret_cast<void*>(gl::hk_gl_load_identity) },
        hook_def{ .target = gl::detail::g_hooks.glu_look_at,
                  .dll    = L"glu32.dll",
                  .proc   = "gluLookAt",
                  .detour = reinterpret_cast<void*>(gl::hk_glu_look_at) },
    };

    for (const auto& [target, dll, proc, detour] : hooks)
    {
        if (!target)
        {
            static_cast<void>(
                try_install_inline_hook(target, dll, proc, detour));
        }
    }
}

} // namespace

void install_hooks()
{
    HMODULE pinned_module{};
    ensure(GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                                  GET_MODULE_HANDLE_EX_FLAG_PIN,
                              reinterpret_cast<LPCWSTR>(&install_hooks),
                              &pinned_module) != FALSE,
           "install_hooks: failed to pin proxy module");

    sdk::log_info("detecting render API...");

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

    static_cast<void>(d3d8::install_hooks());

    install_opengl_hooks();

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
        sdk::log_error(
            std::format("script engine initialization failed: {}", e.what()));
        g_script_engine.reset();
    }
    catch (...)
    {
        sdk::log_error(
            "script engine initialization failed: unknown exception");
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

    if (g_ctx.imgui_initialized.load(std::memory_order::acquire))
    {
        overlay::shutdown();
    }

    d3d8::uninstall_hooks();
    g_ll_a_hook.reset();
    g_ll_w_hook.reset();
    gl::detail::g_hooks.reset();
    sdk::log_info("shutdown complete");
}

} // namespace sdk
