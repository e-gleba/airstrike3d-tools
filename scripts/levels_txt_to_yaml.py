#!/usr/bin/env python3
"""
Convert levels.txt to YAML format.

Simple script that reuses decryption logic from wpn_to_yaml.py
and parses the level definition format.
"""

import sys
import argparse
import yaml
from pathlib import Path
from typing import Optional, Dict, List, Any

# Reuse decryption functions
sys.path.insert(0, str(Path(__file__).parent))
from wpn_to_yaml import extract_pack_key, decrypt_pack_data, is_likely_text


def parse_level_text(content: str) -> List[Dict[str, Any]]:
    """Parse level definition text format."""
    levels = []
    lines = content.split('\n')
    i = 0
    
    while i < len(lines):
        line = lines[i].strip()
        if not line or line.startswith('//'):
            i += 1
            continue
        
        # Level definition starts with opening brace
        if line == '{':
            level = {'properties': {}}
            i += 1
        else:
            i += 1
            continue
        
        # Parse properties
        while i < len(lines):
            line = lines[i].strip()
            if line == '}':
                break
            
            if not line or line.startswith('//'):
                i += 1
                continue
            
            # Parse key-value pair (tab or space separated)
            parts = line.split(None, 1)
            if len(parts) >= 1:
                key = parts[0]
                value = parts[1] if len(parts) > 1 else ""
                
                # Remove quotes if present
                if value.startswith('"') and value.endswith('"'):
                    value = value[1:-1]
                elif value.startswith("'") and value.endswith("'"):
                    value = value[1:-1]
                
                # Try to parse as number
                try:
                    if '.' in value:
                        level['properties'][key] = float(value)
                    else:
                        level['properties'][key] = int(value)
                except ValueError:
                    # Check if it's multiple values (like fog color + distances)
                    value_parts = value.split()
                    if len(value_parts) > 1:
                        try:
                            level['properties'][key] = [float(v) for v in value_parts]
                        except ValueError:
                            level['properties'][key] = value
                    else:
                        level['properties'][key] = value
            
            i += 1
        
        if level['properties']:
            levels.append(level)
        
        i += 1
    
    return levels


def format_levels_yaml(levels: List[Dict[str, Any]], source_file: str) -> str:
    """Format levels as YAML."""
    output = {
        'source_file': source_file,
        'level_count': len(levels),
        'levels': levels
    }
    
    return yaml.dump(output, default_flow_style=False, sort_keys=False, allow_unicode=True)


def convert_levels_txt(input_path: Path, output_path: Optional[Path] = None, pack_key: Optional[bytes] = None) -> bool:
    """Convert levels.txt to YAML format."""
    try:
        data = input_path.read_bytes()
    except Exception as e:
        print(f"Error reading {input_path}: {e}", file=sys.stderr)
        return False
    
    # Try to decode as text first
    text_content = None
    
    if is_likely_text(data):
        try:
            text_content = data.decode('ascii', errors='replace').replace('\r\n', '\n').replace('\r', '\n')
        except:
            pass
    
    # Try decryption
    if text_content is None:
        if pack_key:
            keys_to_try = [pack_key]
        else:
            keys_to_try = [bytes((i * 17 + 42) % 256 for i in range(1024))]
        
        for key in keys_to_try:
            for offset in [0, 1, 2, 3, 4, 5]:
                decrypted = decrypt_pack_data(data, key, offset)
                if is_likely_text(decrypted):
                    try:
                        text_content = decrypted.decode('ascii', errors='replace').replace('\r\n', '\n').replace('\r', '\n')
                        break
                    except:
                        pass
            if text_content:
                break
    
    if text_content is None:
        print(f"Warning: {input_path} appears to be binary/encrypted and could not be decrypted", file=sys.stderr)
        return False
    
    # Parse the text content
    try:
        levels = parse_level_text(text_content)
    except Exception as e:
        print(f"Error parsing {input_path}: {e}", file=sys.stderr)
        import traceback
        traceback.print_exc()
        return False
    
    # Format as YAML
    yaml_content = format_levels_yaml(levels, input_path.name)
    
    if output_path:
        output_path.write_text(yaml_content)
        print(f"Converted {input_path.name} -> {output_path.name} ({len(levels)} levels)")
    else:
        print(yaml_content)
    
    return True


def main():
    parser = argparse.ArgumentParser(description="Convert levels.txt to YAML format")
    parser.add_argument("input", type=Path, help="Input levels.txt file")
    parser.add_argument("-o", "--output", type=Path, help="Output YAML file (default: input with .yaml extension)")
    parser.add_argument("--pack-key", type=Path, help="Path to .apk pack file to extract decryption key from")
    
    args = parser.parse_args()
    
    # Extract pack key if provided, or try to auto-detect
    pack_key = None
    if args.pack_key:
        pack_key = extract_pack_key(args.pack_key)
        if not pack_key:
            print(f"Warning: Could not extract key from {args.pack_key}", file=sys.stderr)
    else:
        for pak_path in [Path("2.71/data/pak1.apk"), Path("../2.71/data/pak1.apk"), Path("data/pak1.apk")]:
            if pak_path.exists():
                pack_key = extract_pack_key(pak_path)
                if pack_key:
                    break
    
    input_path = args.input.resolve()
    if not input_path.exists():
        print(f"Error: {input_path} does not exist", file=sys.stderr)
        sys.exit(1)
    
    output_path = args.output
    if output_path is None:
        output_path = input_path.with_suffix('.yaml')
    
    if not convert_levels_txt(input_path, output_path, pack_key):
        sys.exit(1)


if __name__ == "__main__":
    main()

