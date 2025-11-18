#!/usr/bin/env python3
"""
divo save file crypto utility (modernized)
"""
import sys
import struct
import argparse
import shutil
from pathlib import Path
from itertools import cycle
from typing import Optional, Literal

__version__ = "2.0.0"

# Configuration
HDR_SIZE = 0x108
DATA_SIZE = 0x574
KEY_SIZE = 256
TOTAL_SIZE = HDR_SIZE + DATA_SIZE


class Style:
    RESET = "\033[0m"
    BOLD = "\033[1m"
    RED = "\033[31m"
    GREEN = "\033[32m"
    YELLOW = "\033[33m"
    CYAN = "\033[36m"
    DIM = "\033[2m"

    @staticmethod
    def fmt(msg: str, color: str, bold: bool = False) -> str:
        if not sys.stdout.isatty():
            return msg
        pre = (Style.BOLD if bold else "") + color
        return f"{pre}{msg}{Style.RESET}"


def die(msg: str):
    sys.exit(f"{Style.fmt('error:', Style.RED, True)} {msg}")


def log(msg: str, type: str = "info", quiet: bool = False):
    if quiet:
        return
    pmap = {
        "info": ("", ""),
        "warn": (f"{Style.fmt('warning:', Style.YELLOW, True)} ", ""),
        "ok": (f"{Style.fmt('success:', Style.GREEN, True)} ", ""),
        "note": (f"{Style.fmt('note:', Style.CYAN, True)} ", ""),
    }
    prefix, suffix = pmap.get(type, ("", ""))
    print(f"{prefix}{msg}{suffix}", file=sys.stderr)


# --- Core Logic ---


def crc16(data: bytes) -> int:
    """CRC-16/CCITT-FALSE (0xFFFF init, 0x1021 poly)"""
    crc = 0xFFFF
    for b in data:
        crc ^= (b << 8) & 0xFFFF
        for _ in range(8):
            crc = ((crc << 1) ^ 0x1021) if (crc & 0x8000) else (crc << 1)
            crc &= 0xFFFF
    return crc


def xor_cipher(data: bytes, key: bytes) -> bytes:
    if not key:
        return data
    return bytes(b ^ k for b, k in zip(data, cycle(key)))


class SaveFile:
    """Handles parsing, validation, and rebuilding of save files."""

    def __init__(self, path: Path):
        self.path = path
        try:
            raw = path.read_bytes()
        except FileNotFoundError:
            die(f"file '{path}' not found")

        if len(raw) != TOTAL_SIZE:
            log(f"size mismatch: {len(raw)} bytes (expected {TOTAL_SIZE})", "warn")

        self.magic = raw[:4]  # float 1.0
        self.key = raw[4:260]
        self.stored_crc = struct.unpack("<I", raw[260:264])[0]
        self.encrypted_payload = raw[264 : 264 + DATA_SIZE]
        self.decrypted_payload: Optional[bytes] = None

    def decrypt(self) -> bytes:
        if not self.decrypted_payload:
            self.decrypted_payload = xor_cipher(self.encrypted_payload, self.key)
        return self.decrypted_payload

    @staticmethod
    def build(payload: bytes, key: bytes, output: Path) -> None:
        if len(payload) != DATA_SIZE:
            die(f"payload must be {DATA_SIZE} bytes (got {len(payload)})")
        if len(key) != KEY_SIZE:
            die(f"key must be {KEY_SIZE} bytes (got {len(key)})")

        encrypted = xor_cipher(payload, key)
        crc = crc16(encrypted)

        # Header: float(1.0) + Key(256) + CRC(4)
        header = struct.pack("<f", 1.0) + key + struct.pack("<I", crc)

        output.write_bytes(header + encrypted)


# --- Formatting Tools ---


def hexdump(data: bytes, offset_start=0) -> str:
    out = []
    for i in range(0, len(data), 16):
        chunk = data[i : i + 16]
        hex_str = " ".join(f"{b:02x}" for b in chunk).ljust(47)
        asc_str = "".join(chr(b) if 32 <= b <= 126 else "." for b in chunk)
        out.append(
            f"{Style.DIM}{offset_start+i:04x}{Style.RESET}  {hex_str}  {Style.CYAN}|{asc_str}|{Style.RESET}"
        )
    return "\n".join(out)


def analyze_payload(data: bytes) -> str:
    nulls = data.count(0) / len(data)
    printable = sum(1 for b in data if 32 <= b <= 126) / len(data)

    status = Style.fmt("CORRUPT?", Style.RED)
    if nulls < 0.9 and printable > 0.1:
        status = Style.fmt("OK", Style.GREEN)

    return f"validity: {status} (printable: {printable:.1%}, nulls: {nulls:.1%})"


# --- Commands ---


def cmd_info(args):
    save = SaveFile(args.input)
    dec = save.decrypt()

    print(f"{Style.BOLD}File:{Style.RESET}   {args.input.name}")
    print(
        f"{Style.BOLD}Size:{Style.RESET}   {len(save.encrypted_payload) + HDR_SIZE} bytes"
    )
    print(
        f"{Style.BOLD}Key:{Style.RESET}    {save.key[:8].hex()}... ({len(set(save.key))} unique bytes)"
    )
    print(f"{Style.BOLD}CRC:{Style.RESET}    0x{save.stored_crc:04x}")
    print(f"{Style.BOLD}Data:{Style.RESET}   {analyze_payload(dec)}")

    if args.verbose:
        print(f"\n{Style.BOLD}Key Dump:{Style.RESET}")
        print(hexdump(save.key))
        print(f"\n{Style.BOLD}Payload Head:{Style.RESET}")
        print(hexdump(dec[:64], 0x108))


def cmd_decrypt(args):
    save = SaveFile(args.input)
    data = save.decrypt()

    if args.output:
        if args.output.exists() and not args.force:
            die(f"output '{args.output}' exists (use -f to force)")
        args.output.write_bytes(data)
        log(f"wrote {len(data)} bytes to '{args.output}'", "ok", args.quiet)
    else:
        # Stdout modes
        if args.format == "raw":
            sys.stdout.buffer.write(data)
        elif args.format == "text":
            print(data.decode("utf-8", errors="replace"))
        else:
            print(hexdump(data, HDR_SIZE))


def cmd_encrypt(args):
    # Resolve Key
    key = None
    if args.key:
        key = args.key.read_bytes()[:KEY_SIZE]
    elif args.from_save:
        key = SaveFile(args.from_save).key

    if not key or len(key) != KEY_SIZE:
        die("valid 256-byte key source required (--key or --from-save)")

    # Resolve Payload
    payload = args.payload.read_bytes()

    if args.output.exists() and not args.force:
        die(f"output '{args.output}' exists (use -f to force)")

    SaveFile.build(payload, key, args.output)
    log(f"built save file '{args.output}'", "ok", args.quiet)


def cmd_roundtrip(args):
    src = SaveFile(args.input)
    original_data = src.path.read_bytes()

    # Reconstruct
    payload = src.decrypt()
    re_encrypted = xor_cipher(payload, src.key)
    crc = crc16(re_encrypted)
    new_header = struct.pack("<f", 1.0) + src.key + struct.pack("<I", crc)
    rebuilt_data = new_header + re_encrypted

    if original_data == rebuilt_data:
        log(f"verification passed: {src.path.name} is reproducible", "ok", args.quiet)
    else:
        die(f"verification failed: rebuilt file differs from original")


# --- CLI Setup ---


def main():
    p = argparse.ArgumentParser(description="divo save file tool", add_help=False)
    p.add_argument("--version", action="version", version=f"v{__version__}")

    # Global args
    base = argparse.ArgumentParser(add_help=False)
    base.add_argument("-v", "--verbose", action="store_true")
    base.add_argument("-q", "--quiet", action="store_true")
    base.add_argument("-f", "--force", action="store_true")

    subs = p.add_subparsers(dest="cmd", metavar="COMMAND")

    # Info
    subs.add_parser("info", parents=[base], help="Show save details").add_argument(
        "input", type=Path
    )

    # Decrypt (merged dump+decrypt)
    dec = subs.add_parser("decrypt", parents=[base], help="Decrypt payload")
    dec.add_argument("input", type=Path)
    dec.add_argument(
        "-o", "--output", type=Path, help="Write to file instead of stdout"
    )
    dec.add_argument(
        "--format", choices=["hex", "text", "raw"], default="hex", help="Stdout format"
    )

    # Extract Key
    key = subs.add_parser("key", parents=[base], help="Extract key")
    key.add_argument("input", type=Path)
    key.add_argument("-o", "--output", type=Path)

    # Encrypt
    enc = subs.add_parser("encrypt", parents=[base], help="Pack payload into save")
    enc.add_argument("payload", type=Path)
    enc.add_argument("-o", "--output", type=Path, required=True)
    g = enc.add_mutually_exclusive_group(required=True)
    g.add_argument("--key", type=Path, help="Raw key file")
    g.add_argument("--from-save", type=Path, help="Extract key from another save")

    # Roundtrip
    subs.add_parser("roundtrip", parents=[base], help="Verify integrity").add_argument(
        "input", type=Path
    )

    args = p.parse_args()

    if not args.cmd:
        p.print_help()
        sys.exit(1)

    # Dispatch
    try:
        if args.cmd == "info":
            cmd_info(args)
        elif args.cmd == "decrypt":
            cmd_decrypt(args)
        elif args.cmd == "encrypt":
            cmd_encrypt(args)
        elif args.cmd == "roundtrip":
            cmd_roundtrip(args)
        elif args.cmd == "key":
            # Inline key logic as it's simple
            k = SaveFile(args.input).key
            if args.output:
                args.output.write_bytes(k)
                log(f"key saved to {args.output}", "ok", args.quiet)
            else:
                sys.stdout.buffer.write(k)
    except KeyboardInterrupt:
        print()
    except Exception as e:
        if args.verbose:
            raise
        die(str(e))


if __name__ == "__main__":
    main()
