#include "sdk.hpp"
#include "scripting_backend.hpp"

#include <mutex>
#include <ranges>
#include <vector>

namespace sdk {

namespace {

// Internal callback storage — hidden from users.
std::recursive_mutex g_mutex;
std::unique_ptr<scripting_backend> g_backend;

std::vector<std::function<void()>> g_on_frame;
std::vector<std::function<void()>> g_on_overlay;
std::vector<std::function<void()>> g_on_gl_identity;
std::vector<std::function<bool(double, double, double, double, double, double, double, double, double)>> g_on_glu_lookat;
std::vector<std::function<bool(int)>> g_on_key_down;
std::vector<std::function<void()>> g_on_load;
std::vector<std::function<void()>> g_on_unload;

} // anonymous namespace

// Public callback registration API
void on_frame(std::function<void()> callback) {
    std::lock_guard lock(g_mutex);
    g_on_frame.push_back(std::move(callback));
}

void on_overlay(std::function<void()> callback) {
    std::lock_guard lock(g_mutex);
    g_on_overlay.push_back(std::move(callback));
}

void on_gl_identity(std::function<void()> callback) {
    std::lock_guard lock(g_mutex);
    g_on_gl_identity.push_back(std::move(callback));
}

void on_glu_lookat(std::function<bool(double, double, double, double, double, double, double, double, double)> callback) {
    std::lock_guard lock(g_mutex);
    g_on_glu_lookat.push_back(std::move(callback));
}

void on_key_down(std::function<bool(int)> callback) {
    std::lock_guard lock(g_mutex);
    g_on_key_down.push_back(std::move(callback));
}

void on_load(std::function<void()> callback) {
    std::lock_guard lock(g_mutex);
    g_on_load.push_back(std::move(callback));
}

void on_unload(std::function<void()> callback) {
    std::lock_guard lock(g_mutex);
    g_on_unload.push_back(std::move(callback));
}

void set_scripting_backend(std::unique_ptr<scripting_backend> backend) {
    std::lock_guard lock(g_mutex);
    g_backend = std::move(backend);
}

scripting_backend* get_scripting_backend() {
    std::lock_guard lock(g_mutex);
    return g_backend.get();
}

// Internal dispatch — called by hooks and internal code
namespace detail {

void invoke_on_frame() {
    std::lock_guard lock(g_mutex);
    std::ranges::for_each(g_on_frame, [](auto& fn) { fn(); });
}

void invoke_on_overlay() {
    std::lock_guard lock(g_mutex);
    std::ranges::for_each(g_on_overlay, [](auto& fn) { fn(); });
}

void invoke_on_gl_identity() {
    std::lock_guard lock(g_mutex);
    std::ranges::for_each(g_on_gl_identity, [](auto& fn) { fn(); });
}

bool invoke_on_glu_lookat(double ex, double ey, double ez, double cx, double cy, double cz, double ux, double uy, double uz) {
    std::lock_guard lock(g_mutex);
    return std::ranges::any_of(g_on_glu_lookat, [&](auto& fn) { return fn(ex, ey, ez, cx, cy, cz, ux, uy, uz); });
}

bool invoke_on_key_down(int vk) {
    std::lock_guard lock(g_mutex);
    return std::ranges::any_of(g_on_key_down, [&](auto& fn) { return fn(vk); });
}

void invoke_on_load() {
    std::lock_guard lock(g_mutex);
    std::ranges::for_each(g_on_load, [](auto& fn) { fn(); });
}

void invoke_on_unload() {
    std::lock_guard lock(g_mutex);
    std::ranges::for_each(g_on_unload, [](auto& fn) { fn(); });
}

void clear_all() {
    std::lock_guard lock(g_mutex);
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
