#!/usr/bin/env python3
# fmt: off
# ruff: noqa
"""
Airstrike 3D II Save Editor

A modern CLI tool for editing Divo Games save files.
Supports viewing, editing scores, unlocking missions, and managing player profiles.

Usage:
    save_editor.py info <save_file>
    save_editor.py decrypt <save_file> [-o OUTPUT]
    save_editor.py encrypt <payload> -o OUTPUT --from-save <donor>
    save_editor.py scores <save_file> [--set INDEX:SCORE:LEVEL:NAME]... [-o OUTPUT]
    save_editor.py missions <save_file> [--unlock-all | --lock-all | --unlock N]... [-o OUTPUT]
    save_editor.py players <save_file> [--set INDEX:NAME]... [-o OUTPUT]
"""
# fmt: on

from __future__ import annotations

import argparse
import struct
import sys
from dataclasses import dataclass, field
from itertools import cycle
from pathlib import Path
from typing import Self

__version__ = "1.0.0"

# ─────────────────────────────────────────────────────────────────────────────
# Constants derived from Ghidra RE analysis
# ─────────────────────────────────────────────────────────────────────────────
HDR_SIZE = 0x108  # Header: magic(4) + key(256) + crc(4)
DATA_SIZE = 0x574  # Payload size
KEY_SIZE = 256
TOTAL_SIZE = HDR_SIZE + DATA_SIZE

# Save structure offsets (in decrypted payload)
# Derived from Ghidra RE + binary analysis
PAYLOAD_HEADER_SIZE = 4  # 4-byte header (zeros)

HIGHSCORE_ENTRY_SIZE = 40  # 0x28: name(32) + score(4) + level(4)
HIGHSCORE_COUNT = 15
HIGHSCORES_OFFSET = PAYLOAD_HEADER_SIZE  # 0x04
HIGHSCORES_END = HIGHSCORES_OFFSET + HIGHSCORE_ENTRY_SIZE * HIGHSCORE_COUNT  # 0x25C

PLAYER_ENTRY_SIZE = 33  # 0x21: unlocked(1) + name(32)
PLAYER_COUNT = 6
PLAYERS_OFFSET = HIGHSCORES_END  # 0x25C
PLAYERS_END = PLAYERS_OFFSET + PLAYER_ENTRY_SIZE * PLAYER_COUNT  # 0x322

MISSION_ENTRY_SIZE = 33  # 0x21: completed(1) + name(32)
MISSION_COUNT = 18
MISSIONS_OFFSET = PLAYERS_END  # 0x322


# ─────────────────────────────────────────────────────────────────────────────
# Terminal styling
# ─────────────────────────────────────────────────────────────────────────────
class Style:
    """ANSI escape codes for terminal styling."""

    RESET = "\033[0m"
    BOLD = "\033[1m"
    DIM = "\033[2m"
    RED = "\033[31m"
    GREEN = "\033[32m"
    YELLOW = "\033[33m"
    BLUE = "\033[34m"
    MAGENTA = "\033[35m"
    CYAN = "\033[36m"

    @classmethod
    def enabled(cls) -> bool:
        return sys.stdout.isatty()

    @classmethod
    def fmt(cls, text: str, *codes: str) -> str:
        if not cls.enabled():
            return text
        return "".join(codes) + text + cls.RESET


def info(msg: str) -> None:
    print(f"{Style.fmt('•', Style.BLUE)} {msg}", file=sys.stderr)


def success(msg: str) -> None:
    print(f"{Style.fmt('✓', Style.GREEN, Style.BOLD)} {msg}", file=sys.stderr)


def warn(msg: str) -> None:
    print(f"{Style.fmt('⚠', Style.YELLOW, Style.BOLD)} {msg}", file=sys.stderr)


def error(msg: str) -> None:
    print(f"{Style.fmt('✗', Style.RED, Style.BOLD)} {msg}", file=sys.stderr)


def die(msg: str) -> None:
    error(msg)
    sys.exit(1)


# ─────────────────────────────────────────────────────────────────────────────
# Crypto primitives (from Ghidra: crc16_calculate @ 004021a0, data_xor_decrypt @ 004031d0)
# ─────────────────────────────────────────────────────────────────────────────
def crc16_ccitt(data: bytes, init: int = 0xFFFF) -> int:
    """CRC-16/CCITT-FALSE: poly=0x1021, init=0xFFFF, no reflection."""
    crc = init
    for byte in data:
        crc ^= byte << 8
        for _ in range(8):
            crc = ((crc << 1) ^ 0x1021) if crc & 0x8000 else crc << 1
            crc &= 0xFFFF
    return crc


def xor_cipher(data: bytes, key: bytes) -> bytes:
    """XOR cipher with cycling key."""
    return bytes(d ^ k for d, k in zip(data, cycle(key)))


# ─────────────────────────────────────────────────────────────────────────────
# Data structures
# ─────────────────────────────────────────────────────────────────────────────
@dataclass
class HighscoreEntry:
    """
    High score entry (40 bytes / 0x28):
    - name: 32 bytes, null-terminated string
    - score: u32 little-endian
    - level: u32 little-endian (reached level)
    """

    name: str
    score: int
    level: int

    SIZE = HIGHSCORE_ENTRY_SIZE

    @classmethod
    def unpack(cls, data: bytes) -> Self:
        name = data[:32].split(b"\x00", 1)[0].decode("latin-1")
        score, level = struct.unpack_from("<II", data, 32)
        return cls(name=name, score=score, level=level)

    def pack(self) -> bytes:
        name_bytes = self.name.encode("latin-1")[:32].ljust(32, b"\x00")
        return name_bytes + struct.pack("<II", self.score, self.level)


@dataclass
class PlayerEntry:
    """
    Player slot entry (33 bytes / 0x21):
    - unlocked: u8 (0 or 1)
    - name: 32 bytes, null-terminated string
    """

    unlocked: bool
    name: str

    SIZE = PLAYER_ENTRY_SIZE

    @classmethod
    def unpack(cls, data: bytes) -> Self:
        unlocked = data[0] != 0
        name = data[1:33].split(b"\x00", 1)[0].decode("latin-1")
        return cls(unlocked=unlocked, name=name)

    def pack(self) -> bytes:
        name_bytes = self.name.encode("latin-1")[:32].ljust(32, b"\x00")
        return bytes([1 if self.unlocked else 0]) + name_bytes


@dataclass
class MissionEntry:
    """
    Mission entry (33 bytes / 0x21):
    - completed: u8 (0 or 1)
    - name: 32 bytes, null-terminated string
    """

    completed: bool
    name: str

    SIZE = MISSION_ENTRY_SIZE

    @classmethod
    def unpack(cls, data: bytes) -> Self:
        completed = data[0] != 0
        name = data[1:33].split(b"\x00", 1)[0].decode("latin-1")
        return cls(completed=completed, name=name)

    def pack(self) -> bytes:
        name_bytes = self.name.encode("latin-1")[:32].ljust(32, b"\x00")
        return bytes([1 if self.completed else 0]) + name_bytes


@dataclass
class SavePayload:
    """Decrypted save file payload structure."""

    highscores: list[HighscoreEntry] = field(default_factory=list)
    players: list[PlayerEntry] = field(default_factory=list)
    missions: list[MissionEntry] = field(default_factory=list)
    _header: bytes = field(default=b"\x00\x00\x00\x00", repr=False)
    _raw_tail: bytes = field(default=b"", repr=False)

    @classmethod
    def parse(cls, data: bytes) -> Self:
        if len(data) != DATA_SIZE:
            warn(f"Payload size mismatch: {len(data)} (expected {DATA_SIZE})")

        header = data[:PAYLOAD_HEADER_SIZE]

        highscores = [
            HighscoreEntry.unpack(
                data[HIGHSCORES_OFFSET + i * HIGHSCORE_ENTRY_SIZE : HIGHSCORES_OFFSET + (i + 1) * HIGHSCORE_ENTRY_SIZE]
            )
            for i in range(HIGHSCORE_COUNT)
        ]

        players = [
            PlayerEntry.unpack(
                data[PLAYERS_OFFSET + i * PLAYER_ENTRY_SIZE : PLAYERS_OFFSET + (i + 1) * PLAYER_ENTRY_SIZE]
            )
            for i in range(PLAYER_COUNT)
        ]

        missions = [
            MissionEntry.unpack(
                data[MISSIONS_OFFSET + i * MISSION_ENTRY_SIZE : MISSIONS_OFFSET + (i + 1) * MISSION_ENTRY_SIZE]
            )
            for i in range(MISSION_COUNT)
        ]

        tail_offset = MISSIONS_OFFSET + MISSION_COUNT * MISSION_ENTRY_SIZE
        raw_tail = data[tail_offset:]

        return cls(highscores=highscores, players=players, missions=missions, _header=header, _raw_tail=raw_tail)

    def serialize(self) -> bytes:
        parts = [self._header]
        parts += [hs.pack() for hs in self.highscores]
        parts += [p.pack() for p in self.players]
        parts += [m.pack() for m in self.missions]
        parts.append(self._raw_tail)
        result = b"".join(parts)
        # Pad or truncate to exact size
        if len(result) < DATA_SIZE:
            result = result.ljust(DATA_SIZE, b"\x00")
        return result[:DATA_SIZE]


@dataclass
class SaveFile:
    """
    Complete save file with header and encrypted payload.

    Header layout (0x108 bytes):
    - magic: f32 = 1.0 (4 bytes)
    - key: 256 bytes XOR key
    - crc: u16 + u16 padding (4 bytes)
    """

    magic: float
    key: bytes
    stored_crc: int
    encrypted_payload: bytes
    _decrypted: bytes | None = field(default=None, repr=False)

    @classmethod
    def load(cls, path: Path) -> Self:
        data = path.read_bytes()
        if len(data) != TOTAL_SIZE:
            warn(f"File size: {len(data)} bytes (expected {TOTAL_SIZE})")

        magic = struct.unpack_from("<f", data, 0)[0]
        key = data[4:260]
        stored_crc = struct.unpack_from("<H", data, 260)[0]
        encrypted_payload = data[HDR_SIZE : HDR_SIZE + DATA_SIZE]

        return cls(magic=magic, key=key, stored_crc=stored_crc, encrypted_payload=encrypted_payload)

    def decrypt(self) -> bytes:
        if self._decrypted is None:
            self._decrypted = xor_cipher(self.encrypted_payload, self.key)
        return self._decrypted

    def payload(self) -> SavePayload:
        return SavePayload.parse(self.decrypt())

    def verify_crc(self) -> bool:
        computed = crc16_ccitt(self.encrypted_payload)
        return computed == self.stored_crc

    @classmethod
    def build(cls, payload: bytes, key: bytes) -> bytes:
        """Build a complete save file from payload and key."""
        if len(payload) != DATA_SIZE:
            raise ValueError(f"Payload must be {DATA_SIZE} bytes")
        if len(key) != KEY_SIZE:
            raise ValueError(f"Key must be {KEY_SIZE} bytes")

        encrypted = xor_cipher(payload, key)
        crc = crc16_ccitt(encrypted)

        header = struct.pack("<f", 1.0) + key + struct.pack("<HH", crc, 0)
        return header + encrypted


# ─────────────────────────────────────────────────────────────────────────────
# Display helpers
# ─────────────────────────────────────────────────────────────────────────────
def hexdump(data: bytes, offset: int = 0, limit: int | None = None) -> str:
    """Format bytes as a hex dump with ASCII representation."""
    lines = []
    data = data[:limit] if limit else data
    for i in range(0, len(data), 16):
        chunk = data[i : i + 16]
        hex_part = " ".join(f"{b:02x}" for b in chunk).ljust(47)
        ascii_part = "".join(chr(b) if 32 <= b < 127 else "." for b in chunk)
        addr = Style.fmt(f"{offset + i:04x}", Style.DIM)
        lines.append(f"{addr}  {hex_part}  {Style.fmt(f'|{ascii_part}|', Style.CYAN)}")
    return "\n".join(lines)


def format_table(headers: list[str], rows: list[list[str]], col_styles: list[str] | None = None) -> str:
    """Format data as an aligned table."""
    widths = [len(h) for h in headers]
    for row in rows:
        for i, cell in enumerate(row):
            # Strip ANSI for width calculation
            plain = cell
            for code in [Style.RESET, Style.BOLD, Style.DIM, Style.RED, Style.GREEN, Style.YELLOW, Style.CYAN]:
                plain = plain.replace(code, "")
            widths[i] = max(widths[i], len(plain))

    header_line = "  ".join(Style.fmt(h.ljust(widths[i]), Style.BOLD) for i, h in enumerate(headers))
    sep_line = Style.fmt("─" * (sum(widths) + 2 * (len(widths) - 1)), Style.DIM)

    lines = [header_line, sep_line]
    for row in rows:
        cells = []
        for i, cell in enumerate(row):
            # Pad considering ANSI codes
            plain = cell
            for code in [Style.RESET, Style.BOLD, Style.DIM, Style.RED, Style.GREEN, Style.YELLOW, Style.CYAN]:
                plain = plain.replace(code, "")
            padding = widths[i] - len(plain)
            cells.append(cell + " " * padding)
        lines.append("  ".join(cells))
    return "\n".join(lines)


# ─────────────────────────────────────────────────────────────────────────────
# Commands
# ─────────────────────────────────────────────────────────────────────────────
def cmd_info(args: argparse.Namespace) -> None:
    """Display save file information."""
    save = SaveFile.load(args.input)
    payload = save.payload()

    crc_ok = save.verify_crc()
    crc_status = Style.fmt("✓ valid", Style.GREEN) if crc_ok else Style.fmt("✗ invalid", Style.RED)

    print(f"{Style.fmt('File:', Style.BOLD)}     {args.input.name}")
    print(f"{Style.fmt('Size:', Style.BOLD)}     {TOTAL_SIZE} bytes (header: {HDR_SIZE}, data: {DATA_SIZE})")
    print(f"{Style.fmt('Magic:', Style.BOLD)}    {save.magic:.1f}")
    print(f"{Style.fmt('CRC-16:', Style.BOLD)}   0x{save.stored_crc:04x} {crc_status}")
    print(f"{Style.fmt('Key:', Style.BOLD)}      {save.key[:8].hex()}... ({len(set(save.key))} unique bytes)")

    # High scores
    print(f"\n{Style.fmt('═══ HIGH SCORES ═══', Style.BOLD, Style.CYAN)}")
    rows = []
    for i, hs in enumerate(payload.highscores):
        if hs.score > 0 or hs.name:
            score_fmt = Style.fmt(f"{hs.score:,}", Style.YELLOW) if hs.score else Style.fmt("0", Style.DIM)
            rows.append([str(i + 1), hs.name or Style.fmt("(empty)", Style.DIM), score_fmt, str(hs.level)])
    if rows:
        print(format_table(["#", "Name", "Score", "Level"], rows))

    # Players
    print(f"\n{Style.fmt('═══ PLAYER SLOTS ═══', Style.BOLD, Style.CYAN)}")
    rows = []
    for i, p in enumerate(payload.players):
        status = Style.fmt("●", Style.GREEN) if p.unlocked else Style.fmt("○", Style.DIM)
        rows.append([str(i + 1), status, p.name or Style.fmt("(empty)", Style.DIM)])
    print(format_table(["#", "Unlocked", "Name"], rows))

    # Missions
    print(f"\n{Style.fmt('═══ MISSIONS ═══', Style.BOLD, Style.CYAN)}")
    completed = sum(1 for m in payload.missions if m.completed)
    print(f"Progress: {completed}/{len(payload.missions)}")
    rows = []
    for i, m in enumerate(payload.missions):
        status = Style.fmt("✓", Style.GREEN) if m.completed else Style.fmt("○", Style.DIM)
        rows.append([str(i + 1), status, m.name])
    print(format_table(["#", "Done", "Mission"], rows))


def cmd_decrypt(args: argparse.Namespace) -> None:
    """Decrypt and output payload."""
    save = SaveFile.load(args.input)
    data = save.decrypt()

    if args.output:
        if args.output.exists() and not args.force:
            die(f"Output file exists: {args.output} (use -f to overwrite)")
        args.output.write_bytes(data)
        success(f"Wrote {len(data)} bytes to {args.output}")
    elif args.format == "raw":
        sys.stdout.buffer.write(data)
    elif args.format == "hex":
        print(hexdump(data, HDR_SIZE))
    else:
        print(data.decode("latin-1", errors="replace"))


def cmd_encrypt(args: argparse.Namespace) -> None:
    """Encrypt payload into save file."""
    payload = args.payload.read_bytes()
    if len(payload) != DATA_SIZE:
        die(f"Payload must be {DATA_SIZE} bytes (got {len(payload)})")

    donor = SaveFile.load(args.from_save)
    key = donor.key

    if args.output.exists() and not args.force:
        die(f"Output file exists: {args.output} (use -f to overwrite)")

    result = SaveFile.build(payload, key)
    args.output.write_bytes(result)
    success(f"Created save file: {args.output}")


def cmd_scores(args: argparse.Namespace) -> None:
    """View or edit high scores."""
    save = SaveFile.load(args.input)
    payload = save.payload()

    if args.set:
        for spec in args.set:
            parts = spec.split(":")
            if len(parts) < 2:
                die(f"Invalid score spec: {spec} (use INDEX:SCORE or INDEX:SCORE:LEVEL:NAME)")
            idx = int(parts[0]) - 1
            if not 0 <= idx < len(payload.highscores):
                die(f"Invalid score index: {idx + 1} (1-{len(payload.highscores)})")

            payload.highscores[idx].score = int(parts[1])
            if len(parts) > 2:
                payload.highscores[idx].level = int(parts[2])
            if len(parts) > 3:
                payload.highscores[idx].name = parts[3]

        _write_modified(save, payload, args)
    else:
        # Display scores
        rows = []
        for i, hs in enumerate(payload.highscores):
            score_fmt = Style.fmt(f"{hs.score:,}", Style.YELLOW) if hs.score else Style.fmt("0", Style.DIM)
            rows.append([str(i + 1), hs.name or Style.fmt("(empty)", Style.DIM), score_fmt, str(hs.level)])
        print(format_table(["#", "Name", "Score", "Level"], rows))


def cmd_missions(args: argparse.Namespace) -> None:
    """View or edit mission completion status."""
    save = SaveFile.load(args.input)
    payload = save.payload()
    modified = False

    if args.unlock_all:
        for m in payload.missions:
            m.completed = True
        modified = True
        info("Unlocked all missions")

    if args.lock_all:
        for m in payload.missions:
            m.completed = False
        modified = True
        info("Locked all missions")

    if args.unlock:
        for idx in args.unlock:
            if 1 <= idx <= len(payload.missions):
                payload.missions[idx - 1].completed = True
                info(f"Unlocked mission {idx}: {payload.missions[idx - 1].name}")
            else:
                warn(f"Invalid mission index: {idx}")
        modified = True

    if args.lock:
        for idx in args.lock:
            if 1 <= idx <= len(payload.missions):
                payload.missions[idx - 1].completed = False
                info(f"Locked mission {idx}: {payload.missions[idx - 1].name}")
            else:
                warn(f"Invalid mission index: {idx}")
        modified = True

    if modified:
        _write_modified(save, payload, args)
    else:
        # Display missions
        rows = []
        for i, m in enumerate(payload.missions):
            status = Style.fmt("✓", Style.GREEN) if m.completed else Style.fmt("○", Style.DIM)
            rows.append([str(i + 1), status, m.name])
        print(format_table(["#", "Done", "Mission"], rows))


def cmd_players(args: argparse.Namespace) -> None:
    """View or edit player slots."""
    save = SaveFile.load(args.input)
    payload = save.payload()

    if args.set:
        for spec in args.set:
            parts = spec.split(":", 1)
            if len(parts) != 2:
                die(f"Invalid player spec: {spec} (use INDEX:NAME)")
            idx = int(parts[0]) - 1
            if not 0 <= idx < len(payload.players):
                die(f"Invalid player index: {idx + 1} (1-{len(payload.players)})")
            payload.players[idx].name = parts[1]
            payload.players[idx].unlocked = True

        _write_modified(save, payload, args)
    else:
        # Display players
        rows = []
        for i, p in enumerate(payload.players):
            status = Style.fmt("●", Style.GREEN) if p.unlocked else Style.fmt("○", Style.DIM)
            rows.append([str(i + 1), status, p.name or Style.fmt("(empty)", Style.DIM)])
        print(format_table(["#", "Unlocked", "Name"], rows))


def cmd_verify(args: argparse.Namespace) -> None:
    """Verify save file integrity via roundtrip."""
    save = SaveFile.load(args.input)
    original = args.input.read_bytes()

    # Decrypt, re-encrypt
    payload = save.decrypt()
    rebuilt = SaveFile.build(payload, save.key)

    if original == rebuilt:
        success(f"Roundtrip OK: {args.input.name}")
    else:
        die(f"Roundtrip failed: rebuilt data differs from original")


def _write_modified(save: SaveFile, payload: SavePayload, args: argparse.Namespace) -> None:
    """Write modified payload back to save file."""
    output = args.output or args.input
    if output.exists() and output != args.input and not args.force:
        die(f"Output file exists: {output} (use -f to overwrite)")

    new_payload = payload.serialize()
    result = SaveFile.build(new_payload, save.key)
    output.write_bytes(result)
    success(f"Saved to {output}")


# ─────────────────────────────────────────────────────────────────────────────
# CLI setup
# ─────────────────────────────────────────────────────────────────────────────
def main() -> None:
    parser = argparse.ArgumentParser(
        prog="save_editor",
        description="Airstrike 3D II Save File Editor",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples:
  %(prog)s info game.bin                      Show save file details
  %(prog)s missions game.bin --unlock-all     Unlock all missions
  %(prog)s scores game.bin --set 1:999999     Set top score to 999999
  %(prog)s decrypt game.bin -o decrypted.bin  Extract decrypted payload
""",
    )
    parser.add_argument("-V", "--version", action="version", version=f"%(prog)s {__version__}")

    # Common arguments
    common = argparse.ArgumentParser(add_help=False)
    common.add_argument("-f", "--force", action="store_true", help="Overwrite existing files")
    common.add_argument("-q", "--quiet", action="store_true", help="Suppress non-error output")

    subparsers = parser.add_subparsers(dest="command", metavar="COMMAND", required=True)

    # info
    p_info = subparsers.add_parser("info", parents=[common], help="Show save file information")
    p_info.add_argument("input", type=Path, metavar="FILE", help="Save file to inspect")
    p_info.set_defaults(func=cmd_info)

    # decrypt
    p_dec = subparsers.add_parser("decrypt", parents=[common], help="Decrypt save payload")
    p_dec.add_argument("input", type=Path, metavar="FILE", help="Save file to decrypt")
    p_dec.add_argument("-o", "--output", type=Path, help="Output file (default: stdout)")
    p_dec.add_argument("--format", choices=["hex", "raw", "text"], default="hex", help="Output format")
    p_dec.set_defaults(func=cmd_decrypt)

    # encrypt
    p_enc = subparsers.add_parser("encrypt", parents=[common], help="Encrypt payload into save file")
    p_enc.add_argument("payload", type=Path, metavar="PAYLOAD", help="Decrypted payload file")
    p_enc.add_argument("-o", "--output", type=Path, required=True, help="Output save file")
    p_enc.add_argument("--from-save", type=Path, required=True, metavar="DONOR", help="Donor save for XOR key")
    p_enc.set_defaults(func=cmd_encrypt)

    # scores
    p_scores = subparsers.add_parser("scores", parents=[common], help="View/edit high scores")
    p_scores.add_argument("input", type=Path, metavar="FILE", help="Save file")
    p_scores.add_argument(
        "--set",
        action="append",
        metavar="SPEC",
        help="Set score: INDEX:SCORE or INDEX:SCORE:LEVEL:NAME",
    )
    p_scores.add_argument("-o", "--output", type=Path, help="Output file (default: modify in-place)")
    p_scores.set_defaults(func=cmd_scores)

    # missions
    p_missions = subparsers.add_parser("missions", parents=[common], help="View/edit mission status")
    p_missions.add_argument("input", type=Path, metavar="FILE", help="Save file")
    p_missions.add_argument("--unlock-all", action="store_true", help="Unlock all missions")
    p_missions.add_argument("--lock-all", action="store_true", help="Lock all missions")
    p_missions.add_argument("--unlock", type=int, action="append", metavar="N", help="Unlock mission N")
    p_missions.add_argument("--lock", type=int, action="append", metavar="N", help="Lock mission N")
    p_missions.add_argument("-o", "--output", type=Path, help="Output file (default: modify in-place)")
    p_missions.set_defaults(func=cmd_missions)

    # players
    p_players = subparsers.add_parser("players", parents=[common], help="View/edit player slots")
    p_players.add_argument("input", type=Path, metavar="FILE", help="Save file")
    p_players.add_argument("--set", action="append", metavar="SPEC", help="Set player: INDEX:NAME")
    p_players.add_argument("-o", "--output", type=Path, help="Output file (default: modify in-place)")
    p_players.set_defaults(func=cmd_players)

    # verify
    p_verify = subparsers.add_parser("verify", parents=[common], help="Verify save file integrity")
    p_verify.add_argument("input", type=Path, metavar="FILE", help="Save file to verify")
    p_verify.set_defaults(func=cmd_verify)

    args = parser.parse_args()
    try:
        args.func(args)
    except FileNotFoundError as e:
        die(f"File not found: {e.filename}")
    except KeyboardInterrupt:
        print()
        sys.exit(130)
    except Exception as e:
        die(str(e))


if __name__ == "__main__":
    main()

