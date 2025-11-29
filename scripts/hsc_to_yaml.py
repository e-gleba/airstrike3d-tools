#!/usr/bin/env python3
"""
Convert .hsc (HMAP) level files to YAML format.

Based on level_explorer.py parsing logic and Ghidra analysis.
HMAP format contains:
- Header (28 bytes): magic, version, grid_size, terrain_scale, object counts
- Object type names (length-prefixed strings)
- Item type names (length-prefixed strings)
- Heightmap layers (grid_size × grid_size × 4 bytes per layer)
- Object placement data (0xFFFF delimited records)
"""

import sys
import struct
import argparse
import yaml
from pathlib import Path
from typing import Optional, Dict, List, Any
from dataclasses import dataclass, field, asdict

# Reuse parsing logic from level_explorer
sys.path.insert(0, str(Path(__file__).parent))
from level_explorer import parse_hmap, HMapLevel, LevelObject


def level_to_dict(level: HMapLevel) -> Dict[str, Any]:
    """Convert HMapLevel dataclass to dictionary for YAML serialization."""
    result = {
        'name': level.name,
        'header': {
            'magic': level.header.magic.decode('ascii', errors='replace'),
            'version': level.header.version,
            'grid_size': level.header.grid_size,
            'terrain_scale': level.header.terrain_scale,
            'object_count': level.header.object_count,
            'object_type_count': level.header.object_type_count,
            'item_type_count': level.header.item_type_count,
        },
        'object_types': level.object_types,
        'item_types': level.item_types,
        'heightmap_layers': len(level.heightmaps),
        'heightmaps': level.heightmaps,  # Full heightmap data
        'objects': []
    }
    
    # Convert objects to dictionaries
    for obj in level.objects:
        obj_dict = {
            'type_idx': obj.type_idx,
            'type_name': obj.type_name,
            'position': {
                'x': obj.x,
                'y': obj.y,
                'z': obj.z
            },
            'rotation': obj.rotation,
            'flags': obj.flags
        }
        if obj.script_path:
            obj_dict['script_path'] = obj.script_path
        result['objects'].append(obj_dict)
    
    return result


def convert_hsc_file(input_path: Path, output_path: Optional[Path] = None) -> bool:
    """Convert a .hsc file to YAML format."""
    try:
        data = input_path.read_bytes()
    except Exception as e:
        print(f"Error reading {input_path}: {e}", file=sys.stderr)
        return False
    
    # Parse HMAP file
    level = parse_hmap(data, input_path.stem)
    if not level:
        print(f"Error: Failed to parse HMAP file {input_path}", file=sys.stderr)
        return False
    
    # Convert to dictionary
    level_dict = level_to_dict(level)
    
    # Add metadata
    output = {
        'source_file': input_path.name,
        'file_size': len(data),
        'level': level_dict
    }
    
    # Format as YAML
    yaml_content = yaml.dump(output, default_flow_style=False, sort_keys=False, allow_unicode=True)
    
    if output_path:
        output_path.write_text(yaml_content)
        print(f"Converted {input_path.name} -> {output_path.name}")
        print(f"  Terrain: {level.header.grid_size}x{len(level.heightmaps) * level.header.grid_size} ({len(level.heightmaps)} layers)")
        print(f"  Objects: {len(level.objects)} placed, {len(level.object_types)} types")
    else:
        print(yaml_content)
    
    return True


def main():
    parser = argparse.ArgumentParser(description="Convert .hsc (HMAP) level files to YAML format")
    parser.add_argument(
        "input",
        type=Path,
        help="Input .hsc file or directory containing .hsc files"
    )
    parser.add_argument(
        "-o", "--output",
        type=Path,
        help="Output file or directory (default: same as input with .yaml extension)"
    )
    parser.add_argument(
        "-r", "--recursive",
        action="store_true",
        help="Process directories recursively"
    )
    
    args = parser.parse_args()
    
    input_path = args.input.resolve()
    
    if not input_path.exists():
        print(f"Error: {input_path} does not exist", file=sys.stderr)
        sys.exit(1)
    
    # Process single file
    if input_path.is_file():
        if not input_path.suffix.lower() == '.hsc':
            print(f"Warning: {input_path} doesn't have .hsc extension", file=sys.stderr)
        
        output_path = args.output
        if output_path is None:
            output_path = input_path.with_suffix('.yaml')
        elif output_path.is_dir():
            output_path = output_path / input_path.with_suffix('.yaml').name
        
        if not convert_hsc_file(input_path, output_path):
            sys.exit(1)
    
    # Process directory
    elif input_path.is_dir():
        pattern = "**/*.hsc" if args.recursive else "*.hsc"
        hsc_files = list(input_path.glob(pattern))
        
        if not hsc_files:
            print(f"No .hsc files found in {input_path}", file=sys.stderr)
            sys.exit(1)
        
        output_dir = args.output
        if output_dir is None:
            output_dir = input_path
        elif not output_dir.exists():
            output_dir.mkdir(parents=True, exist_ok=True)
        
        success = 0
        failed = 0
        
        for hsc_file in hsc_files:
            if output_dir.is_dir():
                rel_path = hsc_file.relative_to(input_path)
                out_file = output_dir / rel_path.with_suffix('.yaml')
                out_file.parent.mkdir(parents=True, exist_ok=True)
            else:
                out_file = output_dir
            
            if convert_hsc_file(hsc_file, out_file):
                success += 1
            else:
                failed += 1
        
        print(f"\nProcessed {success} files successfully, {failed} failed")
    
    else:
        print(f"Error: {input_path} is not a file or directory", file=sys.stderr)
        sys.exit(1)


if __name__ == "__main__":
    main()

