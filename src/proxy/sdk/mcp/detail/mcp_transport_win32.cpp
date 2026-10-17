/// @file mcp_transport_win32.cpp
/// @brief Loopback TCP transport for the in-game MCP surface (Win32).
///
/// Frame protocol (text, length-delimited; the host opens one connection per
/// request and the reply closes it):
///
///   request  `"<OP> <body-length>\n<body>"`
///   reply    `"OK <body-length>\n<body>"` or `"ERR <body-length>\n<body>"`
///
/// Ops:
///
///   - `STATUS`  — answered on the listener thread; touches no live game
///     state beyond read-only option surfaces.
///   - `RUN_LUA` — queued for the render thread via `take_request()` and
///     resolved through `complete_request()`.
///
/// Thread model: `start()` spawns the listener thread detached; it lives
/// until the process exits (game shutdown never joins it). Connections are
/// serialized — the listener serves them one at a time, so at most one
/// `RUN_LUA` request is in flight.

#include "sdk/mcp/mcp.hpp"

#include "sdk/core/logging.hpp"
#include "sdk/game/cheats.hpp"
#include "sdk/graphics/rendering.hpp"

#include <atomic>
#include <charconv>
#include <chrono>
#include <condition_variable>
#include <format>
#include <mutex>
#include <string>
#include <string_view>
#include <system_error>
#include <thread>
#include <utility>

#include <winsock2.h>

#include <windows.h>

namespace sdk::mcp
{

namespace
{

/// Loopback port when `AS3D_MCP_PORT` is unset. One game instance at a time
/// owns it; a second concurrent game logs a warning and stays disabled.
constexpr std::uint16_t k_default_port = 13373;

/// Port override environment variable (host-side launcher sets it to match
/// the bridge script).
constexpr const char* k_port_env = "AS3D_MCP_PORT";

/// Render-thread drain deadline for a queued `RUN_LUA` request.
constexpr std::chrono::seconds k_run_deadline{ 5 };

/// Caps so a greedy client can never drive unbounded allocations.
constexpr std::size_t k_max_code_bytes    = 1024U * 1024U;
constexpr std::size_t k_max_outcome_bytes = 256U * 1024U;
/// Header line budget for the `"<OP> <length>\n"` shape.
constexpr std::size_t k_max_header_bytes = 128U;

/// Blocking socket stalls, so a stalled peer can never pin the listener.
constexpr DWORD k_socket_timeout_ms = 30'000U;

std::mutex              g_mutex;
std::condition_variable g_response_event;
std::string             g_request_code; //< guarded by g_mutex
std::uint64_t           g_request_id{ 0U };
std::string             g_response_outcome; //< guarded by g_mutex
std::uint64_t           g_response_id{ 0U };
bool                    g_request_ready{ false };
bool                    g_response_ready{ false };
/// Fast path for the per-frame drain poll (checked before the mutex).
std::atomic<bool> g_request_pending{ false };
std::atomic<bool> g_started{ false };

const std::chrono::steady_clock::time_point g_boot{
    std::chrono::steady_clock::now()
};

/// Deliberately never closed: the listener is torn down with the process.
SOCKET g_listener = INVALID_SOCKET;

/// @brief Opens the loopback listener socket.
[[nodiscard]] SOCKET bind_listener(std::uint16_t port) noexcept
{
    const SOCKET sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (sock == INVALID_SOCKET)
    {
        return INVALID_SOCKET;
    }

    DWORD                      timeout = k_socket_timeout_ms;
    constexpr auto             size    = static_cast<int>(sizeof(timeout));
    [[maybe_unused]] const int receive_option =
        setsockopt(sock,
                   SOL_SOCKET,
                   SO_RCVTIMEO,
                   reinterpret_cast<const char*>(&timeout),
                   size);
    [[maybe_unused]] const int send_option =
        setsockopt(sock,
                   SOL_SOCKET,
                   SO_SNDTIMEO,
                   reinterpret_cast<const char*>(&timeout),
                   size);

    sockaddr_in address{};
    address.sin_family      = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    address.sin_port        = htons(port);
    if (bind(sock,
             reinterpret_cast<const sockaddr*>(&address),
             sizeof(address)) != 0 ||
        listen(sock, 4) != 0)
    {
        closesocket(sock);
        return INVALID_SOCKET;
    }
    return sock;
}

/// @brief Reads exactly @p size bytes from @p socket.
[[nodiscard]] bool read_exact(SOCKET      socket,
                              char*       data,
                              std::size_t size) noexcept
{
    std::size_t consumed = 0U;
    while (consumed < size)
    {
        const int chunk =
            recv(socket, data + consumed, static_cast<int>(size - consumed), 0);
        if (chunk <= 0)
        {
            return false;
        }
        consumed += static_cast<std::size_t>(chunk);
    }
    return true;
}

/// @brief Writes exactly @p size bytes to @p socket.
[[nodiscard]] bool send_all(SOCKET      socket,
                            const char* data,
                            std::size_t size) noexcept
{
    std::size_t written = 0U;
    while (written < size)
    {
        const int chunk =
            send(socket, data + written, static_cast<int>(size - written), 0);
        if (chunk <= 0)
        {
            return false;
        }
        written += static_cast<std::size_t>(chunk);
    }
    return true;
}

/// @brief Reads the `"<OP> <length>\n"` request header.
[[nodiscard]] bool read_header(SOCKET       socket,
                               std::string& op,
                               std::size_t& body_size)
{
    std::string line;
    char        byte = 0;
    while (line.size() < k_max_header_bytes)
    {
        if (!read_exact(socket, &byte, 1U))
        {
            return false;
        }
        if (byte == '\n')
        {
            const auto space = line.find(' ');
            if (space == std::string::npos)
            {
                return false;
            }
            op                = line.substr(0U, space);
            const auto text   = line.substr(space + 1U);
            const auto parsed = std::from_chars(
                text.data(), text.data() + text.size(), body_size);
            return parsed.ec == std::errc() &&
                   parsed.ptr == text.data() + text.size();
        }
        line.push_back(byte);
    }
    return false;
}

/// @brief Reads one `<OP> <length>\n<body>` request frame.
[[nodiscard]] bool read_frame(SOCKET socket, std::string& op, std::string& body)
{
    std::size_t body_size = 0U;
    if (!read_header(socket, op, body_size) || body_size > k_max_code_bytes)
    {
        return false;
    }
    body.resize(body_size);
    return body_size == 0U || read_exact(socket, body.data(), body_size);
}

/// @brief Sends one `OK|ERR <length>\n<body>` reply frame.
[[nodiscard]] bool send_reply(SOCKET socket, bool ok, std::string_view payload)
{
    const std::string prefix =
        std::format("{} {}\n", ok ? "OK" : "ERR", payload.size());
    return send_all(socket, prefix.data(), prefix.size()) &&
           send_all(socket, payload.data(), payload.size());
}

/// @brief Converts a wide string to UTF-8 narrow text (best effort).
[[nodiscard]] std::string to_utf8(std::wstring_view value)
{
    if (value.empty())
    {
        return {};
    }
    const int size = WideCharToMultiByte(CP_UTF8,
                                         0,
                                         value.data(),
                                         static_cast<int>(value.size()),
                                         nullptr,
                                         0,
                                         nullptr,
                                         nullptr);
    if (size <= 0)
    {
        return {};
    }
    std::string                out(static_cast<std::size_t>(size), '\0');
    [[maybe_unused]] const int written =
        WideCharToMultiByte(CP_UTF8,
                            0,
                            value.data(),
                            static_cast<int>(value.size()),
                            out.data(),
                            size,
                            nullptr,
                            nullptr);
    return out;
}

/// @brief Builds the STATUS body on the listener thread.
///
/// Touches option surfaces only; it never mutates game state.
/// `cheats_available()` lazily runs the dispatcher scan on the first call,
/// which is what a modding tool wants to see anyway.
[[nodiscard]] std::string collect_status()
{
    wchar_t                      exe_path[MAX_PATH]{};
    [[maybe_unused]] const DWORD copied =
        GetModuleFileNameW(nullptr, exe_path, MAX_PATH);
    const std::wstring path{ exe_path };
    const auto         slash = path.find_last_of(L"\\/");
    const std::string  exe_name =
        to_utf8(slash == std::wstring::npos ? path : path.substr(slash + 1U));

    std::string_view backend{ "unknown" };
    switch (graphics::active_backend())
    {
        case render_api::direct3d8:
            backend = "direct3d8";
            break;
        case render_api::opengl:
            backend = "opengl";
            break;
        case render_api::unknown:
            break;
    }

    const auto uptime = std::chrono::duration_cast<std::chrono::seconds>(
                            std::chrono::steady_clock::now() - g_boot)
                            .count();

    return std::format(
        "exe={}\nbackend={}\ncheats_available={}\nfullscreen_4k={}\n"
        "uptime_seconds={}\n",
        exe_name,
        backend,
        game::cheat::cheats_available() ? "true" : "false",
        graphics::get_visual_settings().fullscreen_4k ? "true" : "false",
        uptime);
}

/// @brief Queues a `RUN_LUA` request and waits for the rendered outcome.
[[nodiscard]] bool run_request(SOCKET socket, std::string code)
{
    std::uint64_t id = 0U;
    {
        std::lock_guard lock{ g_mutex };
        id              = ++g_request_id;
        g_request_code  = std::move(code);
        g_request_ready = true;
    }
    g_request_pending.store(true, std::memory_order_release);

    std::unique_lock lock{ g_mutex };
    const bool       drained = g_response_event.wait_for(
        lock,
        k_run_deadline,
        [&]() { return g_response_ready && g_response_id == id; });
    if (!drained)
    {
        lock.unlock();
        [[maybe_unused]] const bool timeout_reply = send_reply(
            socket,
            false,
            "error:render thread did not drain the request within 5s");
        return false;
    }
    std::string outcome = std::move(g_response_outcome);
    g_response_ready    = false;
    lock.unlock();

    return send_reply(socket, outcome.starts_with("ok"), outcome);
}

/// @brief Serves exactly one request on the current connection.
[[nodiscard]] bool serve_connection(SOCKET socket)
{
    std::string op;
    std::string body;
    if (!read_frame(socket, op, body))
    {
        return false;
    }

    if (op == "STATUS")
    {
        return send_reply(socket, true, collect_status());
    }
    if (op == "RUN_LUA")
    {
        return run_request(socket, std::move(body));
    }
    return send_reply(socket, false, "error:unknown op");
}

/// @brief Listener thread: one request per connection, forever.
void serve_forever() noexcept
{
    for (;;)
    {
        const SOCKET client = accept(g_listener, nullptr, nullptr);
        if (client == INVALID_SOCKET)
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            continue;
        }
        try
        {
            [[maybe_unused]] const bool served = serve_connection(client);
        }
        catch (...)
        {
            sdk::log_error("mcp transport: request handler failed");
        }
        closesocket(client);
    }
}

/// @brief Resolves the listener port from `AS3D_MCP_PORT` when present.
[[nodiscard]] std::uint16_t port_from_env() noexcept
{
    constexpr auto k_buffer_size = 16U;

    char        raw_value[k_buffer_size]{};
    const DWORD written = GetEnvironmentVariableA(
        k_port_env, raw_value, static_cast<DWORD>(sizeof(raw_value)));
    if (written == 0U || written >= sizeof(raw_value))
    {
        return k_default_port;
    }
    const std::string_view text{ raw_value, static_cast<std::size_t>(written) };
    std::uint16_t          port = 0U;
    const auto             parsed =
        std::from_chars(text.data(), text.data() + text.size(), port);
    return parsed.ec == std::errc() ? port : k_default_port;
}

} // namespace

void start() noexcept
{
    if (g_started.load(std::memory_order_acquire))
    {
        return;
    }

    WSADATA wsa_data{};
    if (WSAStartup(MAKEWORD(2U, 2U), &wsa_data) != 0)
    {
        sdk::log_error("mcp transport: WSAStartup failed");
        return;
    }

    const auto port = port_from_env();
    g_listener      = bind_listener(port);
    if (g_listener == INVALID_SOCKET)
    {
        sdk::log_warn(std::format("mcp transport disabled: cannot listen on "
                                  "127.0.0.1:{} (port busy?)",
                                  port));
        [[maybe_unused]] const int first_cleanup = WSACleanup();
        return;
    }

    try
    {
        std::thread(serve_forever).detach();
    }
    catch (...)
    {
        sdk::log_error("mcp transport: listener thread spawn failed");
        closesocket(g_listener);
        g_listener                                = INVALID_SOCKET;
        [[maybe_unused]] const int second_cleanup = WSACleanup();
        return;
    }

    g_started.store(true, std::memory_order_release);
    sdk::log_info(std::format("mcp transport listening on 127.0.0.1:{} "
                              "(override with AS3D_MCP_PORT)",
                              port));
}

std::string take_request(std::uint64_t& out_id) noexcept
{
    if (!g_request_pending.load(std::memory_order_acquire))
    {
        return {};
    }

    std::string code;
    try
    {
        std::lock_guard lock{ g_mutex };
        if (!g_request_ready)
        {
            return {};
        }
        code            = std::move(g_request_code);
        g_request_code  = {};
        out_id          = g_request_id;
        g_request_ready = false;
    }
    catch (...)
    {
        return {};
    }
    g_request_pending.store(false, std::memory_order_release);
    return code;
}

void complete_request(std::uint64_t id, std::string_view outcome) noexcept
{
    try
    {
        {
            std::lock_guard lock{ g_mutex };
            g_response_outcome.assign(outcome.substr(0, k_max_outcome_bytes));
            g_response_id    = id;
            g_response_ready = true;
        }
        g_response_event.notify_one();
    }
    catch (...)
    {
    }
}

} // namespace sdk::mcp
