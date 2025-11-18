#!/usr/bin/env python3
"""
paktool - .apk archive utility (Pack & Unpack)
"""
import sys
import struct
import argparse
from pathlib import Path
from typing import Generator, Tuple, List

# --- Configuration ---
MAGIC = b"\x00\x00\x80\x3f\x99\x99\x00\x00"  # Float 1.0 + padding
ENTRY_SIZE = 76
CIPHER_SZ = 1024
MAX_NAME = 64


class Style:
    R = "\033[31m"  # Red
    G = "\033[32m"  # Green
    Y = "\033[33m"  # Yellow
    B = "\033[1m"  # Bold
    D = "\033[2m"  # Dim
    X = "\033[0m"  # Reset

    @staticmethod
    def err(msg):
        print(f"{Style.B}{Style.R}error:{Style.X} {msg}", file=sys.stderr)

    @staticmethod
    def warn(msg):
        print(f"{Style.B}{Style.Y}warning:{Style.X} {msg}", file=sys.stderr)

    @staticmethod
    def ok(msg):
        print(f"{Style.B}{Style.G}success:{Style.X} {msg}", file=sys.stderr)


def gen_cipher() -> bytes:
    """Generates the standard 1KB XOR key used by this format."""
    return bytes((i * 17 + 42) % 256 for i in range(CIPHER_SZ))


def hex_line(data: bytes, addr: int) -> str:
    h = " ".join(f"{b:02x}" for b in data).ljust(48)
    a = "".join(chr(b) if 32 <= b <= 126 else "." for b in data)
    return f"{Style.D}{addr:08x}{Style.X}  {h}  {Style.D}|{a}|{Style.X}"


# --- Reading / Unpacking ---


class PakReader:
    def __init__(self, path: Path):
        self.path = path
        if not path.exists():
            Style.err(f"'{path}' not found")
            sys.exit(1)

    def _decrypt_entry(self, data: bytes, idx: int, cipher: bytes) -> bytes:
        # XOR based on position relative to the start of the file table
        offset_base = idx * ENTRY_SIZE
        out = bytearray(len(data))
        for i in range(len(data)):
            key_idx = (offset_base + i) % CIPHER_SZ
            out[i] = data[i] ^ cipher[key_idx]
        return bytes(out)

    def stream_entries(self) -> Generator[Tuple[str, int, int], None, None]:
        with open(self.path, "rb") as f:
            if f.read(8) != MAGIC:
                Style.err("invalid magic number")
                sys.exit(1)

            tbl_offset, count = struct.unpack("<II", f.read(8))
            cipher = f.read(CIPHER_SZ)

            f.seek(tbl_offset)
            for i in range(count):
                raw = f.read(ENTRY_SIZE)
                if len(raw) != ENTRY_SIZE:
                    break

                dec = self._decrypt_entry(raw, i, cipher)
                name_bytes, off, sz, _ = struct.unpack("<64sIII", dec)

                name = (
                    name_bytes.split(b"\0", 1)[0]
                    .decode("ascii", "ignore")
                    .replace("\\", "/")
                )
                if name:
                    yield name, off, sz

    def inspect(self):
        with open(self.path, "rb") as f:
            print(f"{Style.B}--- Header ---{Style.X}")
            raw_hdr = f.read(16)
            print(hex_line(raw_hdr, 0))
            tbl_off, count = struct.unpack("<II", raw_hdr[8:16])
            print(f"Table Offset: 0x{tbl_off:08x} | Files: {count}")

            print(f"\n{Style.B}--- Cipher (Start) ---{Style.X}")
            cipher = f.read(CIPHER_SZ)
            print(hex_line(cipher[:16], 16))

            print(f"\n{Style.B}--- First Entry ---{Style.X}")
            if tbl_off > self.path.stat().st_size:
                Style.err("Table offset out of bounds")
                return
            f.seek(tbl_off)
            raw = f.read(ENTRY_SIZE)
            print("Encrypted:")
            print(hex_line(raw[:16], tbl_off))

            dec = self._decrypt_entry(raw, 0, cipher)
            print("Decrypted:")
            print(hex_line(dec[:16], 0))
            name, off, sz, _ = struct.unpack("<64sIII", dec)
            print(f"Name: {name.split(b'\\0')[0].decode('ascii', 'ignore')}")


# --- Writing / Packing ---


class PakBuilder:
    def __init__(self):
        self.files: List[Tuple[str, Path]] = []

    def scan_directory(self, input_dir: Path):
        input_dir = input_dir.resolve()
        for path in input_dir.rglob("*"):
            if path.is_file():
                # Create relative path with Windows separators
                rel_path = path.relative_to(input_dir)
                name_str = str(rel_path).replace("/", "\\")

                if len(name_str.encode("ascii", "ignore")) >= MAX_NAME:
                    Style.warn(f"skipping '{name_str}' (name too long)")
                    continue

                self.files.append((name_str, path))

    def write(self, output_file: Path):
        cipher = gen_cipher()
        file_entries_meta = []  # Stores (name, offset, size)

        # Header size: 8 (magic) + 8 (offsets) + 1024 (cipher) = 1040
        current_offset = 1040

        try:
            with open(output_file, "wb") as f:
                # 1. Write Placeholder Header
                f.write(MAGIC)
                f.write(bytes(8))  # Placeholder for tbl_offset, count
                f.write(cipher)

                # 2. Write File Data
                print(f"Writing {len(self.files)} files...")
                for name, path in self.files:
                    size = path.stat().st_size
                    file_entries_meta.append((name, current_offset, size))

                    # Stream file content
                    with open(path, "rb") as src:
                        while chunk := src.read(65536):
                            f.write(chunk)

                    current_offset += size

                # 3. Write File Table
                table_start_offset = f.tell()

                for i, (name, off, sz) in enumerate(file_entries_meta):
                    # Construct entry
                    name_bytes = name.encode("ascii", "ignore").ljust(MAX_NAME, b"\0")
                    raw_struct = struct.pack("<64sIII", name_bytes, off, sz, 0)

                    # Encrypt entry
                    # XOR key index depends on entry index * entry size + byte position
                    base_idx = i * ENTRY_SIZE
                    enc_struct = bytearray(len(raw_struct))
                    for b_i, byte in enumerate(raw_struct):
                        enc_struct[b_i] = byte ^ cipher[(base_idx + b_i) % CIPHER_SZ]

                    f.write(enc_struct)

                # 4. Update Header
                f.seek(8)
                f.write(struct.pack("<II", table_start_offset, len(self.files)))

            Style.ok(f"Created '{output_file}' ({output_file.stat().st_size:,} bytes)")

        except OSError as e:
            Style.err(f"I/O Error: {e}")
            sys.exit(1)


# --- CLI Commands ---


def cmd_list(args):
    pak = PakReader(args.input)
    print(f"{Style.B}{'SIZE':>10}  {'OFFSET':>10}  {'NAME'}{Style.X}")
    print(f"{Style.D}{'-'*10}  {'-'*10}  {'-'*20}{Style.X}")
    for name, off, sz in pak.stream_entries():
        print(f"{sz:>10,}  0x{off:08x}  {name}")


def cmd_extract(args):
    pak = PakReader(args.input)
    out_dir = args.output or Path(args.input.stem)

    print(f"Extracting to '{out_dir}'...")
    with open(pak.path, "rb") as f:
        ok, fail = 0, 0
        for name, off, sz in pak.stream_entries():
            dest = out_dir / name
            dest.parent.mkdir(parents=True, exist_ok=True)
            try:
                f.seek(off)
                dest.write_bytes(f.read(sz))
                ok += 1
                if args.verbose:
                    print(f"extracted: {name}")
                elif ok % 100 == 0:
                    sys.stdout.write(f"\r{ok} files...")
                    sys.stdout.flush()
            except Exception as e:
                Style.err(f"failed {name}: {e}")
                fail += 1
    print(f"\n{Style.G}Done.{Style.X} {ok} extracted, {fail} failed.")


def cmd_pack(args):
    builder = PakBuilder()
    print(f"Scanning '{args.input}'...")
    builder.scan_directory(args.input)

    if not builder.files:
        Style.err("No files found to pack.")
        sys.exit(1)

    builder.write(args.output)


def cmd_inspect(args):
    PakReader(args.input).inspect()


def main():
    parser = argparse.ArgumentParser(description="pak file utility", add_help=False)
    base = argparse.ArgumentParser(add_help=False)
    base.add_argument("-v", "--verbose", action="store_true")

    subs = parser.add_subparsers(dest="cmd", metavar="COMMAND")

    # Unpack/View
    p_list = subs.add_parser("list", parents=[base], help="List contents")
    p_list.add_argument("input", type=Path)

    p_ext = subs.add_parser("extract", parents=[base], help="Extract archive")
    p_ext.add_argument("input", type=Path)
    p_ext.add_argument("-o", "--output", type=Path)

    p_insp = subs.add_parser("inspect", parents=[base], help="Debug structure")
    p_insp.add_argument("input", type=Path)

    # Pack
    p_pack = subs.add_parser("pack", parents=[base], help="Create archive")
    p_pack.add_argument("input", type=Path, help="Input directory")
    p_pack.add_argument("output", type=Path, help="Output .apk file")

    if len(sys.argv) == 1:
        parser.print_help()
        sys.exit(1)

    args = parser.parse_args()

    try:
        if args.cmd == "list":
            cmd_list(args)
        elif args.cmd == "extract":
            cmd_extract(args)
        elif args.cmd == "inspect":
            cmd_inspect(args)
        elif args.cmd == "pack":
            cmd_pack(args)
    except KeyboardInterrupt:
        sys.exit(130)
    except Exception as e:
        Style.err(str(e))
        if getattr(args, "verbose", False):
            raise
        sys.exit(1)


if __name__ == "__main__":
    main()
