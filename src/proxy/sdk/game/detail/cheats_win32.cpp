/// @file cheats_win32.cpp
/// @brief Runtime locator for the game's native per-character cheat dispatcher.
///
/// The locator is signature-based (no hard-coded VAs) so it survives the three
/// engine variants shipped in `src/`:
///   - v2.51 `as3d2.exe`        — dispatcher `0x00407110`, prologue `83 EC 0C`
///   - v2.71 `as3d_gulf.exe`    — dispatcher `0x0040706D`, prologue `83 EC 0C`
///   - Space Strike `game.exe`  — dispatcher `0x0041C9D0`, prologue `83 EC 10`
/// v2.06 is ASProtect-packed and carries no cheat strings, so the scan stays
/// unresolved there and callers keep the `send_chars` fallback.
///
/// Discovery shape per `docs/re/cheats.md`:
///   1. cheat literals form one `C`-string cluster in `.rdata`;
///   2. each literal has a `push imm32` xref inside `.text`;
///   3. walking back from the earliest xref over a `C3 CC` padding boundary
///      lands on a `83 EC 0C|10` prologue — the dispatcher entry;
///   4. all xrefs walk back to the same entry.

#include "sdk/game/cheats.hpp"

#include "sdk/core/logging.hpp"

#include <algorithm>
#include <array>
#include <atomic>
#include <cstddef>
#include <cstring>
#include <format>
#include <mutex>
#include <ranges>
#include <span>
#include <string_view>
#include <utility>
#include <vector>
#include <windows.h>

namespace sdk::game::cheat
{

namespace
{

using cheat_char_fn = void(__cdecl*)(std::int32_t);

/// Backtrack limit when searching for the `ret; int3` function boundary.
constexpr std::size_t k_max_prologue_backtrack = 0x400;
/// Max spread between the earliest and latest `push` xref (one function body).
constexpr std::size_t k_max_xref_spread = 0x1000;
/// Minimum resolved cheat literals required before trusting the entry point.
constexpr std::size_t k_min_literals = 5U;

constexpr std::array<std::string_view, 7> k_cheat_literals{
    "invulnerability",    "igonnaliveforever", "showmetheweapons",
    "moremoreweapons",    "glitteringprizes",  "deadlineisnear",
    "diediediemydarling",
};

/// Printable ASCII window the engine's keystroke buffer consumes.
constexpr std::int32_t k_minimum_cheat_char = 0x20;
constexpr std::int32_t k_maximum_cheat_char = 0x7E;

struct scan_state final
{
    std::uintptr_t entry{};    // VA of the dispatcher prologue
    std::size_t    literals{}; // resolved cheat literal count
    bool           valid{};
};

std::mutex        g_scan_mutex;
std::atomic<bool> g_scanned{ false };
scan_state        g_state; // guarded by g_scan_mutex after first resolve

struct image_sections final
{
    const std::byte* text{};
    std::size_t      text_size{};
    const std::byte* data{};
    std::size_t      data_size{};
};

/// Map the executable image sections that host code and cheat literals.
///
/// Uses the base of the main module: the proxy runs in-process with the game.
[[nodiscard]] image_sections read_sections() noexcept
{
    const HMODULE base = GetModuleHandleW(nullptr);
    if (base == nullptr)
    {
        return {};
    }

    const auto* dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(base);
    if (dos->e_magic != IMAGE_DOS_SIGNATURE)
    {
        return {};
    }

    const auto* nt = reinterpret_cast<const IMAGE_NT_HEADERS32*>(
        reinterpret_cast<const std::byte*>(base) + dos->e_lfanew);
    if (nt->Signature != IMAGE_NT_SIGNATURE ||
        nt->OptionalHeader.Magic != IMAGE_NT_OPTIONAL_HDR32_MAGIC)
    {
        return {};
    }

    image_sections sections{};
    const auto* section_header = reinterpret_cast<const IMAGE_SECTION_HEADER*>(
        reinterpret_cast<std::uintptr_t>(nt) +
        FIELD_OFFSET(IMAGE_NT_HEADERS, OptionalHeader) +
        nt->FileHeader.SizeOfOptionalHeader);
    const WORD count = nt->FileHeader.NumberOfSections;
    for (WORD index = 0; index < count; ++index)
    {
        const auto& header = section_header[index];
        if ((header.Characteristics & IMAGE_SCN_MEM_DISCARDABLE) != 0U)
        {
            continue;
        }
        const auto* start =
            reinterpret_cast<const std::byte*>(base) + header.VirtualAddress;
        const std::size_t size = header.Misc.VirtualSize;
        if ((header.Characteristics & IMAGE_SCN_MEM_EXECUTE) != 0U)
        {
            sections.text      = start;
            sections.text_size = size;
        }
        else if ((header.Characteristics & IMAGE_SCN_CNT_INITIALIZED_DATA) !=
                 0U)
        {
            if (sections.data != nullptr && sections.data_size >= size)
            {
                continue; // keep the largest initialized section
            }
            sections.data      = start;
            sections.data_size = size;
        }
    }
    return sections;
}

/// Find a literal inside the initialized-data section and return its VA.
[[nodiscard]] bool resolve_literal_stage(const image_sections& sections,
                                         std::string_view      literal,
                                         std::uintptr_t&       out_va) noexcept
{
    if (sections.data == nullptr)
    {
        return false;
    }
    const std::span<const std::byte> haystack{ sections.data,
                                               sections.data_size };
    const std::span<const std::byte> needle{
        reinterpret_cast<const std::byte*>(literal.data()), literal.size() + 1U
    };
    const auto match = std::ranges::search(haystack, needle);
    if (match.empty())
    {
        return false;
    }
    out_va = reinterpret_cast<std::uintptr_t>(sections.data) +
             static_cast<std::uintptr_t>(match.begin() - haystack.begin());
    return true;
}

/// Find the first `push imm32` in .text whose immediate equals @p target_va.
[[nodiscard]] bool find_push_xref(const image_sections& sections,
                                  std::uintptr_t        target_va,
                                  std::uintptr_t&       out_site) noexcept
{
    if (sections.text == nullptr)
    {
        return false;
    }
    const auto* bytes = reinterpret_cast<const unsigned char*>(sections.text);
    const std::size_t limit = sections.text_size;
    for (std::size_t offset = 0; offset + 5U <= limit; ++offset)
    {
        if (bytes[offset] != 0x68U)
        {
            continue;
        }
        std::uint32_t immediate = 0U;
        std::memcpy(&immediate, bytes + offset + 1U, sizeof(immediate));
        if (static_cast<std::uintptr_t>(immediate) != target_va)
        {
            continue;
        }
        out_site = reinterpret_cast<std::uintptr_t>(sections.text) + offset;
        return true;
    }
    return false;
}

/// Walk back from a push xref to the `ret; int3` boundary, then validate the
/// `83 EC imm8` prologue. Returns 0 on failure.
[[nodiscard]] std::uintptr_t find_function_start(const image_sections& sections,
                                                 std::uintptr_t site) noexcept
{
    const auto* base = reinterpret_cast<const unsigned char*>(sections.text);
    const std::size_t limit  = sections.text_size;
    const std::size_t offset = static_cast<std::size_t>(
        site - reinterpret_cast<std::uintptr_t>(sections.text));
    for (std::size_t back = 2U; back <= k_max_prologue_backtrack &&
                                back <= offset && back + 3U <= limit;
         ++back)
    {
        const std::size_t index = offset - back;
        if (base[index] != 0xC3U || base[index + 1U] != 0xCCU)
        {
            continue;
        }
        const std::size_t prologue = index + 2U;
        if (base[prologue] != 0x83U || base[prologue + 1U] != 0xECU)
        {
            continue; // keep searching: inlined `ret` before other int3 runs
        }
        const BYTE stack_delta = base[prologue + 2U];
        if (stack_delta != 0x0CU && stack_delta != 0x10U)
        {
            continue;
        }
        return reinterpret_cast<std::uintptr_t>(sections.text) + prologue;
    }
    return 0U;
}

[[nodiscard]] scan_state scan_dispatcher() noexcept
{
    scan_state state{};

    const auto sections = read_sections();
    if (sections.text == nullptr || sections.data == nullptr)
    {
        return state;
    }

    // 1. locate cheat literals (cluster in .rdata covers >= 5 of 7).
    constexpr std::size_t k_literal_count = k_cheat_literals.size();
    std::array<std::uintptr_t, k_literal_count> literal_vas{};
    std::size_t                                 resolved = 0U;
    for (std::size_t index = 0; index < k_literal_count; ++index)
    {
        if (resolve_literal_stage(
                sections, k_cheat_literals[index], literal_vas[index]))
        {
            ++resolved;
        }
    }
    if (resolved < k_min_literals)
    {
        return state; // packed stream (v2.06) or a future engine without cheats
    }
    state.literals = resolved;

    // 2. find the first cheat push xrefs.
    std::array<std::uintptr_t, k_literal_count> xrefs{};
    std::size_t                                 found = 0U;
    std::uintptr_t                              earliest_site{};
    bool                                        have_earliest = false;
    for (std::size_t index = 0; index < k_literal_count; ++index)
    {
        if (literal_vas[index] == 0U)
        {
            continue;
        }
        std::uintptr_t site{};
        if (!find_push_xref(sections, literal_vas[index], site))
        {
            continue;
        }
        xrefs[found++] = site;
        if (!have_earliest || site < earliest_site)
        {
            earliest_site = site;
            have_earliest = true;
        }
    }
    if (found < k_min_literals)
    {
        return state;
    }

    // 3. xrefs must cluster inside one function body.
    std::uintptr_t latest_site = earliest_site;
    for (std::size_t index = 0; index < found; ++index)
    {
        latest_site = std::max({ latest_site, xrefs[index] });
    }
    if (latest_site - earliest_site > k_max_xref_spread)
    {
        return state;
    }

    // 4. walk back to the function start from the earliest xref.
    state.entry = find_function_start(sections, earliest_site);
    if (state.entry == 0U)
    {
        return state;
    }

    // 5. every xref must walk back to the same entry point.
    for (std::size_t index = 0; index < found; ++index)
    {
        if (find_function_start(sections, xrefs[index]) != state.entry)
        {
            return state;
        }
    }

    state.valid = true;
    return state;
}

void ensure_scan()
{
    if (g_scanned.load(std::memory_order_acquire))
    {
        return;
    }
    std::lock_guard lock{ g_scan_mutex };
    if (g_scanned.load(std::memory_order_relaxed))
    {
        return;
    }
    g_state = scan_dispatcher();
    if (g_state.valid)
    {
        sdk::log_info(
            std::format("cheat dispatcher resolved at {:#010x} ({} literals)",
                        g_state.entry,
                        g_state.literals));
    }
    else
    {
        sdk::log_info("cheat dispatcher not found; send_chars fallback stays "
                      "active for this build");
    }
    g_scanned.store(true, std::memory_order_release);
}

} // namespace

bool cheats_available() noexcept
{
    try
    {
        ensure_scan();
    }
    catch (...)
    {
        return false;
    }
    return g_state.valid;
}

bool dispatch_cheat_char(std::int32_t ch) noexcept
{
    if (!cheats_available() || ch < k_minimum_cheat_char ||
        ch > k_maximum_cheat_char)
    {
        return false;
    }
    const auto fn = reinterpret_cast<cheat_char_fn>(g_state.entry);
    if (fn == nullptr)
    {
        return false;
    }
    fn(ch);
    return true;
}

bool dispatch_cheat(std::string_view code) noexcept
{
    if (!cheats_available() || code.empty())
    {
        return false;
    }
    // Validate the whole code first: the engine's keystroke buffer is
    // persistent state, so a mid-string invalid character must not leave an
    // un-undoable partial cheat behind.
    for (const char character : code)
    {
        const auto value =
            static_cast<std::int32_t>(static_cast<unsigned char>(character));
        if (value < k_minimum_cheat_char || value > k_maximum_cheat_char)
        {
            return false;
        }
    }
    const auto fn = reinterpret_cast<cheat_char_fn>(g_state.entry);
    if (fn == nullptr)
    {
        return false;
    }
    for (const char character : code)
    {
        fn(static_cast<std::int32_t>(static_cast<unsigned char>(character)));
    }
    return true;
}

} // namespace sdk::game::cheat
