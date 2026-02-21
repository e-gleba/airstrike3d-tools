#!/usr/bin/env python3
"""paktool — .apk archive utility (Pack & Unpack)

Requires: Python >= 3.14
Usage:
    paktool list   archive.apk
    paktool extract archive.apk [-o outdir] [-v]
    paktool pack    input_dir output.apk
    paktool inspect archive.apk
"""

from __future__ import annotations

import argparse
import struct
import sys
from dataclasses import dataclass, field
from pathlib import Path
from typing import Final, Generator

# ─── Constants ────────────────────────────────────────────────────────────────

MAGIC: Final[bytes] = b"\x00\x00\x80\x3f\x99\x99\x00\x00"  # float 1.0 + padding
HEADER_SIZE: Final[int] = 16  # 8 magic + 4 tbl_offset + 4 count
ENTRY_SIZE: Final[int] = 76
CIPHER_SIZE: Final[int] = 1024
MAX_NAME_LEN: Final[int] = 64
ENTRY_STRUCT: Final[str] = "<64sIII"  # name(64) + offset(4) + size(4) + reserved(4)
COPY_CHUNK: Final[int] = 1 << 16  # 64 KiB streaming chunk
DATA_START: Final[int] = 8 + 8 + CIPHER_SIZE  # magic + header fields + cipher = 1040

# ─── Type Aliases ─────────────────────────────────────────────────────────────

type EntryTuple = tuple[str, int, int]  # (name, offset, size)
type FilePair = tuple[str, Path]  # (archive_name, local_path)

# ─── Terminal Styling ─────────────────────────────────────────────────────────


class Style:
    """ANSI escape helpers for terminal output."""

    R: Final[str] = "\033[31m"
    G: Final[str] = "\033[32m"
    Y: Final[str] = "\033[33m"
    B: Final[str] = "\033[1m"
    D: Final[str] = "\033[2m"
    X: Final[str] = "\033[0m"

    @staticmethod
    def err(msg: str) -> None:
        print(f"{Style.B}{Style.R}error:{Style.X} {msg}", file=sys.stderr)

    @staticmethod
    def warn(msg: str) -> None:
        print(f"{Style.B}{Style.Y}warning:{Style.X} {msg}", file=sys.stderr)

    @staticmethod
    def ok(msg: str) -> None:
        print(f"{Style.B}{Style.G}success:{Style.X} {msg}", file=sys.stderr)


# ─── Cipher ───────────────────────────────────────────────────────────────────


def gen_cipher() -> bytes:
    """Generate the standard 1 KB XOR key used by this archive format."""
    return bytes((i * 17 + 42) % 256 for i in range(CIPHER_SIZE))


def xor_block(data: bytes | bytearray, cipher: bytes, key_offset: int = 0) -> bytes:
    """XOR `data` against `cipher`, starting at `key_offset` into the key.

    Uses a pre-built memoryview for zero-copy slicing and processes the
    entire block in one pass via a bytes-XOR comprehension.
    """
    cipher_len: int = len(cipher)
    return bytes(b ^ cipher[(key_offset + i) % cipher_len] for i, b in enumerate(data))


# ─── Hex Dump Helper ─────────────────────────────────────────────────────────


def hex_line(data: bytes, addr: int) -> str:
    """Format up to 16 bytes as a hex + ASCII dump line."""
    hex_part: str = " ".join(f"{b:02x}" for b in data).ljust(48)
    ascii_part: str = "".join(chr(b) if 32 <= b <= 126 else "." for b in data)
    return f"{Style.D}{addr:08x}{Style.X}  {hex_part}  {Style.D}|{ascii_part}|{Style.X}"


# ─── Reader / Unpacker ───────────────────────────────────────────────────────


@dataclass(slots=True)
class PakReader:
    """Reads and decrypts entries from an .apk archive."""

    path: Path

    def __post_init__(self) -> None:
        if not self.path.exists():
            Style.err(f"'{self.path}' not found")
            sys.exit(1)

    def _decrypt_entry(self, raw: bytes, index: int, cipher: bytes) -> bytes:
        """Decrypt a single file-table entry using positional XOR."""
        return xor_block(raw, cipher, key_offset=index * ENTRY_SIZE)

    def stream_entries(self) -> Generator[EntryTuple, None, None]:
        """Yield (name, offset, size) for every valid entry in the archive."""
        with open(self.path, "rb") as f:
            if f.read(8) != MAGIC:
                Style.err("invalid magic number")
                sys.exit(1)

            tbl_offset, count = struct.unpack("<II", f.read(8))
            cipher: bytes = f.read(CIPHER_SIZE)

            f.seek(tbl_offset)
            for i in range(count):
                raw: bytes = f.read(ENTRY_SIZE)
                if len(raw) != ENTRY_SIZE:
                    break

                dec: bytes = self._decrypt_entry(raw, i, cipher)
                name_bytes, off, sz, _ = struct.unpack(ENTRY_STRUCT, dec)

                name: str = (
                    name_bytes.split(b"\x00", 1)[0]
                    .decode("ascii", errors="ignore")
                    .replace("\\", "/")
                )
                if name:
                    yield name, off, sz

    def inspect(self) -> None:
        """Print a debug dump of the archive header, cipher, and first entry."""
        with open(self.path, "rb") as f:
            # Header
            print(f"{Style.B}--- Header ---{Style.X}")
            raw_hdr: bytes = f.read(HEADER_SIZE)
            print(hex_line(raw_hdr, 0))
            tbl_off, count = struct.unpack("<II", raw_hdr[8:16])
            print(f"Table Offset: 0x{tbl_off:08x} | Files: {count}")

            # Cipher preview
            print(f"\n{Style.B}--- Cipher (first 16 bytes) ---{Style.X}")
            cipher: bytes = f.read(CIPHER_SIZE)
            print(hex_line(cipher[:16], HEADER_SIZE))

            # First entry
            print(f"\n{Style.B}--- First Entry ---{Style.X}")
            file_size: int = self.path.stat().st_size
            if tbl_off > file_size:
                Style.err(
                    f"table offset 0x{tbl_off:08x} exceeds file size {file_size:,}"
                )
                return

            f.seek(tbl_off)
            raw: bytes = f.read(ENTRY_SIZE)
            if len(raw) < ENTRY_SIZE:
                Style.err("file table truncated")
                return

            print("Encrypted:")
            print(hex_line(raw[:16], tbl_off))

            dec: bytes = self._decrypt_entry(raw, 0, cipher)
            print("Decrypted:")
            print(hex_line(dec[:16], 0))

            name_bytes, off, sz, _ = struct.unpack(ENTRY_STRUCT, dec)
            name: str = name_bytes.split(b"\x00", 1)[0].decode("ascii", errors="ignore")
            print(f"Name: {name!r}  Offset: 0x{off:08x}  Size: {sz:,}")


# ─── Builder / Packer ────────────────────────────────────────────────────────


@dataclass(slots=True)
class PakBuilder:
    """Scans a directory and packs it into an .apk archive."""

    files: list[FilePair] = field(default_factory=list)

    def scan_directory(self, input_dir: Path) -> None:
        """Recursively collect files, converting paths to Windows separators."""
        resolved: Path = input_dir.resolve()
        for path in sorted(resolved.rglob("*")):
            if not path.is_file():
                continue
            rel: Path = path.relative_to(resolved)
            name_str: str = str(rel).replace("/", "\\")

            if len(name_str.encode("ascii", errors="ignore")) >= MAX_NAME_LEN:
                Style.warn(f"skipping '{name_str}' (name ≥ {MAX_NAME_LEN} bytes)")
                continue

            self.files.append((name_str, path))

    def write(self, output_file: Path) -> None:
        """Write the packed archive to `output_file`."""
        cipher: bytes = gen_cipher()
        entries_meta: list[EntryTuple] = []
        current_offset: int = DATA_START

        try:
            with open(output_file, "wb") as f:
                # 1. Placeholder header
                f.write(MAGIC)
                f.write(b"\x00" * 8)  # tbl_offset + count placeholders
                f.write(cipher)

                # 2. File data
                file_count: int = len(self.files)
                print(f"Writing {file_count} file{'s' if file_count != 1 else ''}...")

                for name, path in self.files:
                    size: int = path.stat().st_size
                    entries_meta.append((name, current_offset, size))

                    with open(path, "rb") as src:
                        while chunk := src.read(COPY_CHUNK):
                            f.write(chunk)

                    current_offset += size

                # 3. Encrypted file table
                table_offset: int = f.tell()

                for i, (name, off, sz) in enumerate(entries_meta):
                    name_bytes: bytes = name.encode("ascii", errors="ignore").ljust(
                        MAX_NAME_LEN, b"\x00"
                    )
                    raw_entry: bytes = struct.pack(ENTRY_STRUCT, name_bytes, off, sz, 0)
                    encrypted: bytes = xor_block(
                        raw_entry, cipher, key_offset=i * ENTRY_SIZE
                    )
                    f.write(encrypted)

                # 4. Patch header with real values
                f.seek(8)
                f.write(struct.pack("<II", table_offset, file_count))

            final_size: int = output_file.stat().st_size
            Style.ok(f"created '{output_file}' ({final_size:,} bytes)")

        except OSError as exc:
            Style.err(f"I/O error: {exc}")
            sys.exit(1)


# ─── CLI Commands ─────────────────────────────────────────────────────────────


def cmd_list(args: argparse.Namespace) -> None:
    """List archive contents with sizes and offsets."""
    pak: PakReader = PakReader(args.input)

    print(f"{Style.B}{'SIZE':>10}  {'OFFSET':>10}  {'NAME'}{Style.X}")
    print(f"{Style.D}{'-' * 10}  {'-' * 10}  {'-' * 40}{Style.X}")

    total_files: int = 0
    total_bytes: int = 0

    for name, off, sz in pak.stream_entries():
        print(f"{sz:>10,}  0x{off:08x}  {name}")
        total_files += 1
        total_bytes += sz

    print(f"{Style.D}{'-' * 10}  {'-' * 10}  {'-' * 40}{Style.X}")
    print(
        f"{Style.B}{total_bytes:>10,}  {'':>10}  "
        f"{total_files} file{'s' if total_files != 1 else ''}{Style.X}"
    )


def cmd_extract(args: argparse.Namespace) -> None:
    """Extract all files from the archive."""
    pak: PakReader = PakReader(args.input)
    out_dir: Path = args.output or Path(args.input.stem)

    print(f"Extracting to '{out_dir}'...")

    ok: int = 0
    fail: int = 0

    with open(pak.path, "rb") as f:
        for name, off, sz in pak.stream_entries():
            dest: Path = out_dir / name
            try:
                dest.parent.mkdir(parents=True, exist_ok=True)
                f.seek(off)

                # Stream extraction for large files
                with open(dest, "wb") as out:
                    remaining: int = sz
                    while remaining > 0:
                        chunk_size: int = min(COPY_CHUNK, remaining)
                        chunk: bytes = f.read(chunk_size)
                        if not chunk:
                            break
                        out.write(chunk)
                        remaining -= len(chunk)

                ok += 1
                if args.verbose:
                    print(f"  {Style.G}✓{Style.X} {name}")
                elif ok % 100 == 0:
                    sys.stdout.write(f"\r  {ok} files...")
                    sys.stdout.flush()

            except OSError as exc:
                Style.err(f"failed '{name}': {exc}")
                fail += 1

    if not args.verbose and ok >= 100:
        sys.stdout.write("\r")
        sys.stdout.flush()

    print(f"\n{Style.G}Done.{Style.X} {ok} extracted, {fail} failed.")


def cmd_pack(args: argparse.Namespace) -> None:
    """Pack a directory into an archive."""
    builder: PakBuilder = PakBuilder()

    print(f"Scanning '{args.input}'...")
    builder.scan_directory(args.input)

    if not builder.files:
        Style.err("no files found to pack")
        sys.exit(1)

    print(f"Found {len(builder.files)} file{'s' if len(builder.files) != 1 else ''}.")
    builder.write(args.output)


def cmd_inspect(args: argparse.Namespace) -> None:
    """Inspect archive internals."""
    PakReader(args.input).inspect()


# ─── CLI Entry Point ──────────────────────────────────────────────────────────

# Command dispatch table
_COMMANDS: Final[dict[str, tuple[str, callable, list[tuple[list[str], dict]]]]] = {
    "list": (
        "List archive contents",
        cmd_list,
        [
            (["input"], {"type": Path, "help": "Archive file (.apk)"}),
        ],
    ),
    "extract": (
        "Extract archive contents",
        cmd_extract,
        [
            (["input"], {"type": Path, "help": "Archive file (.apk)"}),
            (
                ["-o", "--output"],
                {"type": Path, "default": None, "help": "Output directory"},
            ),
        ],
    ),
    "inspect": (
        "Debug archive structure",
        cmd_inspect,
        [
            (["input"], {"type": Path, "help": "Archive file (.apk)"}),
        ],
    ),
    "pack": (
        "Create archive from directory",
        cmd_pack,
        [
            (["input"], {"type": Path, "help": "Input directory"}),
            (["output"], {"type": Path, "help": "Output .apk file"}),
        ],
    ),
}


def main() -> int:
    parser: argparse.ArgumentParser = argparse.ArgumentParser(
        description="paktool — .apk archive utility",
        add_help=True,
    )
    base: argparse.ArgumentParser = argparse.ArgumentParser(add_help=False)
    base.add_argument("-v", "--verbose", action="store_true", help="Verbose output")

    subs = parser.add_subparsers(dest="cmd", metavar="COMMAND")

    # Register all commands from the dispatch table
    handlers: dict[str, callable] = {}
    for cmd_name, (help_text, handler, arguments) in _COMMANDS.items():
        sub: argparse.ArgumentParser = subs.add_parser(
            cmd_name, parents=[base], help=help_text
        )
        for args_names, kwargs in arguments:
            sub.add_argument(*args_names, **kwargs)
        handlers[cmd_name] = handler

    if len(sys.argv) == 1:
        parser.print_help()
        return 1

    args: argparse.Namespace = parser.parse_args()

    if args.cmd not in handlers:
        parser.print_help()
        return 1

    try:
        handlers[args.cmd](args)
    except KeyboardInterrupt:
        return 130
    except Exception as exc:
        Style.err(str(exc))
        if getattr(args, "verbose", False):
            raise
        return 1

    return 0


if __name__ == "__main__":
    sys.exit(main())