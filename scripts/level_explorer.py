#!/usr/bin/env python3
"""
level_explorer - AirStrike 3D level explorer and analyzer

Explore and preview levels from AirStrike 3D game pak archives.
Uses paktool.py for archive extraction.
"""
import argparse
import struct
import subprocess
import sys
import tempfile
from dataclasses import dataclass, field
from pathlib import Path
from typing import Generator, Optional

__version__ = "1.0.0"


class Style:
    """Terminal styling utilities."""

    R = "\033[31m"  # Red
    G = "\033[32m"  # Green
    Y = "\033[33m"  # Yellow
    B = "\033[1m"   # Bold
    C = "\033[36m"  # Cyan
    M = "\033[35m"  # Magenta
    D = "\033[2m"   # Dim
    X = "\033[0m"   # Reset

    @staticmethod
    def err(msg: str) -> None:
        print(f"{Style.B}{Style.R}error:{Style.X} {msg}", file=sys.stderr)

    @staticmethod
    def warn(msg: str) -> None:
        print(f"{Style.B}{Style.Y}warning:{Style.X} {msg}", file=sys.stderr)

    @staticmethod
    def ok(msg: str) -> None:
        print(f"{Style.B}{Style.G}success:{Style.X} {msg}", file=sys.stderr)

    @staticmethod
    def info(msg: str) -> None:
        print(f"{Style.B}{Style.C}info:{Style.X} {msg}", file=sys.stderr)


# --- HMAP Format Constants ---
HMAP_MAGIC = b"HMAP"
HMAP_HEADER_SIZE = 28


@dataclass
class HMapHeader:
    """HMAP file header structure."""

    magic: bytes
    version: int
    grid_size: int        # Terrain grid dimension (e.g., 32 = 32x32)
    terrain_scale: int    # Usually 256 (scale factor)
    object_count: int     # Number of placed objects
    object_type_count: int  # Number of unique object types
    layer_count: int      # Number of terrain layers


@dataclass
class HMapLevel:
    """Parsed HMAP level data."""

    header: HMapHeader
    object_types: list[str] = field(default_factory=list)
    raw_data: bytes = b""

    @property
    def name(self) -> str:
        return "Unknown"


def parse_hmap_header(data: bytes) -> Optional[HMapHeader]:
    """Parse HMAP file header."""
    if len(data) < HMAP_HEADER_SIZE:
        return None

    magic = data[:4]
    if magic != HMAP_MAGIC:
        return None

    (
        version,
        grid_size,
        terrain_scale,
        object_count,
        object_type_count,
        layer_count,
    ) = struct.unpack_from("<6I", data, 4)

    return HMapHeader(
        magic=magic,
        version=version,
        grid_size=grid_size,
        terrain_scale=terrain_scale,
        object_count=object_count,
        object_type_count=object_type_count,
        layer_count=layer_count,
    )


def parse_hmap(data: bytes) -> Optional[HMapLevel]:
    """Parse complete HMAP level file."""
    header = parse_hmap_header(data)
    if not header:
        return None

    level = HMapLevel(header=header, raw_data=data)

    # Parse object type string table - keep parsing until we hit heightmap data
    # The heightmap starts when we see consistent uint32 values in 0-255 range
    offset = HMAP_HEADER_SIZE
    while offset < len(data) - 4:
        name_len = data[offset]
        # Check if this looks like a valid string length (1-64 chars)
        if name_len == 0 or name_len > 64:
            break
        # Check if next bytes look like ASCII string data
        if offset + 1 + name_len > len(data):
            break
        # Verify it's printable ASCII
        chunk = data[offset + 1 : offset + 1 + name_len]
        if not all(32 <= b < 127 or b == 0 for b in chunk):
            break
        offset += 1
        name = chunk.decode("ascii", errors="replace")
        level.object_types.append(name.rstrip("\x00"))
        offset += name_len

    # Update header with actual count
    level.header = HMapHeader(
        magic=header.magic,
        version=header.version,
        grid_size=header.grid_size,
        terrain_scale=header.terrain_scale,
        object_count=header.object_count,
        object_type_count=len(level.object_types),
        layer_count=header.layer_count,
    )

    return level


def run_paktool(pak_path: Path, command: str, *args: str) -> Optional[str]:
    """Run paktool.py with given command and arguments."""
    script_dir = Path(__file__).parent
    paktool = script_dir / "paktool.py"

    if not paktool.exists():
        Style.err(f"paktool.py not found at {paktool}")
        return None

    cmd = [sys.executable, str(paktool), command, str(pak_path), *args]
    try:
        result = subprocess.run(cmd, capture_output=True, text=True, check=True)
        return result.stdout
    except subprocess.CalledProcessError as e:
        Style.err(f"paktool failed: {e.stderr}")
        return None


def list_levels_in_pak(pak_path: Path) -> Generator[tuple[str, int, int], None, None]:
    """List all .hsc level files in a pak archive."""
    output = run_paktool(pak_path, "list")
    if not output:
        return

    for line in output.splitlines():
        # Parse paktool list output: "    SIZE  0xOFFSET  NAME"
        parts = line.split()
        if len(parts) >= 3 and parts[2].endswith(".hsc"):
            try:
                size = int(parts[0].replace(",", ""))
                offset = int(parts[1], 16)
                name = parts[2]
                yield name, offset, size
            except (ValueError, IndexError):
                continue


def extract_level_from_pak(pak_path: Path, level_name: str) -> Optional[bytes]:
    """Extract a single level file from pak archive."""
    with tempfile.TemporaryDirectory() as tmpdir:
        tmppath = Path(tmpdir)
        run_paktool(pak_path, "extract", "-o", str(tmppath))

        level_path = tmppath / level_name
        if level_path.exists():
            return level_path.read_bytes()

    return None


def format_object_types(types: list[str], columns: int = 3) -> str:
    """Format object types in columns."""
    if not types:
        return "  (none)"

    max_len = max(len(t) for t in types) + 2
    lines = []
    row = []

    for i, t in enumerate(types):
        row.append(t.ljust(max_len))
        if len(row) >= columns:
            lines.append("  " + "".join(row))
            row = []

    if row:
        lines.append("  " + "".join(row))

    return "\n".join(lines)


def categorize_objects(types: list[str]) -> dict[str, list[str]]:
    """Categorize object types by prefix/type."""
    categories: dict[str, list[str]] = {
        "enemies": [],
        "vehicles": [],
        "buildings": [],
        "nature": [],
        "items": [],
        "misc": [],
    }

    enemy_prefixes = ("helic", "tank", "btr", "mi_", "jeep_helic", "turret")
    vehicle_prefixes = ("jeep", "uaz", "gruzovik", "traktor", "civil_car", "tank")
    building_prefixes = (
        "kolhoz", "dom", "angar", "budka", "tent", "cistern", "factory",
        "office", "hungar", "ruin", "zabor"
    )
    nature_prefixes = (
        "tree", "grass", "bush", "kust", "kamni", "stone", "palm", "elka",
        "bereza", "sosna", "cactus", "penek", "brevna", "stog", "podsolnuh"
    )
    item_prefixes = ("item_", "ammo_", "box")

    for t in types:
        t_lower = t.lower()
        if any(t_lower.startswith(p) for p in enemy_prefixes):
            categories["enemies"].append(t)
        elif any(t_lower.startswith(p) for p in vehicle_prefixes):
            categories["vehicles"].append(t)
        elif any(t_lower.startswith(p) for p in building_prefixes):
            categories["buildings"].append(t)
        elif any(t_lower.startswith(p) for p in nature_prefixes):
            categories["nature"].append(t)
        elif any(t_lower.startswith(p) for p in item_prefixes):
            categories["items"].append(t)
        else:
            categories["misc"].append(t)

    return {k: v for k, v in categories.items() if v}


# --- CLI Commands ---


def cmd_list(args: argparse.Namespace) -> int:
    """List all levels in pak file(s)."""
    pak_paths = args.input if isinstance(args.input, list) else [args.input]

    print(f"{Style.B}{'PAK FILE':<30} {'LEVEL':<35} {'SIZE':>10}{Style.X}")
    print(f"{Style.D}{'-'*30} {'-'*35} {'-'*10}{Style.X}")

    total = 0
    for pak_path in pak_paths:
        if not pak_path.exists():
            Style.warn(f"'{pak_path}' not found, skipping")
            continue

        for name, offset, size in list_levels_in_pak(pak_path):
            pak_name = pak_path.name
            level_name = Path(name).stem
            print(f"{pak_name:<30} {level_name:<35} {size:>10,}")
            total += 1

    print(f"\n{Style.G}Total:{Style.X} {total} levels found")
    return 0


def cmd_info(args: argparse.Namespace) -> int:
    """Show detailed info about a level."""
    level_path: Path = args.level

    # Check if it's a direct .hsc file or needs extraction from pak
    if level_path.suffix.lower() == ".hsc":
        if not level_path.exists():
            Style.err(f"Level file not found: {level_path}")
            return 1
        data = level_path.read_bytes()
        level_name = level_path.stem
    elif level_path.suffix.lower() == ".apk":
        # It's a pak file, need level name
        if not args.name:
            Style.err("--name required when using pak file as input")
            return 1
        data = extract_level_from_pak(level_path, f"maps/{args.name}.hsc")
        if not data:
            Style.err(f"Could not extract level '{args.name}' from {level_path}")
            return 1
        level_name = args.name
    else:
        Style.err(f"Unknown file type: {level_path.suffix}")
        return 1

    level = parse_hmap(data)
    if not level:
        Style.err("Failed to parse HMAP file")
        return 1

    h = level.header

    print(f"\n{Style.B}{Style.C}═══ Level: {level_name} ═══{Style.X}\n")

    print(f"{Style.B}Header:{Style.X}")
    print(f"  Magic:         {h.magic.decode()}")
    print(f"  Version:       {h.version}")
    print(f"  Grid Size:     {h.grid_size}x{h.grid_size}")
    print(f"  Terrain Scale: {h.terrain_scale}")
    print(f"  Layers:        {h.layer_count}")
    print(f"  File Size:     {len(data):,} bytes")

    print(f"\n{Style.B}Objects:{Style.X}")
    print(f"  Total Placed:  {h.object_count}")
    print(f"  Unique Types:  {h.object_type_count}")

    if args.verbose:
        print(f"\n{Style.B}Object Types:{Style.X}")
        print(format_object_types(level.object_types, columns=3))

    # Show categorized summary
    categories = categorize_objects(level.object_types)
    print(f"\n{Style.B}Categories:{Style.X}")
    for cat, items in categories.items():
        color = {
            "enemies": Style.R,
            "vehicles": Style.Y,
            "buildings": Style.M,
            "nature": Style.G,
            "items": Style.C,
            "misc": Style.D,
        }.get(cat, "")
        print(f"  {color}{cat.capitalize():<12}{Style.X} {len(items):>3} types")

        if args.verbose:
            for item in items[:10]:
                print(f"    {Style.D}•{Style.X} {item}")
            if len(items) > 10:
                print(f"    {Style.D}... and {len(items) - 10} more{Style.X}")

    return 0


def cmd_extract(args: argparse.Namespace) -> int:
    """Extract level(s) from pak file."""
    pak_path: Path = args.input
    output_dir: Path = args.output or Path("levels")

    if not pak_path.exists():
        Style.err(f"Pak file not found: {pak_path}")
        return 1

    output_dir.mkdir(parents=True, exist_ok=True)

    # Get list of levels
    levels = list(list_levels_in_pak(pak_path))
    if args.filter:
        filter_lower = args.filter.lower()
        levels = [(n, o, s) for n, o, s in levels if filter_lower in n.lower()]

    if not levels:
        Style.warn("No matching levels found")
        return 1

    Style.info(f"Extracting {len(levels)} levels to {output_dir}")

    with tempfile.TemporaryDirectory() as tmpdir:
        tmppath = Path(tmpdir)
        run_paktool(pak_path, "extract", "-o", str(tmppath))

        extracted = 0
        for name, _, _ in levels:
            src = tmppath / name
            if src.exists():
                dst = output_dir / Path(name).name
                dst.write_bytes(src.read_bytes())
                extracted += 1
                if args.verbose:
                    print(f"  {Style.G}✓{Style.X} {name}")

    Style.ok(f"Extracted {extracted} levels")
    return 0


def cmd_view(args: argparse.Namespace) -> int:
    """Launch 3D viewer for a level."""
    level_path: Path = args.level

    # Check if it's a direct .hsc file or needs extraction from pak
    if level_path.suffix.lower() == ".hsc":
        if not level_path.exists():
            Style.err(f"Level file not found: {level_path}")
            return 1
        hsc_file = level_path
    elif level_path.suffix.lower() == ".apk":
        # Extract from pak to temp file
        if not args.name:
            Style.err("--name required when using pak file as input")
            return 1
        data = extract_level_from_pak(level_path, f"maps/{args.name}.hsc")
        if not data:
            Style.err(f"Could not extract level '{args.name}' from {level_path}")
            return 1
        # Write to temp file
        import tempfile
        with tempfile.NamedTemporaryFile(suffix=".hsc", delete=False) as f:
            f.write(data)
            hsc_file = Path(f.name)
    else:
        Style.err(f"Unknown file type: {level_path.suffix}")
        return 1

    # Launch viewer
    script_dir = Path(__file__).parent
    viewer_script = script_dir / "level_viewer.py"

    if not viewer_script.exists():
        Style.err(f"Viewer script not found: {viewer_script}")
        return 1

    import subprocess
    cmd = [sys.executable, str(viewer_script), str(hsc_file)]
    if args.wireframe:
        cmd.append("--wireframe")

    Style.info(f"Launching 3D viewer for {hsc_file.name}...")
    try:
        subprocess.run(cmd)
    except KeyboardInterrupt:
        pass

    return 0


def cmd_stats(args: argparse.Namespace) -> int:
    """Show statistics across all levels in pak file(s)."""
    pak_paths = args.input if isinstance(args.input, list) else [args.input]

    all_types: dict[str, int] = {}
    level_count = 0
    total_objects = 0

    for pak_path in pak_paths:
        if not pak_path.exists():
            continue

        with tempfile.TemporaryDirectory() as tmpdir:
            tmppath = Path(tmpdir)
            run_paktool(pak_path, "extract", "-o", str(tmppath))

            for name, _, _ in list_levels_in_pak(pak_path):
                level_file = tmppath / name
                if not level_file.exists():
                    continue

                data = level_file.read_bytes()
                level = parse_hmap(data)
                if not level:
                    continue

                level_count += 1
                total_objects += level.header.object_count

                for obj_type in level.object_types:
                    all_types[obj_type] = all_types.get(obj_type, 0) + 1

    print(f"\n{Style.B}{Style.C}═══ Level Statistics ═══{Style.X}\n")
    print(f"  Levels Analyzed:    {level_count}")
    print(f"  Total Objects:      {total_objects:,}")
    print(f"  Unique Object Types: {len(all_types)}")

    # Top 20 most common object types
    print(f"\n{Style.B}Most Common Object Types:{Style.X}")
    sorted_types = sorted(all_types.items(), key=lambda x: -x[1])
    for i, (name, count) in enumerate(sorted_types[:20], 1):
        bar_len = int(count / max(all_types.values()) * 30)
        bar = "█" * bar_len
        print(f"  {i:>2}. {name:<30} {count:>3} {Style.D}{bar}{Style.X}")

    # Category breakdown
    categories = categorize_objects(list(all_types.keys()))
    print(f"\n{Style.B}Category Distribution:{Style.X}")
    for cat, items in sorted(categories.items(), key=lambda x: -len(x[1])):
        pct = len(items) / len(all_types) * 100
        print(f"  {cat.capitalize():<12} {len(items):>4} ({pct:>5.1f}%)")

    return 0


def main() -> int:
    """Main entry point."""
    parser = argparse.ArgumentParser(
        description="AirStrike 3D level explorer",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
examples:
  %(prog)s list pak1.apk                    # list all levels
  %(prog)s list pak0.apk pak1.apk           # list from multiple paks
  %(prog)s info level1_tutor.hsc            # show level details
  %(prog)s info pak1.apk --name level1_tutor  # info from pak
  %(prog)s extract pak1.apk -o levels/      # extract all levels
  %(prog)s stats pak1.apk                   # show statistics
  %(prog)s view level1_tutor.hsc            # launch 3D viewer
  %(prog)s view pak1.apk -n level_boss1     # view level from pak
        """,
    )
    parser.add_argument(
        "--version", action="version", version=f"%(prog)s {__version__}"
    )

    subparsers = parser.add_subparsers(dest="command", metavar="COMMAND")

    # List command
    p_list = subparsers.add_parser("list", help="List levels in pak file(s)")
    p_list.add_argument("input", type=Path, nargs="+", help="Pak file(s)")

    # Info command
    p_info = subparsers.add_parser("info", help="Show level details")
    p_info.add_argument("level", type=Path, help="Level file (.hsc) or pak file")
    p_info.add_argument("--name", "-n", help="Level name (when using pak file)")
    p_info.add_argument("-v", "--verbose", action="store_true", help="Show all details")

    # Extract command
    p_extract = subparsers.add_parser("extract", help="Extract levels from pak")
    p_extract.add_argument("input", type=Path, help="Pak file")
    p_extract.add_argument("-o", "--output", type=Path, help="Output directory")
    p_extract.add_argument("-f", "--filter", help="Filter levels by name")
    p_extract.add_argument("-v", "--verbose", action="store_true")

    # Stats command
    p_stats = subparsers.add_parser("stats", help="Show statistics across levels")
    p_stats.add_argument("input", type=Path, nargs="+", help="Pak file(s)")

    # View command (3D viewer)
    p_view = subparsers.add_parser("view", help="Launch 3D level viewer")
    p_view.add_argument("level", type=Path, help="Level file (.hsc) or pak file")
    p_view.add_argument("--name", "-n", help="Level name (when using pak file)")
    p_view.add_argument("-w", "--wireframe", action="store_true", help="Start in wireframe mode")

    if len(sys.argv) == 1:
        parser.print_help()
        return 1

    args = parser.parse_args()

    try:
        if args.command == "list":
            return cmd_list(args)
        elif args.command == "info":
            return cmd_info(args)
        elif args.command == "extract":
            return cmd_extract(args)
        elif args.command == "stats":
            return cmd_stats(args)
        elif args.command == "view":
            return cmd_view(args)
        else:
            parser.print_help()
            return 1
    except KeyboardInterrupt:
        return 130
    except Exception as e:
        Style.err(str(e))
        return 1


if __name__ == "__main__":
    sys.exit(main())

