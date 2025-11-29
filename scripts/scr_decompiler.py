#!/usr/bin/env python3
"""
RCSL Script Decompiler

Decompiles .scr (RCSL) script files to readable Lua format.
Based on Ghidra reverse engineering analysis of execute_script_instruction and load_script_file.

Script Format (from Ghidra analysis):
- Header: 56 bytes (magic + counts + entry points)
- Chunks: STRG (string table), DATA (variable table), FUNC (native functions), 
          CODE (bytecode), CASH (string constants), DEFS (string constant definitions)
- Instructions: 14 bytes each (opcode, flags, operand_a, operand_b, result)

Usage:
    scr_decompiler.py <script_file.scr> [-o output.lua]
    scr_decompiler.py --all [--dir assets/scripts] [--force]
    
With --all, Lua files are placed next to each .scr file (e.g., script.scr -> script.lua)
"""

import sys
import struct
import argparse
from pathlib import Path
from typing import Optional, Dict, List, Any, Tuple
from dataclasses import dataclass, field, asdict
from enum import IntEnum

# Script file structure offsets
SCRIPT_FILENAME_SIZE = 64
SCRIPT_HEADER_SIZE = 0x90

# Chunk type magic values
CHUNK_RCSL = 0x4c534352  # "RCSL" - Script file header
CHUNK_STRG = 0x47525453  # "STRG" - String table
CHUNK_DATA = 0x41544144  # "DATA" - Variable data table
CHUNK_FUNC = 0x434e5546  # "FUNC" - Function table
CHUNK_CODE = 0x45444f43  # "CODE" - Bytecode
CHUNK_CASH = 0x48534143  # "CASH" - String constant table
CHUNK_DEFS = 0x53464544  # "DEFS" - String constant definitions

# Instruction size (confirmed from Ghidra execute_script_instruction analysis)
INSTRUCTION_SIZE = 14

# Opcode enum (from Ghidra execute_script_instruction @ 0041d540 analysis)
# Each opcode is a byte value (0x00-0x1E)
# Switch statement uses float* cast as compiler workaround
class ScriptOpcode(IntEnum):
    NOP = 0x00
    MUL = 0x01
    DIV = 0x02
    ADD = 0x03
    SUB = 0x04
    AND = 0x05
    OR = 0x06
    NOT = 0x07
    NEG = 0x08
    EQ = 0x09
    NE = 0x0A
    LT = 0x0B
    GT = 0x0C
    LE = 0x0D
    GE = 0x0E
    IS_ZERO = 0x0F
    NEGATE = 0x10
    ASSIGN = 0x11
    ARRAY_INDEX = 0x12
    PUSH = 0x13
    POP = 0x14
    ALLOC = 0x15
    RETURN = 0x18
    JMP_IF_FALSE = 0x19
    JMP_IF_TRUE = 0x1A
    JMP = 0x1B
    CALL = 0x1C
    CALL_WAIT = 0x1D
    SET_WAIT_TIME = 0x1E

OPCODE_NAMES = {op.value: op.name for op in ScriptOpcode}

# Addressing mode flags
FLAG_OPERAND_A_IMMEDIATE = 0x01
FLAG_OPERAND_A_INDIRECT = 0x02
FLAG_OPERAND_B_IMMEDIATE = 0x10
FLAG_OPERAND_B_INDIRECT = 0x20
FLAG_RESULT_INDIRECT = 0x80

# Function entry point types
FUNC_INIT = 0
FUNC_UPDATE = 1
FUNC_ON_DAMAGE = 2
FUNC_ON_DEATH = 3
FUNC_ON_COLLISION = 4

FUNC_NAMES = {
    FUNC_INIT: "init",
    FUNC_UPDATE: "update",
    FUNC_ON_DAMAGE: "on_damage",
    FUNC_ON_DEATH: "on_death",
    FUNC_ON_COLLISION: "on_collision",
}


@dataclass
class ScriptInstruction:
    """Single RCSL instruction."""
    address: int
    opcode: int
    opcode_name: str
    flags: int
    operand_a: int
    operand_b: int
    result: int
    operand_a_immediate: Optional[float] = None
    operand_b_immediate: Optional[float] = None
    operand_a_reg: Optional[int] = None
    operand_b_reg: Optional[int] = None
    result_reg: Optional[int] = None
    comment: str = ""

    def to_dict(self) -> Dict[str, Any]:
        d = asdict(self)
        # Convert float to string for YAML if present
        if d.get('operand_a_immediate') is not None:
            d['operand_a_immediate'] = float(d['operand_a_immediate'])
        if d.get('operand_b_immediate') is not None:
            d['operand_b_immediate'] = float(d['operand_b_immediate'])
        return d


@dataclass
class ScriptFunction:
    """Script function definition."""
    index: int
    name: str
    entry_point: int
    instructions: List[ScriptInstruction] = field(default_factory=list)


@dataclass
class ScriptFile:
    """Complete RCSL script file structure."""
    filename: str
    function_count: int
    string_constant_count: int
    native_function_count: int
    variable_table_count: int
    local_variable_count: int
    string_table_size: int
    code_size: int  # Number of instructions
    
    # Function entry points
    init_entry_point: int = -1
    update_entry_point: int = -1
    on_damage_entry_point: int = -1
    on_death_entry_point: int = -1
    on_collision_entry_point: int = -1
    
    # Tables
    string_table: List[Dict[str, Any]] = field(default_factory=list)
    string_constant_table: List[Dict[str, Any]] = field(default_factory=list)
    native_function_table: List[Dict[str, Any]] = field(default_factory=list)
    variable_table: List[Dict[str, Any]] = field(default_factory=list)
    
    # Bytecode
    instructions: List[ScriptInstruction] = field(default_factory=list)
    functions: List[ScriptFunction] = field(default_factory=list)
    
    # Raw data
    string_table_data: Optional[bytes] = None
    code_data: Optional[bytes] = None


def parse_instruction(data: bytes, address: int, string_table: List[Dict], 
                     native_funcs: List[Dict], string_consts: List[Dict]) -> ScriptInstruction:
    """Parse a single 14-byte instruction."""
    if len(data) < INSTRUCTION_SIZE:
        raise ValueError(f"Not enough data for instruction at address {address}")
    
    opcode = data[0]
    flags = data[1]
    operand_a = struct.unpack('<i', data[2:6])[0]
    operand_b = struct.unpack('<i', data[6:10])[0]
    result = struct.unpack('<i', data[10:14])[0]
    
    opcode_name = OPCODE_NAMES.get(opcode, f"UNKNOWN_{opcode:02X}")
    
    inst = ScriptInstruction(
        address=address,
        opcode=opcode,
        opcode_name=opcode_name,
        flags=flags,
        operand_a=operand_a,
        operand_b=operand_b,
        result=result
    )
    
    # Decode addressing modes
    if flags & FLAG_OPERAND_A_IMMEDIATE:
        inst.operand_a_immediate = struct.unpack('<f', data[2:6])[0]
    else:
        inst.operand_a_reg = operand_a
    
    if flags & FLAG_OPERAND_B_IMMEDIATE:
        inst.operand_b_immediate = struct.unpack('<f', data[6:10])[0]
    else:
        inst.operand_b_reg = operand_b
    
    inst.result_reg = result
    
    # Generate comment
    comment_parts = []
    
    # Operand A
    if flags & FLAG_OPERAND_A_IMMEDIATE:
        comment_parts.append(f"a={inst.operand_a_immediate}")
    else:
        if operand_a < 0:
            comment_parts.append(f"a=local[{abs(operand_a)}]")
        else:
            comment_parts.append(f"a=reg[{operand_a}]")
        if flags & FLAG_OPERAND_A_INDIRECT:
            comment_parts[-1] += "*"
    
    # Operand B
    if flags & FLAG_OPERAND_B_IMMEDIATE:
        comment_parts.append(f"b={inst.operand_b_immediate}")
    else:
        if operand_b < 0:
            comment_parts.append(f"b=local[{abs(operand_b)}]")
        else:
            comment_parts.append(f"b=reg[{operand_b}]")
        if flags & FLAG_OPERAND_B_INDIRECT:
            comment_parts[-1] += "*"
    
    # Result
    if result < 0:
        comment_parts.append(f"res=local[{abs(result)}]")
    else:
        comment_parts.append(f"res=reg[{result}]")
    if flags & FLAG_RESULT_INDIRECT:
        comment_parts[-1] += "*"
    
    # Special handling for jumps
    if opcode in (ScriptOpcode.JMP, ScriptOpcode.JMP_IF_FALSE, ScriptOpcode.JMP_IF_TRUE):
        if opcode == ScriptOpcode.JMP:
            jump_offset = operand_a
        else:
            jump_offset = operand_b
        target = address + INSTRUCTION_SIZE + (jump_offset * INSTRUCTION_SIZE)
        comment_parts.append(f"-> {target:04X}")
    
    # Special handling for function calls
    if opcode in (ScriptOpcode.CALL, ScriptOpcode.CALL_WAIT):
        func_index = operand_a
        if func_index < 0:
            # Native function (negative index)
            native_idx = abs(func_index) - 1
            if 0 <= native_idx < len(native_funcs):
                func_name = native_funcs[native_idx].get('name', f'native_func_{native_idx}')
                comment_parts.append(f"native: {func_name}")
            else:
                comment_parts.append(f"native[{native_idx}]")
        else:
            # Script function (positive index)
            comment_parts.append(f"script_func[{func_index}]")
    
    inst.comment = ", ".join(comment_parts)
    
    return inst


def parse_string_table_entry(data: bytes, offset: int) -> Dict[str, Any]:
    """Parse a string table entry (0x48 bytes)."""
    entry_type = data[offset]
    name_bytes = data[offset + 4:offset + 72]
    name = name_bytes.split(b'\x00')[0].decode('ascii', errors='replace')
    
    return {
        'type': entry_type,
        'name': name,
        'offset': offset
    }


def parse_variable_table_entry(data: bytes, offset: int) -> Dict[str, Any]:
    """Parse a variable table entry (8 bytes)."""
    var_type = struct.unpack('<H', data[offset:offset+2])[0]
    register_index = struct.unpack('<h', data[offset+2:offset+4])[0]
    value = struct.unpack('<i', data[offset+4:offset+8])[0]
    
    # Try to interpret as float if type is 2
    value_float = None
    if var_type == 2:
        value_float = struct.unpack('<f', data[offset+4:offset+8])[0]
    
    return {
        'type': var_type,
        'type_name': {2: 'float', 3: 'pointer'}.get(var_type, f'unknown_{var_type}'),
        'register_index': register_index,
        'value': value,
        'value_float': value_float,
        'offset': offset
    }


def load_script_file(filepath: Path) -> ScriptFile:
    """Load and parse an RCSL script file."""
    with open(filepath, 'rb') as f:
        data = f.read()
    
    if len(data) < 16:
        raise ValueError(f"File too small: {len(data)} bytes")
    
    # Parse header - file starts with "RCSL" magic
    file_header = struct.unpack('<I', data[0:4])[0]
    if file_header != CHUNK_RCSL:
        raise ValueError(f"Invalid file header: {file_header:08X} (expected {CHUNK_RCSL:08X})")
    
    # Header structure (based on actual file format):
    # [0x00-0x03] "RCSL" magic
    # [0x04-0x07] header_size (int)
    # [0x08-0x0B] function_count (int)
    # [0x0C-0x0F] string_constant_count (int)
    # [0x10-0x13] native_function_count (int)
    # [0x14-0x17] variable_table_count (int)
    # [0x18-0x1B] local_variable_count (int)
    # [0x1C-0x1F] string_table_size (int)
    # [0x20-0x23] code_size (int) - number of instructions
    # [0x24-0x27] update_entry_point (int)
    # [0x28-0x2B] init_entry_point (int)
    # [0x2C-0x2F] on_damage_entry_point (int)
    # [0x30-0x33] on_death_entry_point (int)
    # [0x34-0x37] on_collision_entry_point (int)
    
    header_size = struct.unpack('<I', data[4:8])[0]
    function_count = struct.unpack('<i', data[8:12])[0]
    string_constant_count = struct.unpack('<i', data[12:16])[0]
    native_function_count = struct.unpack('<i', data[16:20])[0]
    variable_table_count = struct.unpack('<i', data[20:24])[0]
    local_variable_count = struct.unpack('<i', data[24:28])[0]
    string_table_size = struct.unpack('<i', data[28:32])[0]
    code_size = struct.unpack('<i', data[32:36])[0]
    
    # Entry points
    update_entry_point = struct.unpack('<i', data[36:40])[0]
    init_entry_point = struct.unpack('<i', data[40:44])[0]
    on_damage_entry_point = struct.unpack('<i', data[44:48])[0]
    on_death_entry_point = struct.unpack('<i', data[48:52])[0]
    on_collision_entry_point = struct.unpack('<i', data[52:56])[0]
    
    filename = filepath.name
    
    script = ScriptFile(
        filename=filename,
        function_count=function_count,
        string_constant_count=string_constant_count,
        native_function_count=native_function_count,
        variable_table_count=variable_table_count,
        local_variable_count=local_variable_count,
        string_table_size=string_table_size,
        code_size=code_size,
        init_entry_point=init_entry_point,
        update_entry_point=update_entry_point,
        on_damage_entry_point=on_damage_entry_point,
        on_death_entry_point=on_death_entry_point,
        on_collision_entry_point=on_collision_entry_point
    )
    
    # Parse chunks from the file
    # Header is 56 bytes total: magic(4) + header_size(4) + 12 ints(48) = 56
    # Chunks start at offset 56
    chunk_start = 56
    if chunk_start >= len(data):
        return script  # No chunks, return early
    
    chunk_data = data[chunk_start:]
    pos = 0
    
    while pos < len(chunk_data) - 8:
        chunk_type = struct.unpack('<I', chunk_data[pos:pos+4])[0]
        chunk_size = struct.unpack('<I', chunk_data[pos+4:pos+8])[0]
        chunk_start = pos + 8
        
        if chunk_start + chunk_size > len(chunk_data):
            break
        
        chunk_bytes = chunk_data[chunk_start:chunk_start + chunk_size]
        
        if chunk_type == CHUNK_STRG:
            # String table
            script.string_table_data = chunk_bytes
            for i in range(function_count):
                entry_offset = i * 0x48
                if entry_offset + 0x48 <= len(chunk_bytes):
                    entry = parse_string_table_entry(chunk_bytes, entry_offset)
                    script.string_table.append(entry)
        
        elif chunk_type == CHUNK_DATA:
            # Variable data table
            for i in range(variable_table_count):
                entry_offset = i * 8
                if entry_offset + 8 <= len(chunk_bytes):
                    entry = parse_variable_table_entry(chunk_bytes, entry_offset)
                    script.variable_table.append(entry)
        
        elif chunk_type == CHUNK_FUNC:
            # Native function table - contains function names
            # Format: length-prefixed strings
            i = 0
            str_pos = 0
            while i < native_function_count and str_pos < len(chunk_bytes):
                name_len = chunk_bytes[str_pos]
                if name_len == 0 or str_pos + 1 + name_len > len(chunk_bytes):
                    break
                name_bytes = chunk_bytes[str_pos + 1:str_pos + 1 + name_len]
                name = name_bytes.split(b'\x00')[0].decode('ascii', errors='replace')
                script.native_function_table.append({
                    'index': i,
                    'name': name
                })
                str_pos += 1 + name_len
                i += 1
            # Fill remaining with placeholders if needed
            while i < native_function_count:
                script.native_function_table.append({
                    'index': i,
                    'name': f'native_func_{i}'
                })
                i += 1
        
        elif chunk_type == CHUNK_CODE:
            # Bytecode
            script.code_data = chunk_bytes
            num_instructions = len(chunk_bytes) // INSTRUCTION_SIZE
            for i in range(num_instructions):
                inst_offset = i * INSTRUCTION_SIZE
                inst_data = chunk_bytes[inst_offset:inst_offset + INSTRUCTION_SIZE]
                instruction = parse_instruction(
                    inst_data, 
                    inst_offset,
                    script.string_table,
                    script.native_function_table,
                    script.string_constant_table
                )
                script.instructions.append(instruction)
        
        elif chunk_type == CHUNK_DEFS:
            # String constant definitions - contains string names
            # Format: length-prefixed strings
            i = 0
            str_pos = 0
            while i < string_constant_count and str_pos < len(chunk_bytes):
                name_len = chunk_bytes[str_pos]
                if name_len == 0 or str_pos + 1 + name_len > len(chunk_bytes):
                    break
                name_bytes = chunk_bytes[str_pos + 1:str_pos + 1 + name_len]
                name = name_bytes.split(b'\x00')[0].decode('ascii', errors='replace')
                script.string_constant_table.append({
                    'index': i,
                    'name': name
                })
                str_pos += 1 + name_len
                i += 1
            # Fill remaining with placeholders if needed
            while i < string_constant_count:
                script.string_constant_table.append({
                    'index': i,
                    'name': f'string_const_{i}'
                })
                i += 1
        
        pos = chunk_start + chunk_size
    
    # Build function list from entry points
    entry_points = {
        'init': init_entry_point,
        'update': update_entry_point,
        'on_damage': on_damage_entry_point,
        'on_death': on_death_entry_point,
        'on_collision': on_collision_entry_point
    }
    
    for func_name, entry_point in entry_points.items():
        if entry_point >= 0 and entry_point < len(script.instructions):
            func = ScriptFunction(
                index=entry_point,
                name=func_name,
                entry_point=entry_point,
                instructions=script.instructions[entry_point:]
            )
            script.functions.append(func)
    
    return script


def escape_lua_string(s: str) -> str:
    """Escape a string for Lua output."""
    return s.replace('\\', '\\\\').replace('"', '\\"').replace('\n', '\\n').replace('\r', '\\r')


def format_lua_value(value: Any, indent: str = "") -> str:
    """Format a value for Lua output."""
    if value is None:
        return "nil"
    elif isinstance(value, bool):
        return "true" if value else "false"
    elif isinstance(value, (int, float)):
        return str(value)
    elif isinstance(value, str):
        return f'"{escape_lua_string(value)}"'
    elif isinstance(value, list):
        if not value:
            return "{}"
        lines = ["{"]
        for item in value:
            lines.append(f"{indent}  {format_lua_value(item, indent + '  ')},")
        lines.append(f"{indent}}}")
        return "\n".join(lines)
    elif isinstance(value, dict):
        if not value:
            return "{}"
        lines = ["{"]
        for k, v in value.items():
            if isinstance(k, str) and k.isidentifier():
                lines.append(f"{indent}  {k} = {format_lua_value(v, indent + '  ')},",)
            else:
                lines.append(f"{indent}  [{format_lua_value(k, indent + '  ')}] = {format_lua_value(v, indent + '  ')},",)
        lines.append(f"{indent}}}")
        return "\n".join(lines)
    else:
        return str(value)


def script_to_lua(script: ScriptFile) -> str:
    """Convert ScriptFile to Lua code."""
    lines = []
    lines.append("-- Decompiled RCSL script file")
    lines.append(f"-- Original file: {script.filename}")
    lines.append("-- Generated by scr_decompiler.py")
    lines.append("")
    lines.append("local script = {}")
    lines.append("")
    
    # Metadata
    lines.append("script.metadata = {")
    lines.append(f"  function_count = {script.function_count},")
    lines.append(f"  string_constant_count = {script.string_constant_count},")
    lines.append(f"  native_function_count = {script.native_function_count},")
    lines.append(f"  variable_table_count = {script.variable_table_count},")
    lines.append(f"  local_variable_count = {script.local_variable_count},")
    lines.append(f"  string_table_size = {script.string_table_size},")
    lines.append(f"  code_size = {script.code_size},")
    lines.append("}")
    lines.append("")
    
    # Entry points
    lines.append("script.entry_points = {")
    for name, entry_point in [
        ('init', script.init_entry_point),
        ('update', script.update_entry_point),
        ('on_damage', script.on_damage_entry_point),
        ('on_death', script.on_death_entry_point),
        ('on_collision', script.on_collision_entry_point),
    ]:
        if entry_point >= 0:
            lines.append(f"  {name} = {entry_point},")
        else:
            lines.append(f"  {name} = nil,")
    lines.append("}")
    lines.append("")
    
    # Native function table
    if script.native_function_table:
        lines.append("script.native_function_table = {")
        for func in script.native_function_table:
            lines.append(f'  [{func["index"]}] = "{escape_lua_string(func["name"])}",')
        lines.append("}")
        lines.append("")
    
    # String constant table
    if script.string_constant_table:
        lines.append("script.string_constant_table = {")
        for const in script.string_constant_table:
            lines.append(f'  [{const["index"]}] = "{escape_lua_string(const["name"])}",')
        lines.append("}")
        lines.append("")
    
    # Variable table
    if script.variable_table:
        lines.append("script.variable_table = {")
        for var in script.variable_table:
            lines.append("  {")
            lines.append(f'    type = {var["type"]},')
            lines.append(f'    type_name = "{var.get("type_name", "unknown")}",')
            lines.append(f'    register_index = {var["register_index"]},')
            if var.get("value_float") is not None:
                lines.append(f'    value_float = {var["value_float"]},')
            lines.append(f'    value = {var["value"]},')
            lines.append("  },")
        lines.append("}")
        lines.append("")
    
    # Instructions
    if script.instructions:
        lines.append("script.instructions = {")
        for inst in script.instructions:
            lines.append("  {")
            lines.append(f"    address = {inst.address},")
            lines.append(f"    opcode = {inst.opcode}, -- {inst.opcode_name}")
            lines.append(f"    flags = {inst.flags},")
            if inst.operand_a_immediate is not None:
                lines.append(f"    operand_a_immediate = {inst.operand_a_immediate},")
            if inst.operand_a_reg is not None:
                lines.append(f"    operand_a_reg = {inst.operand_a_reg},")
            if inst.operand_b_immediate is not None:
                lines.append(f"    operand_b_immediate = {inst.operand_b_immediate},")
            if inst.operand_b_reg is not None:
                lines.append(f"    operand_b_reg = {inst.operand_b_reg},")
            lines.append(f"    result_reg = {inst.result_reg},")
            if inst.comment:
                lines.append(f'    -- {inst.comment}')
            lines.append("  },")
        lines.append("}")
        lines.append("")
    
    # Functions
    if script.functions:
        lines.append("script.functions = {")
        for func in script.functions:
            lines.append("  {")
            lines.append(f'    name = "{func.name}",')
            lines.append(f"    index = {func.index},")
            lines.append(f"    entry_point = {func.entry_point},")
            lines.append(f"    instruction_count = {len(func.instructions)},")
            lines.append("    instructions = {")
            for inst in func.instructions:
                lines.append("      {")
                lines.append(f"        address = {inst.address},")
                lines.append(f"        opcode = {inst.opcode}, -- {inst.opcode_name}")
                lines.append(f"        flags = {inst.flags},")
                if inst.operand_a_immediate is not None:
                    lines.append(f"        operand_a_immediate = {inst.operand_a_immediate},")
                if inst.operand_a_reg is not None:
                    lines.append(f"        operand_a_reg = {inst.operand_a_reg},")
                if inst.operand_b_immediate is not None:
                    lines.append(f"        operand_b_immediate = {inst.operand_b_immediate},")
                if inst.operand_b_reg is not None:
                    lines.append(f"        operand_b_reg = {inst.operand_b_reg},")
                lines.append(f"        result_reg = {inst.result_reg},")
                if inst.comment:
                    lines.append(f'        -- {inst.comment}')
                lines.append("      },")
            lines.append("    },")
            lines.append("  },")
        lines.append("}")
        lines.append("")
    
    lines.append("return script")
    return "\n".join(lines)


def main():
    parser = argparse.ArgumentParser(
        description="Decompile RCSL script files to Lua format"
    )
    parser.add_argument('input', nargs='?', type=Path, help='Input .scr file')
    parser.add_argument('-o', '--output', type=Path, help='Output Lua file')
    parser.add_argument('--all', action='store_true', 
                       help='Process all .scr files recursively and place Lua next to each .scr file')
    parser.add_argument('--dir', type=Path, default=Path('assets/scripts'), 
                       help='Directory to search for .scr files (default: assets/scripts)')
    parser.add_argument('--force', action='store_true',
                       help='Force re-decompilation even if Lua is newer than .scr file')
    
    args = parser.parse_args()
    
    if args.all:
        # Process all .scr files recursively
        script_dir = args.dir
        if not script_dir.exists():
            print(f"Error: Directory {script_dir} does not exist", file=sys.stderr)
            return 1
        
        # Find all .scr files recursively
        scr_files = list(script_dir.rglob('*.scr'))
        if not scr_files:
            print(f"No .scr files found in {script_dir}", file=sys.stderr)
            return 1
        
        print(f"Found {len(scr_files)} script files")
        print(f"Processing and placing Lua files next to .scr files...\n")
        
        success_count = 0
        error_count = 0
        skip_count = 0
        
        for scr_file in sorted(scr_files):
            try:
                # Place Lua file next to .scr file
                output_file = scr_file.with_suffix('.lua')
                
                # Skip if Lua already exists and is newer (unless --force is used)
                if not args.force and output_file.exists() and output_file.stat().st_mtime > scr_file.stat().st_mtime:
                    print(f"Skipping {scr_file.relative_to(script_dir)} (Lua is newer, use --force to override)")
                    skip_count += 1
                    continue
                
                relative_path = scr_file.relative_to(script_dir)
                print(f"Processing {relative_path}...", end=' ')
                
                script = load_script_file(scr_file)
                
                with open(output_file, 'w', encoding='utf-8') as f:
                    f.write(script_to_lua(script))
                
                print(f"✓ -> {output_file.name}")
                success_count += 1
            except Exception as e:
                print(f"✗ Error: {e}", file=sys.stderr)
                error_count += 1
        
        print(f"\n{'='*60}")
        print(f"Decompiled: {success_count} files")
        if skip_count > 0:
            print(f"Skipped: {skip_count} files (Lua already exists and is newer)")
        if error_count > 0:
            print(f"Errors: {error_count} files")
        print(f"{'='*60}")
        return 0 if error_count == 0 else 1
    
    elif args.input:
        # Process single file
        if not args.input.exists():
            print(f"Error: File {args.input} does not exist", file=sys.stderr)
            return 1
        
        try:
            script = load_script_file(args.input)
            output_file = args.output or args.input.with_suffix('.lua')
            
            with open(output_file, 'w', encoding='utf-8') as f:
                f.write(script_to_lua(script))
            
            print(f"Decompiled {args.input} -> {output_file}")
            return 0
        except Exception as e:
            print(f"Error: {e}", file=sys.stderr)
            import traceback
            traceback.print_exc()
            return 1
    else:
        parser.print_help()
        return 1


if __name__ == '__main__':
    sys.exit(main())

