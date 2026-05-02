#include "hooks.hpp"

#include "sdk/core/context.hpp"
#include "sdk/gl/gl_hooks.hpp"
#include "sdk/lua/lua_engine.hpp"
#include "sdk/overlay/overlay.hpp"
#include "sdk/util/win32.hpp"

#include <safetyhook.hpp>
#include <spdlog/spdlog.h>

namespace sdk
{

static BOOL WINAPI hk_wgl_swap(HDC dc)
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

    using wgl_swap_fn = BOOL(WINAPI*)(HDC);
    return call_orig<wgl_swap_fn>(g_ctx.hooks.wgl_swap)(dc);
}

void install_hooks()
{
    spdlog::info("[sdk] installing hooks...");
    spdlog::default_logger()->flush();

    using namespace win32;

    // Hook definitions: { target_hook, dll, export_name, detour }
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
        spdlog::info("[sdk] hooked {} -> trampoline={:p}", proc, target.trampoline().address());
        spdlog::default_logger()->flush();
    }

    spdlog::info("[sdk] hooks installed, loading plugins...");
    spdlog::default_logger()->flush();
    lua::load_plugins();
    spdlog::info("[sdk] plugins loaded OK");
    spdlog::default_logger()->flush();
}

void uninstall_hooks()
{
    spdlog::info("[sdk] uninstalling...");
    spdlog::default_logger()->flush();
    g_ctx.should_unload.store(true);

    lua::unload_plugins();
    overlay::shutdown();

    if ((g_ctx.window != nullptr) && (g_ctx.original_wnd_proc != nullptr))
    {
        SetWindowLongPtrA(g_ctx.window,
                          GWLP_WNDPROC,
                          reinterpret_cast<LONG_PTR>(g_ctx.original_wnd_proc));
    }

    g_ctx.hooks.reset();
    spdlog::info("[sdk] shutdown complete");
    spdlog::default_logger()->flush();
}

} // namespace sdk
