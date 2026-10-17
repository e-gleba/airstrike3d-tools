/// @file cheats.hpp
/// @brief Lua-reachable interface into the game's native cheat dispatcher.
///
/// The engine builds shipped under `src/` (2.51, 2.71, Space Strike) expose a
/// single `__cdecl(int)` per-character cheat dispatcher fed from the WndProc
/// `WM_CHAR` path. Feeding a character into that dispatcher reproduces what
/// the keyboard path does — effects, `sounds\cheat.wav`, HUD status text and
/// the keystroke-buffer reset all stay engine-native — without synthesizing
/// Windows input. v2.06 has no cheat system; `cheats_available()` stays
/// false there and callers fall back to `sdk.send_chars`.
///
/// See `docs/re/cheats.md` for the radare2/r2ghidra analysis backing this
/// module and the byte-signature the locator requires.

#pragma once

#include <cstdint>
#include <string_view>

namespace sdk::game::cheat
{

/// True once the loaded game image passed the cheat-dispatcher signature and
/// the entry point was resolved.
[[nodiscard]] bool cheats_available() noexcept;

/// Feed one character to the engine's native cheat dispatcher.
///
/// Only valid from the game/render thread (same thread the WndProc uses).
/// Returns false when the build has no cheat dispatcher (v2.06) or the
/// character is outside the printable ASCII window the engine consumes
/// (`0x20`–`0x7E`).
bool dispatch_cheat_char(std::int32_t ch) noexcept;

/// Feed a full cheat code one character at a time so the engine's own
/// stricmp-triggered effects fire on the final character. The whole code is
/// validated before anything is dispatched, so a malformed code leaves the
/// engine's keystroke buffer untouched.
bool dispatch_cheat(std::string_view code) noexcept;

} // namespace sdk::game::cheat
