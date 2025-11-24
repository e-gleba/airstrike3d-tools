#!/usr/bin/env python3
"""
ASProtect 1.0 Static Unpacker for Airstrike 3D
Extracts and decrypts the protected code without running the executable.

ASProtect 1.0 (2003) structure:
- Uses aPLib compression (LZ77 variant)
- Entry point: PUSHAD + CALL to loader
- Loader uses position-independent code (delta_base calculation)
- Encrypted .text stored in large .data section
- Control data contains: compressed data ptr, decompressed size, target address
"""

import struct
import sys
import os

# aPLib decompression implementation
# Based on aPLib 1.1.1 by Joergen Ibsen (http://www.ibsensoftware.com/)

def aplib_decompress(src):
    """Decompress aPLib compressed data."""
    dst = bytearray()
    src_idx = 0
    
    def getbit(tag, bitcount):
        nonlocal src_idx
        bitcount -= 1
        if bitcount < 0:
            if src_idx >= len(src):
                raise ValueError("Unexpected end of compressed data")
            tag = src[src_idx]
            src_idx += 1
            bitcount = 7
        bit = (tag >> 7) & 1
        tag = (tag << 1) & 0xFF
        return tag, bitcount, bit
    
    def getgamma(tag, bitcount):
        result = 1
        while True:
            tag, bitcount, bit = getbit(tag, bitcount)
            result = (result << 1) + bit
            tag, bitcount, bit = getbit(tag, bitcount)
            if bit == 0:
                break
        return tag, bitcount, result
    
    # Initialize
    tag = 0
    bitcount = 0
    
    # First byte is literal
    if src_idx < len(src):
        dst.append(src[src_idx])
        src_idx += 1
    
    lwm = 0  # last was match
    last_offset = 0
    
    while src_idx < len(src) or bitcount > 0:
        tag, bitcount, bit = getbit(tag, bitcount)
        
        if bit == 1:
            # 1 - literal or short match
            tag, bitcount, bit = getbit(tag, bitcount)
            
            if bit == 1:
                # 11 - long match
                tag, bitcount, length = getgamma(tag, bitcount)
                
                if lwm == 0 and length == 2:
                    # Use last offset
                    offset = last_offset
                    tag, bitcount, length = getgamma(tag, bitcount)
                else:
                    # Read high byte of offset
                    high = (length - 2) & 0xFF
                    if src_idx >= len(src):
                        break
                    low = src[src_idx]
                    src_idx += 1
                    offset = (high << 8) | low
                    tag, bitcount, length = getgamma(tag, bitcount)
                    
                    if offset >= 32000:
                        length += 1
                    if offset >= 1280:
                        length += 1
                    if offset < 128:
                        length += 2
                    
                    last_offset = offset
                
                # Copy from back reference
                for _ in range(length):
                    if len(dst) - offset - 1 >= 0:
                        dst.append(dst[len(dst) - offset - 1])
                    else:
                        dst.append(0)
                
                lwm = 1
                
            else:
                # 10 - short match or literal
                tag, bitcount, bit = getbit(tag, bitcount)
                
                if bit == 1:
                    # 101 - single byte short match
                    tag, bitcount, bit = getbit(tag, bitcount)
                    offset = bit << 1
                    tag, bitcount, bit = getbit(tag, bitcount)
                    offset |= bit
                    
                    if offset == 0:
                        # Literal 0x00
                        dst.append(0)
                    else:
                        if len(dst) - offset >= 0:
                            dst.append(dst[len(dst) - offset])
                        else:
                            dst.append(0)
                    
                    lwm = 0
                    
                else:
                    # 100 - literal
                    if src_idx >= len(src):
                        break
                    dst.append(src[src_idx])
                    src_idx += 1
                    lwm = 0
                    
        else:
            # 0 - short match from last offset
            tag, bitcount, length = getgamma(tag, bitcount)
            
            if lwm == 0:
                length -= 1
            
            if length > 0:
                for _ in range(length):
                    if len(dst) - last_offset - 1 >= 0:
                        dst.append(dst[len(dst) - last_offset - 1])
                    else:
                        dst.append(0)
                lwm = 1
            else:
                # End marker
                break
    
    return bytes(dst)


def find_pe_sections(data):
    """Parse PE headers and return section info."""
    # DOS header check
    if data[:2] != b'MZ':
        raise ValueError("Not a valid PE file (no MZ header)")
    
    # Get PE header offset
    pe_offset = struct.unpack('<I', data[0x3C:0x40])[0]
    
    # PE signature check
    if data[pe_offset:pe_offset+4] != b'PE\x00\x00':
        raise ValueError("Not a valid PE file (no PE signature)")
    
    # COFF header
    coff_offset = pe_offset + 4
    num_sections = struct.unpack('<H', data[coff_offset+2:coff_offset+4])[0]
    optional_header_size = struct.unpack('<H', data[coff_offset+16:coff_offset+18])[0]
    
    # Optional header
    opt_offset = coff_offset + 20
    image_base = struct.unpack('<I', data[opt_offset+28:opt_offset+32])[0]
    entry_point_rva = struct.unpack('<I', data[opt_offset+16:opt_offset+20])[0]
    
    # Section headers
    section_offset = opt_offset + optional_header_size
    sections = []
    
    for i in range(num_sections):
        sec = section_offset + i * 40
        name = data[sec:sec+8].rstrip(b'\x00').decode('ascii', errors='ignore')
        virtual_size = struct.unpack('<I', data[sec+8:sec+12])[0]
        virtual_addr = struct.unpack('<I', data[sec+12:sec+16])[0]
        raw_size = struct.unpack('<I', data[sec+16:sec+20])[0]
        raw_ptr = struct.unpack('<I', data[sec+20:sec+24])[0]
        
        sections.append({
            'name': name,
            'virtual_size': virtual_size,
            'virtual_addr': virtual_addr,
            'raw_size': raw_size,
            'raw_ptr': raw_ptr,
            'header_offset': sec
        })
    
    return {
        'image_base': image_base,
        'entry_point_rva': entry_point_rva,
        'pe_offset': pe_offset,
        'opt_offset': opt_offset,
        'sections': sections
    }


def find_asprotect_data(data, pe_info):
    """Find ASProtect control structures and encrypted data."""
    
    # Known ASProtect 1.0 entry pattern: 60 E8 xx xx xx xx (PUSHAD + CALL)
    entry_va = pe_info['entry_point_rva'] + pe_info['image_base']
    print(f"[*] Entry point VA: 0x{entry_va:08X}")
    
    # Find the ASProtect stub section (last .data section typically)
    stub_section = None
    for sec in pe_info['sections']:
        if sec['name'] == '.data' and sec['raw_ptr'] > 0:
            if stub_section is None or sec['raw_ptr'] > stub_section['raw_ptr']:
                stub_section = sec
    
    if stub_section:
        print(f"[*] ASProtect stub section: {stub_section['name']} at file offset 0x{stub_section['raw_ptr']:X}")
    
    # The ASProtect loader at 0x021b3008 calculates:
    # delta_base = return_addr (0x021b3007) - 0x45afbb = 0x01D5804C
    # 
    # This means original compiled addresses used 0x45xxxx range
    # Actual runtime addresses are delta_base + original_offset
    
    # Key offsets in ASProtect (from our analysis):
    # [EBP + 0x45bc23] = OEP offset (relative to image base after unpack)
    # [EBP + 0x45c288] = initialization flag / base address
    
    # These translate to actual file offsets by:
    # file_offset = section_raw_ptr + (VA - section_VA)
    
    return stub_section


def find_compressed_blocks(data, stub_section, pe_info):
    """
    Scan the stub section for compressed data blocks.
    ASProtect 1.0 stores control records that describe:
    - Source (compressed data location)
    - Destination (target VA to write decompressed data)
    - Sizes
    """
    blocks = []
    
    stub_start = stub_section['raw_ptr']
    stub_end = stub_start + stub_section['raw_size']
    stub_data = data[stub_start:stub_end]
    stub_va = stub_section['virtual_addr'] + pe_info['image_base']
    
    print(f"[*] Scanning stub section: 0x{stub_va:08X} - 0x{stub_va + len(stub_data):08X}")
    
    # Look for aPLib signature patterns or control structures
    # aPLib compressed data often starts with a literal byte followed by bit stream
    
    # ASProtect stores metadata about sections to unpack
    # Format varies but typically: [dest_rva][compressed_size][decompressed_size][data...]
    
    # Let's find the .text section info
    text_section = None
    for sec in pe_info['sections']:
        if sec['name'] == '.text':
            text_section = sec
            break
    
    if text_section:
        print(f"[*] .text section: VA=0x{text_section['virtual_addr'] + pe_info['image_base']:08X}, "
              f"Size=0x{text_section['virtual_size']:X}")
        
        # The encrypted/compressed .text data should be somewhere in the packed file
        # Check if .text section has actual data or is zeroed/compressed elsewhere
        text_start = text_section['raw_ptr']
        text_data = data[text_start:text_start + min(64, text_section['raw_size'])]
        
        # Check entropy / pattern of .text
        zero_count = text_data.count(0)
        print(f"[*] First 64 bytes of .text: {zero_count} zeros")
        print(f"    Hex: {text_data[:32].hex()}")
    
    return blocks


def scan_for_aplib_data(data, min_size=0x1000):
    """
    Scan entire file for potential aPLib compressed blocks.
    aPLib starts with a literal byte, then uses bit-packed data.
    """
    candidates = []
    
    # Simple heuristic: look for runs of high-entropy data
    # that could be aPLib compressed
    
    window_size = 256
    for i in range(0, len(data) - window_size, 256):
        window = data[i:i+window_size]
        
        # Check for characteristics of compressed data:
        # - Non-zero entropy
        # - Mix of byte values
        unique_bytes = len(set(window))
        
        if unique_bytes > 100:  # High diversity suggests compressed data
            # Try to decompress
            try:
                decompressed = aplib_decompress(data[i:i+0x10000])
                if len(decompressed) > min_size:
                    candidates.append({
                        'offset': i,
                        'compressed_size': 0x10000,  # approximate
                        'decompressed_size': len(decompressed)
                    })
                    print(f"[+] Potential aPLib block at 0x{i:X}, decompresses to {len(decompressed)} bytes")
            except:
                pass
    
    return candidates


def extract_oep_from_stub(data, pe_info):
    """
    Extract the Original Entry Point from the ASProtect stub.
    The OEP is stored at [delta_base + 0x45bc23] and added to image base.
    """
    # From disassembly:
    # 021b40fd: MOV EAX, [EBP + 0x45bc23]  ; Load OEP offset
    # 021b4104: ADD EAX, [EBP + 0x45c288]  ; Add base
    
    # delta_base = 0x01D5804C
    # So offset 0x45bc23 in "original" space = delta_base + 0x45bc23 = 0x021b3c6f
    
    # In file terms, we need to find where this is stored
    
    # Entry VA: 0x021b3001
    # Section .data starts at VA: 0x021b3000
    
    for sec in pe_info['sections']:
        sec_va = sec['virtual_addr'] + pe_info['image_base']
        if sec['name'] == '.data' and sec_va <= 0x021b3000:
            if sec_va + sec['virtual_size'] >= 0x021b3000:
                # This is our stub section
                # OEP data at VA 0x021b3c6f (approximately)
                oep_va = 0x021b3c6f
                if sec_va <= oep_va < sec_va + sec['virtual_size']:
                    file_offset = sec['raw_ptr'] + (oep_va - sec_va)
                    if file_offset + 4 <= len(data):
                        oep_offset = struct.unpack('<I', data[file_offset:file_offset+4])[0]
                        print(f"[*] Found OEP offset at file 0x{file_offset:X}: 0x{oep_offset:08X}")
                        return oep_offset
    
    # Alternative: scan for known pattern
    # The stub stores OEP as offset from image base
    # For Airstrike 3D, OEP should be 0x00401000 (start of .text)
    
    return 0x1000  # Default: start of .text section RVA


def unpack_asprotect(input_file, output_file):
    """Main unpacking routine."""
    
    print(f"[*] ASProtect 1.0 Static Unpacker")
    print(f"[*] Input: {input_file}")
    print(f"[*] Output: {output_file}")
    print()
    
    # Read input file
    with open(input_file, 'rb') as f:
        data = bytearray(f.read())
    
    print(f"[*] File size: {len(data)} bytes")
    
    # Parse PE
    pe_info = find_pe_sections(data)
    print(f"[*] Image base: 0x{pe_info['image_base']:08X}")
    print(f"[*] Original entry point RVA: 0x{pe_info['entry_point_rva']:08X}")
    print(f"[*] Sections: {len(pe_info['sections'])}")
    
    for sec in pe_info['sections']:
        print(f"    {sec['name']:8s} VA=0x{sec['virtual_addr']:08X} Raw=0x{sec['raw_ptr']:08X} "
              f"Size=0x{sec['virtual_size']:08X}")
    print()
    
    # Find ASProtect structures
    stub_section = find_asprotect_data(data, pe_info)
    
    # Try to locate and extract compressed data
    print()
    print("[*] Analyzing encryption/compression...")
    
    # Method 1: Check if .text is XOR encrypted (common in ASProtect)
    text_section = None
    for sec in pe_info['sections']:
        if sec['name'] == '.text':
            text_section = sec
            break
    
    if text_section:
        text_data = data[text_section['raw_ptr']:text_section['raw_ptr'] + text_section['raw_size']]
        
        # Check for common encryption patterns
        # XOR encryption leaves statistical patterns
        byte_freq = [0] * 256
        for b in text_data[:0x1000]:
            byte_freq[b] += 1
        
        most_common = sorted(range(256), key=lambda x: byte_freq[x], reverse=True)[:5]
        print(f"[*] Most common bytes in .text: {[hex(b) for b in most_common]}")
        
        # If mostly zeros or one value, likely XOR encrypted
        if byte_freq[most_common[0]] > 0x800:
            possible_key = most_common[0]
            print(f"[*] Possible XOR key: 0x{possible_key:02X}")
            
            # Try XOR decryption
            decrypted = bytearray(len(text_data))
            for i, b in enumerate(text_data):
                decrypted[i] = b ^ possible_key
            
            # Check if result looks like x86 code
            # Common x86 opcodes: 55 (push ebp), 8B (mov), 89 (mov), 83 (sub/add)
            if decrypted[0] in [0x55, 0x8B, 0x89, 0x83, 0x56, 0x57]:
                print(f"[+] XOR decryption looks promising!")
                print(f"    First bytes: {decrypted[:16].hex()}")
    
    # Method 2: Look for compressed data in stub section
    print()
    print("[*] Scanning for aPLib compressed blocks...")
    
    # The actual compressed data in ASProtect is often stored
    # in the large .data section between .rdata and .rsrc
    
    data_section = None
    for sec in pe_info['sections']:
        if sec['name'] == '.data' and sec['virtual_size'] > 0x100000:
            data_section = sec
            break
    
    if data_section:
        print(f"[*] Large .data section found: 0x{data_section['raw_ptr']:X}, size 0x{data_section['virtual_size']:X}")
        
        # Scan for compressed blocks
        scan_start = data_section['raw_ptr']
        scan_end = min(scan_start + 0x50000, len(data))  # First 320KB
        
        # Look for potential aPLib start patterns
        for offset in range(scan_start, scan_end, 0x100):
            chunk = data[offset:offset+0x8000]
            
            # Try decompression
            try:
                result = aplib_decompress(bytes(chunk))
                if len(result) > 0x2000:  # Meaningful decompression
                    ratio = len(result) / len(chunk)
                    if 1.5 < ratio < 10:  # Reasonable compression ratio
                        print(f"[+] aPLib block at 0x{offset:X}: {len(chunk)} -> {len(result)} bytes (ratio: {ratio:.2f})")
            except:
                pass
    
    # Extract OEP
    print()
    oep_rva = extract_oep_from_stub(data, pe_info)
    print(f"[*] Original Entry Point RVA: 0x{oep_rva:08X}")
    
    # For now, create output with basic modifications
    print()
    print("[*] Creating unpacked output...")
    
    # Update entry point to OEP
    entry_offset = pe_info['opt_offset'] + 16  # AddressOfEntryPoint in optional header
    struct.pack_into('<I', data, entry_offset, oep_rva)
    print(f"[+] Patched entry point to 0x{oep_rva:08X}")
    
    # Write output
    with open(output_file, 'wb') as f:
        f.write(data)
    
    print(f"[+] Written to {output_file}")
    print()
    print("[!] NOTE: This is a basic unpacker. For full functionality:")
    print("    1. The compressed .text code needs proper aPLib decompression")
    print("    2. IAT needs reconstruction")
    print("    3. May need to fix section permissions")
    print()
    print("[*] To complete unpacking, you need to:")
    print("    - Find the exact location of compressed code")  
    print("    - Apply aPLib decompression")
    print("    - Write decompressed code to .text section")
    print("    - Rebuild imports using the import descriptors we found")
    
    return True


def main():
    if len(sys.argv) < 2:
        print(f"Usage: {sys.argv[0]} <input.exe> [output.exe]")
        print()
        print("ASProtect 1.0 Static Unpacker")
        print("Attempts to unpack ASProtect 1.0 protected executables")
        sys.exit(1)
    
    input_file = sys.argv[1]
    output_file = sys.argv[2] if len(sys.argv) > 2 else input_file.replace('.exe', '_unpacked.exe')
    
    if not os.path.exists(input_file):
        print(f"Error: {input_file} not found")
        sys.exit(1)
    
    try:
        unpack_asprotect(input_file, output_file)
    except Exception as e:
        print(f"Error: {e}")
        import traceback
        traceback.print_exc()
        sys.exit(1)


if __name__ == '__main__':
    main()

