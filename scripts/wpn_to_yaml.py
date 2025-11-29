#!/usr/bin/env python3
"""
Convert .wpn weapon definition files to YAML format.

Based on reverse engineering of the game's parse_weapon_definition function,
the .wpn files are text-based with the following format:
- Weapon name
- Opening brace {
- Properties (key-value pairs):
  - flash <object_name>
  - missile <object_name>
  - speed <float_value>
  - And other properties...
- Closing brace }

The files are encrypted in pack files and need to be decrypted first.
"""

import sys
import struct
import argparse
import yaml
from pathlib import Path
from typing import Optional, Dict, List, Any


def extract_pack_key(pak_path: Path) -> Optional[bytes]:
    """Extract the 1024-byte cipher key from a pack file."""
    try:
        with open(pak_path, 'rb') as f:
            magic = f.read(8)
            if magic != b'\x00\x00\x80\x3f\x99\x99\x00\x00':
                return None
            f.read(8)  # Skip table offset and count
            key = f.read(1024)
            if len(key) == 1024:
                return key
    except:
        pass
    return None


def decrypt_pack_data(data: bytes, key: bytes, offset: int = 0) -> bytes:
    """
    Decrypt pack file data using the game's decrypt_pack_data algorithm.
    
    Based on Ghidra analysis of 0x00405e50:
    - XOR each byte with key[(i + offset) % 1024]
    - The offset is typically 0 or file index
    """
    if len(key) != 1024:
        # Generate standard key if not provided
        key = bytes((i * 17 + 42) % 256 for i in range(1024))
    
    result = bytearray(len(data))
    for i in range(len(data)):
        key_idx = (i + offset) & 0x800003ff
        if key_idx < 0:
            key_idx = ((key_idx - 1) | 0xfffffc00) + 1
        key_idx = key_idx % 1024
        result[i] = data[i] ^ key[key_idx]
    return bytes(result)


def is_likely_text(data: bytes) -> bool:
    """Check if data looks like text."""
    if len(data) == 0:
        return False
    
    # Check for null bytes (binary files often have them)
    if b'\x00' in data[:min(1024, len(data))]:
        return False
    
    # Check if most bytes are printable ASCII (including Windows line endings \r\n)
    printable = sum(1 for b in data[:min(1024, len(data))] if 32 <= b <= 126 or b in (9, 10, 13))
    ratio = printable / min(1024, len(data))
    return ratio > 0.7  # Lower threshold to catch more text


def parse_wpn_text(content: str) -> List[Dict[str, Any]]:
    """Parse weapon definition text format."""
    weapons = []
    lines = content.split('\n')
    i = 0
    
    while i < len(lines):
        line = lines[i].strip()
        if not line or line.startswith('//'):
            i += 1
            continue
        
        # Weapon name - check if brace is on same line
        if '{' in line:
            # Format: "weapon_name {"
            parts = line.split('{', 1)
            weapon_name = parts[0].strip()
            weapon = {'name': weapon_name, 'properties': {}}
            i += 1
        else:
            # Format: "weapon_name" on one line, "{" on next
            weapon_name = line
            weapon = {'name': weapon_name, 'properties': {}}
            i += 1
            if i >= len(lines):
                break
            line = lines[i].strip()
            if line != '{':
                continue
            i += 1
        
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
                        weapon['properties'][key] = float(value)
                    else:
                        weapon['properties'][key] = int(value)
                except ValueError:
                    weapon['properties'][key] = value
            
            i += 1
        
        if weapon['properties']:
            weapons.append(weapon)
        
        i += 1
    
    return weapons


def format_weapon_yaml(weapons: List[Dict[str, Any]], source_file: str) -> str:
    """Format weapons as YAML."""
    output = {
        'source_file': source_file,
        'weapon_count': len(weapons),
        'weapons': {}
    }
    
    for weapon in weapons:
        output['weapons'][weapon['name']] = weapon['properties']
    
    return yaml.dump(output, default_flow_style=False, sort_keys=False, allow_unicode=True)


def convert_wpn_file(input_path: Path, output_path: Optional[Path] = None, pack_key: Optional[bytes] = None) -> bool:
    """Convert a .wpn file to YAML format."""
    try:
        data = input_path.read_bytes()
    except Exception as e:
        print(f"Error reading {input_path}: {e}", file=sys.stderr)
        return False
    
    # Try to decode as text first
    text_content = None
    
    # Check if it's already text
    if is_likely_text(data):
        try:
            text_content = data.decode('ascii', errors='replace').replace('\r\n', '\n').replace('\r', '\n')
        except:
            pass
    
    # Try simple XOR decryption
    if text_content is None:
        # Generate a test key
        key = bytes((i * 17 + 42) % 256 for i in range(1024))
        decrypted = decrypt_pack_data(data, key, 0)
        if is_likely_text(decrypted):
            try:
                text_content = decrypted.decode('ascii', errors='replace').replace('\r\n', '\n').replace('\r', '\n')
            except:
                pass
    
    # Try pack file style decryption (multiple offset values)
    if text_content is None:
        # Use provided key or generate standard key
        if pack_key:
            keys_to_try = [pack_key]
        else:
            keys_to_try = [bytes((i * 17 + 42) % 256 for i in range(1024))]
        
        for key in keys_to_try:
            # Try different offsets (file index in pack)
            for offset in [0, 1, 2, 3, 4, 5, 10, 20, 50, 100, 200]:
                decrypted = decrypt_pack_data(data, key, offset)
                if is_likely_text(decrypted):
                    try:
                        text_content = decrypted.decode('ascii', errors='replace').replace('\r\n', '\n').replace('\r', '\n')
                        break
                    except:
                        pass
            if text_content:
                break
    
    # If still not text, output error
    if text_content is None:
        print(f"Warning: {input_path} appears to be binary/encrypted and could not be decrypted", file=sys.stderr)
        print(f"File size: {len(data)} bytes", file=sys.stderr)
        print(f"First 64 bytes (hex):", file=sys.stderr)
        print(' '.join(f'{b:02x}' for b in data[:64]), file=sys.stderr)
        return False
    
    # Parse the text content
    try:
        weapons = parse_wpn_text(text_content)
    except Exception as e:
        print(f"Error parsing {input_path}: {e}", file=sys.stderr)
        return False
    
    # Format as YAML
    yaml_content = format_weapon_yaml(weapons, input_path.name)
    
    if output_path:
        output_path.write_text(yaml_content)
        print(f"Converted {input_path.name} -> {output_path.name} ({len(weapons)} weapons)")
    else:
        print(yaml_content)
    
    return True


def main():
    parser = argparse.ArgumentParser(
        description="Convert .wpn weapon definition files to YAML format"
    )
    parser.add_argument(
        "input",
        type=Path,
        help="Input .wpn file or directory containing .wpn files"
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
    parser.add_argument(
        "--pack-key",
        type=Path,
        help="Path to .apk pack file to extract decryption key from"
    )
    
    args = parser.parse_args()
    
    # Extract pack key if provided, or try to auto-detect
    pack_key = None
    if args.pack_key:
        pack_key = extract_pack_key(args.pack_key)
        if not pack_key:
            print(f"Warning: Could not extract key from {args.pack_key}", file=sys.stderr)
    else:
        # Try to auto-detect pack file in common locations
        for pak_path in [
            Path("2.71/data/pak1.apk"),
            Path("../2.71/data/pak1.apk"),
            Path("data/pak1.apk"),
        ]:
            if pak_path.exists():
                pack_key = extract_pack_key(pak_path)
                if pack_key:
                    break
    
    input_path = args.input.resolve()
    
    if not input_path.exists():
        print(f"Error: {input_path} does not exist", file=sys.stderr)
        sys.exit(1)
    
    # Process single file
    if input_path.is_file():
        if not input_path.suffix.lower() == '.wpn':
            print(f"Warning: {input_path} doesn't have .wpn extension", file=sys.stderr)
        
        output_path = args.output
        if output_path is None:
            output_path = input_path.with_suffix('.yaml')
        elif output_path.is_dir():
            output_path = output_path / input_path.with_suffix('.yaml').name
        
        convert_wpn_file(input_path, output_path, pack_key)
    
    # Process directory
    elif input_path.is_dir():
        pattern = "**/*.wpn" if args.recursive else "*.wpn"
        wpn_files = list(input_path.glob(pattern))
        
        if not wpn_files:
            print(f"No .wpn files found in {input_path}", file=sys.stderr)
            sys.exit(1)
        
        output_dir = args.output
        if output_dir is None:
            output_dir = input_path
        elif not output_dir.exists():
            output_dir.mkdir(parents=True, exist_ok=True)
        
        success = 0
        failed = 0
        
        for wpn_file in wpn_files:
            if output_dir.is_dir():
                rel_path = wpn_file.relative_to(input_path)
                out_file = output_dir / rel_path.with_suffix('.yaml')
                out_file.parent.mkdir(parents=True, exist_ok=True)
            else:
                out_file = output_dir
            
            if convert_wpn_file(wpn_file, out_file, pack_key):
                success += 1
            else:
                failed += 1
        
        print(f"\nProcessed {success} files successfully, {failed} failed")
    
    else:
        print(f"Error: {input_path} is not a file or directory", file=sys.stderr)
        sys.exit(1)


if __name__ == "__main__":
    main()

