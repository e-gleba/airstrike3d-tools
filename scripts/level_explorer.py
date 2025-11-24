#!/usr/bin/env python3
"""
level_explorer - AirStrike 3D level explorer and analyzer

Explore and preview levels from AirStrike 3D game pak archives.
Uses paktool.py for archive extraction.

HMAP Format (reverse engineered):
  - Header: 28 bytes
  - Object type names: length-prefixed strings
  - Item type names: length-prefixed strings  
  - Heightmap layers: grid_size × grid_size × 4 bytes per layer
  - Object placement data: 32 bytes per object + optional scripts
"""
import argparse
import struct
import subprocess
import sys
import tempfile
from dataclasses import dataclass, field
from pathlib import Path
from typing import Generator, Optional

__version__ = "2.0.0"


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
    """HMAP file header structure (28 bytes)."""

    magic: bytes          # "HMAP"
    version: int          # Always 2
    grid_size: int        # Terrain grid dimension (e.g., 32)
    terrain_scale: int    # World scale factor (e.g., 256)
    object_count: int     # Number of placed objects
    object_type_count: int  # Number of unique object types
    item_type_count: int  # Number of item types (also layer count)


@dataclass
class LevelObject:
    """A placed object in the level."""

    type_idx: int
    type_name: str
    x: float
    y: float
    z: float
    rotation: float
    flags: int
    script_path: Optional[str] = None


@dataclass
class HMapLevel:
    """Fully parsed HMAP level data."""

    header: HMapHeader
    object_types: list[str] = field(default_factory=list)
    item_types: list[str] = field(default_factory=list)
    heightmaps: list[list[list[int]]] = field(default_factory=list)
    objects: list[LevelObject] = field(default_factory=list)
    raw_data: bytes = b""
    name: str = "Unknown"


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
        item_type_count,
    ) = struct.unpack_from("<6I", data, 4)

    return HMapHeader(
        magic=magic,
        version=version,
        grid_size=grid_size,
        terrain_scale=terrain_scale,
        object_count=object_count,
        object_type_count=object_type_count,
        item_type_count=item_type_count,
    )


def read_length_prefixed_string(data: bytes, offset: int) -> tuple[str, int]:
    """Read a length-prefixed string, return (string, new_offset)."""
    if offset >= len(data):
        return "", offset
    name_len = data[offset]
    offset += 1
    if offset + name_len > len(data):
        return "", offset
    name = data[offset:offset + name_len].decode("ascii", errors="replace").rstrip("\x00")
    return name, offset + name_len


def parse_hmap(data: bytes, name: str = "Unknown") -> Optional[HMapLevel]:
    """Parse complete HMAP level file."""
    header = parse_hmap_header(data)
    if not header:
        return None

    level = HMapLevel(header=header, raw_data=data, name=name)

    # Parse object type names
    offset = HMAP_HEADER_SIZE
    for _ in range(header.object_type_count):
        obj_name, offset = read_length_prefixed_string(data, offset)
        level.object_types.append(obj_name)

    # Parse item type names
    for _ in range(header.item_type_count):
        item_name, offset = read_length_prefixed_string(data, offset)
        level.item_types.append(item_name)

    # Find where object data starts (first 0xFFFF marker)
    first_ffff = len(data)
    for i in range(offset, len(data) - 1):
        if data[i:i + 2] == b"\xff\xff":
            first_ffff = i
            break

    # Calculate actual number of heightmap layers from available data
    layer_size = header.grid_size * header.grid_size * 4
    heightmap_bytes = first_ffff - offset
    actual_layers = heightmap_bytes // layer_size

    # Parse heightmap layers
    # Each layer is grid_size × grid_size × 4 bytes (uint32 heights 0-255)
    for layer in range(actual_layers):
        hmap = []
        for y in range(header.grid_size):
            row = []
            for x in range(header.grid_size):
                if offset + 4 <= len(data):
                    h = struct.unpack_from("<I", data, offset)[0]
                    h = min(255, h)  # Clamp to byte range
                else:
                    h = 128
                row.append(h)
                offset += 4
            hmap.append(row)
        level.heightmaps.append(hmap)

    # Parse object placement data
    # Objects are delimited by 0xFFFF markers
    # Format: ffff(2) + type_idx(2) + x_grid(2) + y_grid(2) + extra(5+) bytes
    ffff_positions = []
    for i in range(offset, len(data) - 1):
        if data[i:i + 2] == b"\xff\xff":
            ffff_positions.append(i)

    # Parse each object record
    scale = header.terrain_scale / header.grid_size  # World units per grid cell
    for i, pos in enumerate(ffff_positions):
        if pos + 8 > len(data):
            break

        type_idx = struct.unpack_from("<H", data, pos + 2)[0]
        x_grid = struct.unpack_from("<H", data, pos + 4)[0]
        y_grid = struct.unpack_from("<H", data, pos + 6)[0]

        if type_idx >= len(level.object_types):
            continue

        # Convert grid coordinates to world coordinates
        x_world = x_grid * scale
        y_world = y_grid * scale

        # Get extra bytes to check for rotation/flags
        next_pos = ffff_positions[i + 1] if i + 1 < len(ffff_positions) else len(data)
        extra = data[pos + 8:next_pos]

        # Extract flags/rotation from extra bytes
        flags = extra[0] if len(extra) > 0 else 0
        rotation = 0.0
        script_path = None

        # Check for script path in larger records
        if len(extra) > 20:
            script_marker = extra.find(b"scripts\\")
            if script_marker >= 0:
                script_end = extra.find(b"\x00", script_marker)
                if script_end > script_marker:
                    script_path = extra[script_marker:script_end].decode("ascii", errors="replace")

        type_name = level.object_types[type_idx]

        obj = LevelObject(
            type_idx=type_idx,
            type_name=type_name,
            x=x_world,
            y=y_world,
            z=0.0,  # Z is determined by terrain height
            rotation=rotation,
            flags=flags,
            script_path=script_path,
        )
        level.objects.append(obj)

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

    enemy_prefixes = ("helic", "tank", "btr", "mi_", "jeep_helic", "turret", "boss")
    vehicle_prefixes = ("jeep", "uaz", "gruzovik", "traktor", "civil_car", "tank", "cutter")
    building_prefixes = (
        "kolhoz", "dom", "angar", "budka", "tent", "cistern", "factory",
        "office", "hungar", "ruin", "zabor", "radar", "dock", "mayak"
    )
    nature_prefixes = (
        "tree", "grass", "bush", "kust", "kamni", "stone", "palm", "elka",
        "bereza", "sosna", "cactus", "penek", "brevna", "stog", "podsolnuh", "ice"
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

    if level_path.suffix.lower() == ".hsc":
        if not level_path.exists():
            Style.err(f"Level file not found: {level_path}")
            return 1
        data = level_path.read_bytes()
        level_name = level_path.stem
    elif level_path.suffix.lower() == ".apk":
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

    level = parse_hmap(data, level_name)
    if not level:
        Style.err("Failed to parse HMAP file")
        return 1

    h = level.header

    print(f"\n{Style.B}{Style.C}═══ Level: {level_name} ═══{Style.X}\n")

    actual_layers = len(level.heightmaps)
    full_height = h.grid_size * actual_layers

    print(f"{Style.B}Header:{Style.X}")
    print(f"  Magic:         {h.magic.decode()}")
    print(f"  Version:       {h.version}")
    print(f"  Terrain:       {h.grid_size}x{full_height} ({actual_layers} layers)")
    print(f"  World Scale:   {h.terrain_scale}")
    print(f"  File Size:     {len(data):,} bytes")

    print(f"\n{Style.B}Objects:{Style.X}")
    print(f"  Total Placed:  {h.object_count} (parsed: {len(level.objects)})")
    print(f"  Unique Types:  {h.object_type_count}")

    print(f"\n{Style.B}Items:{Style.X}")
    for item in level.item_types:
        print(f"  • {item}")

    if args.verbose and level.heightmaps:
        all_heights = [h for hmap in level.heightmaps for row in hmap for h in row]
        print(f"\n{Style.B}Heightmap:{Style.X}")
        print(f"  Range: {min(all_heights)} - {max(all_heights)}")
        print(f"  Unique values: {len(set(all_heights))}")

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

    if args.verbose and level.objects:
        print(f"\n{Style.B}Sample Objects (first 10):{Style.X}")
        for obj in level.objects[:10]:
            print(f"  {obj.type_name:<25} ({obj.x:.1f}, {obj.y:.1f}, {obj.z:.1f}) rot={obj.rotation:.1f}°")

    return 0


def cmd_extract(args: argparse.Namespace) -> int:
    """Extract level(s) from pak file."""
    pak_path: Path = args.input
    output_dir: Path = args.output or Path("levels")

    if not pak_path.exists():
        Style.err(f"Pak file not found: {pak_path}")
        return 1

    output_dir.mkdir(parents=True, exist_ok=True)

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

    if level_path.suffix.lower() == ".hsc":
        if not level_path.exists():
            Style.err(f"Level file not found: {level_path}")
            return 1
        hsc_file = level_path
    elif level_path.suffix.lower() == ".apk":
        if not args.name:
            Style.err("--name required when using pak file as input")
            return 1
        data = extract_level_from_pak(level_path, f"maps/{args.name}.hsc")
        if not data:
            Style.err(f"Could not extract level '{args.name}' from {level_path}")
            return 1
        with tempfile.NamedTemporaryFile(suffix=".hsc", delete=False) as f:
            f.write(data)
            hsc_file = Path(f.name)
    else:
        Style.err(f"Unknown file type: {level_path.suffix}")
        return 1

    script_dir = Path(__file__).parent
    viewer_script = script_dir / "level_viewer.py"

    if not viewer_script.exists():
        Style.err(f"Viewer script not found: {viewer_script}")
        return 1

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

    print(f"\n{Style.B}Most Common Object Types:{Style.X}")
    sorted_types = sorted(all_types.items(), key=lambda x: -x[1])
    for i, (name, count) in enumerate(sorted_types[:20], 1):
        bar_len = int(count / max(all_types.values()) * 30)
        bar = "█" * bar_len
        print(f"  {i:>2}. {name:<30} {count:>3} {Style.D}{bar}{Style.X}")

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
  %(prog)s info level1_tutor.hsc -v         # show level details
  %(prog)s extract pak1.apk -o levels/      # extract all levels
  %(prog)s stats pak1.apk                   # show statistics
  %(prog)s view level1_tutor.hsc            # launch 3D viewer
        """,
    )
    parser.add_argument("--version", action="version", version=f"%(prog)s {__version__}")

    subparsers = parser.add_subparsers(dest="command", metavar="COMMAND")

    p_list = subparsers.add_parser("list", help="List levels in pak file(s)")
    p_list.add_argument("input", type=Path, nargs="+", help="Pak file(s)")

    p_info = subparsers.add_parser("info", help="Show level details")
    p_info.add_argument("level", type=Path, help="Level file (.hsc) or pak file")
    p_info.add_argument("--name", "-n", help="Level name (when using pak file)")
    p_info.add_argument("-v", "--verbose", action="store_true", help="Show all details")

    p_extract = subparsers.add_parser("extract", help="Extract levels from pak")
    p_extract.add_argument("input", type=Path, help="Pak file")
    p_extract.add_argument("-o", "--output", type=Path, help="Output directory")
    p_extract.add_argument("-f", "--filter", help="Filter levels by name")
    p_extract.add_argument("-v", "--verbose", action="store_true")

    p_stats = subparsers.add_parser("stats", help="Show statistics across levels")
    p_stats.add_argument("input", type=Path, nargs="+", help="Pak file(s)")

    p_view = subparsers.add_parser("view", help="Launch 3D level viewer")
    p_view.add_argument("level", type=Path, help="Level file (.hsc) or pak file")
    p_view.add_argument("--name", "-n", help="Level name (when using pak file)")
    p_view.add_argument("-w", "--wireframe", action="store_true", help="Wireframe mode")

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
