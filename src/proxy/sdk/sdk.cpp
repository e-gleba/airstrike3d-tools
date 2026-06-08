#include "sdk.hpp"

#include <mutex>
#include <ranges>
#include <vector>

namespace sdk {

namespace {

std::recursive_mutex g_mutex;

std::vector<std::function<void()>> g_on_frame;
std::vector<std::function<void()>> g_on_overlay;
std::vector<std::function<void()>> g_on_gl_identity;
std::vector<std::function<bool(double, double, double, double, double, double, double, double, double)>>
    g_on_glu_lookat;
std::vector<std::function<bool(int)>> g_on_key_down;
std::vector<std::function<void()>> g_on_load;
std::vector<std::function<void()>> g_on_unload;

} // namespace

// ─── Registration ─────────────────────────────────────────────────────────────

void on_frame(std::function<void()> f) {
    std::lock_guard lk{g_mutex};
    g_on_frame.push_back(std::move(f));
}

void on_overlay(std::function<void()> f) {
    std::lock_guard lk{g_mutex};
    g_on_overlay.push_back(std::move(f));
}

void on_gl_identity(std::function<void()> f) {
    std::lock_guard lk{g_mutex};
    g_on_gl_identity.push_back(std::move(f));
}

void on_glu_lookat(
    std::function<bool(double, double, double, double, double, double, double, double, double)> f) {
    std::lock_guard lk{g_mutex};
    g_on_glu_lookat.push_back(std::move(f));
}

void on_key_down(std::function<bool(int)> f) {
    std::lock_guard lk{g_mutex};
    g_on_key_down.push_back(std::move(f));
}

void on_load(std::function<void()> f) {
    std::lock_guard lk{g_mutex};
    g_on_load.push_back(std::move(f));
}

void on_unload(std::function<void()> f) {
    std::lock_guard lk{g_mutex};
    g_on_unload.push_back(std::move(f));
}

// ─── Dispatch (internal) ──────────────────────────────────────────────────────

namespace detail {

void invoke_on_frame() {
    std::lock_guard lk{g_mutex};
    std::ranges::for_each(g_on_frame, [](auto& f) { f(); });
}

void invoke_on_overlay() {
    std::lock_guard lk{g_mutex};
    std::ranges::for_each(g_on_overlay, [](auto& f) { f(); });
}

void invoke_on_gl_identity() {
    std::lock_guard lk{g_mutex};
    std::ranges::for_each(g_on_gl_identity, [](auto& f) { f(); });
}

bool invoke_on_glu_lookat(double ex, double ey, double ez, double cx, double cy, double cz,
                          double ux, double uy, double uz) {
    std::lock_guard lk{g_mutex};
    return std::ranges::any_of(g_on_glu_lookat,
                               [&](auto& f) { return f(ex, ey, ez, cx, cy, cz, ux, uy, uz); });
}

bool invoke_on_key_down(int vk) {
    std::lock_guard lk{g_mutex};
    return std::ranges::any_of(g_on_key_down, [&](auto& f) { return f(vk); });
}

void invoke_on_load() {
    std::lock_guard lk{g_mutex};
    std::ranges::for_each(g_on_load, [](auto& f) { f(); });
}

void invoke_on_unload() {
    std::lock_guard lk{g_mutex};
    std::ranges::for_each(g_on_unload, [](auto& f) { f(); });
}

void clear_all() {
    std::lock_guard lk{g_mutex};
    g_on_frame.clear();
    g_on_overlay.clear();
    g_on_gl_identity.clear();
    g_on_glu_lookat.clear();
    g_on_key_down.clear();
    g_on_load.clear();
    g_on_unload.clear();
}

} // namespace detail
} // namespace sdk
