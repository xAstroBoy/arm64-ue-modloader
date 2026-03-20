#!/usr/bin/env python3
"""
RE4 VR — UAsset Blueprint Decompiler
═════════════════════════════════════
Parses UE4.25 cooked .uasset/.uexp files and decompiles Blueprint Kismet bytecode.

Usage:
  python tools/dump_uasset.py <file.uasset>                  # Full dump
  python tools/dump_uasset.py <file.uasset> --names          # Names only
  python tools/dump_uasset.py <file.uasset> --kismet         # Kismet bytecode only
  python tools/dump_uasset.py --scan <directory>              # Find all Blueprints
  python tools/dump_uasset.py --scan <directory> --search <pattern>  # Search in names

Handles UE4.25 (v522) cooked Android (ETC2) format.
"""
import struct, sys, os, re, argparse
from pathlib import Path
from dataclasses import dataclass, field
from typing import List, Optional, Tuple, Dict

# ═══════════════════════════════════════════════════════════════════════
# UE4 KISMET OPCODES (EExprToken from Script.h)
# ═══════════════════════════════════════════════════════════════════════
KISMET_OPCODES = {
    0x00: "EX_LocalVariable",
    0x01: "EX_InstanceVariable",
    0x02: "EX_DefaultVariable",
    0x04: "EX_Return",
    0x06: "EX_Jump",
    0x07: "EX_JumpIfNot",
    0x09: "EX_Assert",
    0x0B: "EX_Nothing",
    0x0F: "EX_Let",
    0x10: "EX_ClassSparseDataVariable",  # UE4.25+
    0x12: "EX_ClassContext",
    0x13: "EX_MetaCast",
    0x14: "EX_LetBool",
    0x15: "EX_EndParmValue",
    0x16: "EX_EndFunctionParms",
    0x17: "EX_Self",
    0x18: "EX_Skip",
    0x19: "EX_Context",
    0x1A: "EX_Context_FailSilent",
    0x1B: "EX_VirtualFunction",
    0x1C: "EX_FinalFunction",
    0x1D: "EX_IntConst",
    0x1E: "EX_FloatConst",
    0x1F: "EX_StringConst",
    0x20: "EX_ObjectConst",
    0x21: "EX_NameConst",
    0x22: "EX_RotationConst",
    0x23: "EX_VectorConst",
    0x24: "EX_ByteConst",
    0x25: "EX_IntZero",
    0x26: "EX_IntOne",
    0x27: "EX_True",
    0x28: "EX_False",
    0x29: "EX_TextConst",
    0x2A: "EX_NoObject",
    0x2B: "EX_TransformConst",
    0x2C: "EX_IntConstByte",
    0x2D: "EX_NoInterface",
    0x2E: "EX_DynamicCast",
    0x2F: "EX_StructConst",
    0x30: "EX_EndStructConst",
    0x31: "EX_SetArray",
    0x32: "EX_EndArray",
    0x33: "EX_PropertyConst",
    0x34: "EX_UnicodeStringConst",
    0x35: "EX_Int64Const",
    0x36: "EX_UInt64Const",
    0x38: "EX_PrimitiveCast",
    0x39: "EX_SetSet",
    0x3A: "EX_EndSet",
    0x3B: "EX_SetMap",
    0x3C: "EX_EndMap",
    0x3D: "EX_SetConst",
    0x3E: "EX_EndSetConst",
    0x3F: "EX_MapConst",
    0x40: "EX_EndMapConst",
    0x42: "EX_StructMemberContext",
    0x43: "EX_LetMulticastDelegate",
    0x44: "EX_LetDelegate",
    0x45: "EX_LocalVirtualFunction",
    0x46: "EX_LocalFinalFunction",
    0x48: "EX_LocalOutVariable",
    0x4A: "EX_DeprecatedOp4A",
    0x4B: "EX_InstanceDelegate",
    0x4C: "EX_PushExecutionFlow",
    0x4D: "EX_PopExecutionFlow",
    0x4E: "EX_ComputedJump",
    0x4F: "EX_PopExecutionFlowIfNot",
    0x50: "EX_Breakpoint",
    0x51: "EX_InterfaceContext",
    0x52: "EX_ObjToInterfaceCast",
    0x53: "EX_EndOfScript",
    0x54: "EX_CrossInterfaceCast",
    0x55: "EX_InterfaceToObjCast",
    0x5A: "EX_WireTracepoint",
    0x5B: "EX_SkipOffsetConst",
    0x5C: "EX_AddMulticastDelegate",
    0x5D: "EX_ClearMulticastDelegate",
    0x5E: "EX_Tracepoint",
    0x5F: "EX_LetObj",
    0x60: "EX_LetWeakObjPtr",
    0x61: "EX_BindDelegate",
    0x62: "EX_RemoveMulticastDelegate",
    0x63: "EX_CallMulticastDelegate",
    0x64: "EX_LetValueOnPersistentFrame",
    0x65: "EX_ArrayConst",
    0x66: "EX_EndArrayConst",
    0x67: "EX_SoftObjectConst",
    0x68: "EX_CallMath",
    0x69: "EX_SwitchValue",
    0x6A: "EX_InstrumentationEvent",
    0x6B: "EX_ArrayGetByRef",
    0x6C: "EX_ClassSparseDataVariable2",
    0x6D: "EX_FieldPathConst",
}

# ═══════════════════════════════════════════════════════════════════════
# BINARY READER HELPERS
# ═══════════════════════════════════════════════════════════════════════

class BinaryReader:
    """Little-endian binary reader with position tracking."""

    def __init__(self, data: bytes, offset: int = 0):
        self.data = data
        self.pos = offset

    def remaining(self) -> int:
        return len(self.data) - self.pos

    def read_bytes(self, n: int) -> bytes:
        if self.pos + n > len(self.data):
            raise EOFError(f"Read past end: pos={self.pos} n={n} len={len(self.data)}")
        result = self.data[self.pos:self.pos + n]
        self.pos += n
        return result

    def read_u8(self) -> int:
        return struct.unpack_from('<B', self.data, self._advance(1))[0]

    def read_u16(self) -> int:
        return struct.unpack_from('<H', self.data, self._advance(2))[0]

    def read_i32(self) -> int:
        return struct.unpack_from('<i', self.data, self._advance(4))[0]

    def read_u32(self) -> int:
        return struct.unpack_from('<I', self.data, self._advance(4))[0]

    def read_i64(self) -> int:
        return struct.unpack_from('<q', self.data, self._advance(8))[0]

    def read_u64(self) -> int:
        return struct.unpack_from('<Q', self.data, self._advance(8))[0]

    def read_f32(self) -> float:
        return struct.unpack_from('<f', self.data, self._advance(4))[0]

    def read_guid(self) -> bytes:
        return self.read_bytes(16)

    def read_fstring(self) -> str:
        """Read UE4 FString: int32 length, then chars."""
        length = self.read_i32()
        if length == 0:
            return ""
        if length > 0:
            # ASCII/Latin1
            raw = self.read_bytes(length)
            return raw[:-1].decode('utf-8', errors='replace')  # strip null
        else:
            # UTF-16LE
            char_count = -length
            raw = self.read_bytes(char_count * 2)
            return raw[:-2].decode('utf-16-le', errors='replace')  # strip null

    def read_fname(self) -> Tuple[int, int]:
        """Read serialized FName: index + number."""
        idx = self.read_i32()
        num = self.read_i32()
        return (idx, num)

    def _advance(self, n: int) -> int:
        pos = self.pos
        if pos + n > len(self.data):
            raise EOFError(f"Read past end: pos={pos} n={n} len={len(self.data)}")
        self.pos += n
        return pos

    def seek(self, pos: int):
        self.pos = pos

    def tell(self) -> int:
        return self.pos


# ═══════════════════════════════════════════════════════════════════════
# UASSET STRUCTURES
# ═══════════════════════════════════════════════════════════════════════

@dataclass
class UImport:
    class_package: str = ""
    class_name: str = ""
    outer_index: int = 0
    object_name: str = ""

@dataclass
class UExport:
    class_index: int = 0
    super_index: int = 0
    template_index: int = 0
    outer_index: int = 0
    object_name: str = ""
    object_flags: int = 0
    serial_size: int = 0
    serial_offset: int = 0
    forced_export: bool = False
    not_for_client: bool = False
    not_for_server: bool = False
    is_asset: bool = False
    first_export_dep: int = 0

    # Resolved
    class_name: str = ""
    super_name: str = ""


# ═══════════════════════════════════════════════════════════════════════
# UASSET PARSER
# ═══════════════════════════════════════════════════════════════════════

class UAssetParser:
    """Parse UE4.25 cooked .uasset + .uexp files."""

    MAGIC = 0x9E2A83C1  # PACKAGE_FILE_TAG (little-endian: C1 83 2A 9E)

    def __init__(self, uasset_path: str):
        self.path = Path(uasset_path)
        self.uexp_path = self.path.with_suffix('.uexp')
        self.names: List[str] = []
        self.imports: List[UImport] = []
        self.exports: List[UExport] = []
        self.header = {}
        self.uasset_data = b''
        self.uexp_data = b''

    def parse(self) -> bool:
        """Parse the .uasset file. Returns True on success."""
        try:
            with open(self.path, 'rb') as f:
                self.uasset_data = f.read()
            if self.uexp_path.exists():
                with open(self.uexp_path, 'rb') as f:
                    self.uexp_data = f.read()

            r = BinaryReader(self.uasset_data)
            self._parse_header(r)
            self._parse_names(r)
            self._parse_imports(r)
            self._parse_exports(r)
            self._resolve_names()
            return True
        except Exception as e:
            print(f"  Parse error: {e}")
            return False

    def _parse_header(self, r: BinaryReader):
        magic = r.read_u32()
        if magic != self.MAGIC:
            raise ValueError(f"Bad magic: 0x{magic:08X} (expected 0x{self.MAGIC:08X})")

        h = self.header
        h['legacy_version'] = r.read_i32()
        h['legacy_ue3_version'] = r.read_i32()
        h['file_version_ue4'] = r.read_i32()
        h['file_version_licensee'] = r.read_i32()

        # Custom versions (TArray<FCustomVersion>)
        custom_count = r.read_i32()
        for _ in range(custom_count):
            r.read_guid()  # 16 bytes GUID
            r.read_i32()   # version number

        h['total_header_size'] = r.read_i32()
        h['folder_name'] = r.read_fstring()
        h['package_flags'] = r.read_u32()
        h['name_count'] = r.read_i32()
        h['name_offset'] = r.read_i32()

        # UE4.25 has gatherable text data
        h['gatherable_text_count'] = r.read_i32()
        h['gatherable_text_offset'] = r.read_i32()

        h['export_count'] = r.read_i32()
        h['export_offset'] = r.read_i32()
        h['import_count'] = r.read_i32()
        h['import_offset'] = r.read_i32()
        h['depends_offset'] = r.read_i32()

        # Additional fields (version-dependent)
        h['soft_package_refs_count'] = r.read_i32()
        h['soft_package_refs_offset'] = r.read_i32()
        h['searchable_names_offset'] = r.read_i32()
        h['thumbnail_table_offset'] = r.read_i32()
        h['guid'] = r.read_guid()
        h['generations_count'] = r.read_i32()

        for _ in range(h['generations_count']):
            r.read_i32()  # export count
            r.read_i32()  # name count

        # Engine version
        h['saved_by_engine_version_major'] = r.read_u32() if r.remaining() >= 4 else 0

    def _parse_names(self, r: BinaryReader):
        """Parse the name table."""
        count = self.header['name_count']
        offset = self.header['name_offset']

        r.seek(offset)
        for _ in range(count):
            try:
                name = r.read_fstring()
                # Read hash bytes (UE4.12+: 2x uint16)
                if r.remaining() >= 4:
                    r.read_bytes(4)  # NonCasePreservingHash + CasePreservingHash
                self.names.append(name)
            except EOFError:
                break

    def _parse_imports(self, r: BinaryReader):
        """Parse import table."""
        count = self.header['import_count']
        offset = self.header['import_offset']

        r.seek(offset)
        for _ in range(count):
            try:
                imp = UImport()
                pkg_idx, pkg_num = r.read_fname()
                cls_idx, cls_num = r.read_fname()
                imp.outer_index = r.read_i32()
                obj_idx, obj_num = r.read_fname()

                imp.class_package = self._name(pkg_idx, pkg_num)
                imp.class_name = self._name(cls_idx, cls_num)
                imp.object_name = self._name(obj_idx, obj_num)
                self.imports.append(imp)
            except EOFError:
                break

    def _parse_exports(self, r: BinaryReader):
        """Parse export table (UE4.25 cooked format)."""
        count = self.header['export_count']
        offset = self.header['export_offset']
        ue4_ver = self.header.get('file_version_ue4', 522)
        # Cooked packages store version=0; default to UE4.25 (522)
        if ue4_ver == 0:
            ue4_ver = 522

        r.seek(offset)
        for _ in range(count):
            try:
                exp = UExport()
                exp.class_index = r.read_i32()
                exp.super_index = r.read_i32()

                # TemplateIndex: present in UE4.22+ (VER >= 518)
                if ue4_ver >= 518:
                    exp.template_index = r.read_i32()

                exp.outer_index = r.read_i32()

                obj_idx, obj_num = r.read_fname()
                exp.object_name = self._name(obj_idx, obj_num)

                exp.object_flags = r.read_u32()
                exp.serial_size = r.read_i64()
                exp.serial_offset = r.read_i64()

                exp.forced_export = r.read_i32() != 0
                exp.not_for_client = r.read_i32() != 0
                exp.not_for_server = r.read_i32() != 0

                r.read_guid()  # PackageGuid
                r.read_u32()   # PackageFlags

                # bNotForEditorGame (always present in UE4)
                r.read_i32()

                # bIsAsset (UE4.14+, VER >= 517)
                if ue4_ver >= 517:
                    exp.is_asset = r.read_i32() != 0

                # Preload dependencies (UE4.22+, VER >= 518)
                if ue4_ver >= 518:
                    exp.first_export_dep = r.read_i32()
                    r.read_i32()  # SerBefSer count
                    r.read_i32()  # CreateBefSer count
                    r.read_i32()  # SerBefCreate count
                    r.read_i32()  # CreateBefCreate count

                self.exports.append(exp)
            except EOFError:
                break

    def _name(self, idx: int, num: int = 0) -> str:
        """Resolve a name table index to string."""
        if 0 <= idx < len(self.names):
            name = self.names[idx]
            if num > 0:
                name += f"_{num-1}"
            return name
        return f"<name_{idx}>"

    def _resolve_index(self, idx: int) -> str:
        """Resolve a package index: positive=export, negative=import, 0=null."""
        if idx == 0:
            return "<null>"
        elif idx < 0:
            imp_idx = -idx - 1
            if 0 <= imp_idx < len(self.imports):
                return self.imports[imp_idx].object_name
            return f"<import_{imp_idx}>"
        else:
            exp_idx = idx - 1
            if 0 <= exp_idx < len(self.exports):
                return self.exports[exp_idx].object_name
            return f"<export_{exp_idx}>"

    def _resolve_names(self):
        """Resolve class/super names on exports."""
        for exp in self.exports:
            exp.class_name = self._resolve_index(exp.class_index)
            exp.super_name = self._resolve_index(exp.super_index)

    def get_serial_data(self, export: UExport) -> bytes:
        """Get serialized data for an export from .uexp."""
        if not self.uexp_data:
            return b''
        header_size = self.header.get('total_header_size', 0)
        uexp_offset = export.serial_offset - header_size
        if uexp_offset < 0 or uexp_offset >= len(self.uexp_data):
            return b''
        end = uexp_offset + export.serial_size
        return self.uexp_data[uexp_offset:end]

    def is_blueprint(self) -> bool:
        """Check if this asset is a Blueprint."""
        bp_classes = {
            'BlueprintGeneratedClass', 'WidgetBlueprintGeneratedClass',
            'AnimBlueprintGeneratedClass', 'Blueprint', 'WidgetBlueprint',
        }
        for imp in self.imports:
            if imp.class_name in bp_classes or imp.object_name in bp_classes:
                return True
        for exp in self.exports:
            if exp.class_name in bp_classes:
                return True
        return False

    def find_function_exports(self) -> List[UExport]:
        """Find exports that look like UFunction (Blueprint functions)."""
        funcs = []
        for exp in self.exports:
            # Functions typically have class "Function" or are parented to the BP class
            if exp.class_name == "Function" or "Function" in exp.object_name:
                funcs.append(exp)
            # Also look for ScriptBytecodeSize patterns in names
            elif any(kw in exp.object_name for kw in [
                "ExecuteUbergraph", "Setup", "NewMenu", "CreateActiveOption",
                "UpdateOptionHighlight", "ClearWidgets", "InputAction",
            ]):
                funcs.append(exp)
        return funcs


# ═══════════════════════════════════════════════════════════════════════
# KISMET BYTECODE DECOMPILER
# ═══════════════════════════════════════════════════════════════════════

class KismetDecompiler:
    """Basic Kismet bytecode decompiler for UE4.25."""

    def __init__(self, names: List[str]):
        self.names = names

    def decompile(self, bytecode: bytes, max_ops: int = 500) -> List[str]:
        """
        Decompile Kismet bytecode to pseudo-code.
        Returns list of decompiled lines.
        """
        lines = []
        r = BinaryReader(bytecode)
        ops = 0

        while r.remaining() > 0 and ops < max_ops:
            offset = r.tell()
            try:
                opcode = r.read_u8()
            except EOFError:
                break

            op_name = KISMET_OPCODES.get(opcode, f"UNKNOWN_0x{opcode:02X}")
            detail = ""

            try:
                detail = self._decode_op(opcode, r)
            except (EOFError, struct.error):
                detail = f"[decode error at +{r.tell()}]"

            line = f"  [{offset:04X}] {op_name}"
            if detail:
                line += f"  {detail}"
            lines.append(line)
            ops += 1

            if opcode == 0x53:  # EX_EndOfScript
                break

        if ops >= max_ops:
            lines.append(f"  ... (truncated at {max_ops} opcodes)")

        return lines

    def _decode_op(self, opcode: int, r: BinaryReader) -> str:
        """Decode operand data for known opcodes."""

        # Simple opcodes (no operands)
        if opcode in (0x0B, 0x17, 0x25, 0x26, 0x27, 0x28, 0x2A, 0x2D,
                       0x15, 0x16, 0x30, 0x32, 0x3A, 0x3C, 0x3E, 0x40, 0x53):
            return ""

        # Property reference (variable access)
        if opcode in (0x00, 0x01, 0x02, 0x48, 0x10):
            return self._read_property_ref(r)

        # Jump / branch
        if opcode == 0x06:  # EX_Jump
            target = r.read_u32()
            return f"-> 0x{target:04X}"

        if opcode == 0x07:  # EX_JumpIfNot
            target = r.read_u32()
            return f"-> 0x{target:04X} if NOT ..."

        if opcode == 0x18:  # EX_Skip
            skip = r.read_u32()
            return f"skip {skip} bytes"

        if opcode == 0x5B:  # EX_SkipOffsetConst
            skip = r.read_u32()
            return f"offset 0x{skip:04X}"

        # Push/Pop execution flow
        if opcode == 0x4C:  # EX_PushExecutionFlow
            addr = r.read_u32()
            return f"push 0x{addr:04X}"

        if opcode == 0x4D:  # EX_PopExecutionFlow
            return ""

        if opcode == 0x4F:  # EX_PopExecutionFlowIfNot
            return ""

        # Constants
        if opcode == 0x1D:  # EX_IntConst
            val = r.read_i32()
            return f"= {val}"

        if opcode == 0x1E:  # EX_FloatConst
            val = r.read_f32()
            return f"= {val:.4f}"

        if opcode == 0x1F:  # EX_StringConst
            return f'= "{self._read_cstring(r)}"'

        if opcode == 0x2C:  # EX_IntConstByte
            val = r.read_u8()
            return f"= {val}"

        if opcode == 0x24:  # EX_ByteConst
            val = r.read_u8()
            return f"= 0x{val:02X}"

        if opcode == 0x21:  # EX_NameConst
            return f"= FName({self._read_fname(r)})"

        if opcode == 0x23:  # EX_VectorConst
            x, y, z = r.read_f32(), r.read_f32(), r.read_f32()
            return f"= ({x:.2f}, {y:.2f}, {z:.2f})"

        if opcode == 0x22:  # EX_RotationConst
            p, y, rr = r.read_f32(), r.read_f32(), r.read_f32()
            return f"= Rot({p:.2f}, {y:.2f}, {rr:.2f})"

        if opcode == 0x35:  # EX_Int64Const
            val = r.read_i64()
            return f"= {val}L"

        # Object reference
        if opcode == 0x20:  # EX_ObjectConst
            idx = r.read_i64()
            return f"obj[{idx}]"

        if opcode == 0x67:  # EX_SoftObjectConst
            return ""  # complex

        # Function calls
        if opcode in (0x1B, 0x45):  # EX_VirtualFunction, EX_LocalVirtualFunction
            name = self._read_fname(r)
            return f"VirtualCall({name})"

        if opcode in (0x1C, 0x46):  # EX_FinalFunction, EX_LocalFinalFunction
            # FPackageIndex to the UFunction
            idx = r.read_i64()
            return f"FinalCall(func@{idx})"

        if opcode == 0x68:  # EX_CallMath
            idx = r.read_i64()
            return f"MathCall(func@{idx})"

        if opcode == 0x63:  # EX_CallMulticastDelegate
            idx = r.read_i64()
            return f"MulticastDelegate(func@{idx})"

        # Let (assignment)
        if opcode in (0x0F, 0x14, 0x5F, 0x60, 0x43, 0x44):
            return ""  # sub-expressions follow

        # Context
        if opcode in (0x19, 0x1A):
            return ""  # sub-expressions follow

        # Cast
        if opcode in (0x13, 0x2E, 0x52, 0x54, 0x55):
            idx = r.read_i64()
            return f"cast({idx})"

        # Struct const
        if opcode == 0x2F:  # EX_StructConst
            idx = r.read_i64()
            size = r.read_i32()
            return f"struct({idx}, size={size})"

        # Switch value
        if opcode == 0x69:  # EX_SwitchValue
            cases = r.read_u16() if r.remaining() >= 2 else 0
            return f"switch({cases} cases)"

        # Text const (complex)
        if opcode == 0x29:  # EX_TextConst
            text_type = r.read_u8()
            return f"TextConst(type={text_type})"

        # Bind delegate
        if opcode == 0x61:  # EX_BindDelegate
            return f"FName({self._read_fname(r)})"

        # Instance delegate
        if opcode == 0x4B:  # EX_InstanceDelegate
            return f"FName({self._read_fname(r)})"

        # Instrumentation
        if opcode == 0x6A:  # EX_InstrumentationEvent
            evt = r.read_u8()
            return f"event={evt}"

        # Return
        if opcode == 0x04:  # EX_Return
            return ""  # sub-expression follows

        # Assert
        if opcode == 0x09:  # EX_Assert
            line = r.read_u16() if r.remaining() >= 2 else 0
            debug = r.read_u8() if r.remaining() >= 1 else 0
            return f"line={line} debug={debug}"

        return ""

    def _read_property_ref(self, r: BinaryReader) -> str:
        """Read a serialized FProperty reference (FField path)."""
        # UE4.25 FFieldPath serialization:
        # int32 NumParts, then for each: FName + FName(FieldClass) + ...
        # Simplified: just try to read an FPackageIndex
        try:
            idx = r.read_i64()
            return f"prop@{idx}"
        except EOFError:
            return "prop@?"

    def _read_fname(self, r: BinaryReader) -> str:
        """Read a script-serialized FName (FScriptName)."""
        # In Kismet bytecode, FName is serialized as FScriptName:
        # int32 ComparisonIndex, int32 DisplayIndex, int32 Number
        # But in UE4.25 it's often just: FString (null-terminated ASCII)
        try:
            return self._read_cstring(r)
        except Exception:
            return "?"

    def _read_cstring(self, r: BinaryReader) -> str:
        """Read null-terminated ASCII string."""
        chars = []
        while r.remaining() > 0:
            b = r.read_u8()
            if b == 0:
                break
            chars.append(chr(b) if 32 <= b < 127 else '.')
        return ''.join(chars)


# ═══════════════════════════════════════════════════════════════════════
# STRING EXTRACTION (fallback for complex assets)
# ═══════════════════════════════════════════════════════════════════════

def extract_strings(data: bytes, min_len: int = 4) -> List[Tuple[int, str]]:
    """Extract readable ASCII strings from binary data."""
    results = []
    current = []
    start = 0

    for i, b in enumerate(data):
        if 32 <= b < 127:
            if not current:
                start = i
            current.append(chr(b))
        else:
            if len(current) >= min_len:
                results.append((start, ''.join(current)))
            current = []

    if len(current) >= min_len:
        results.append((start, ''.join(current)))

    return results


# ═══════════════════════════════════════════════════════════════════════
# SCAN DIRECTORY FOR BLUEPRINTS
# ═══════════════════════════════════════════════════════════════════════

def scan_directory(scan_dir: str, search: str = None):
    """Scan a directory for Blueprint .uasset files."""
    scan_path = Path(scan_dir)
    if not scan_path.exists():
        print(f"Directory not found: {scan_dir}")
        return

    uassets = list(scan_path.rglob("*.uasset"))
    print(f"═══ Scanning {len(uassets)} .uasset files ═══\n")

    blueprints = []
    datatables = []
    other = []

    for ua in sorted(uassets):
        parser = UAssetParser(str(ua))
        if not parser.parse():
            continue

        rel = ua.relative_to(scan_path)
        is_bp = parser.is_blueprint()

        if search:
            pattern = re.compile(search, re.IGNORECASE)
            found = any(pattern.search(n) for n in parser.names)
            if not found:
                continue

        if "DataTable" in str(rel) or "_DT" in ua.stem:
            datatables.append((rel, parser))
        elif is_bp:
            blueprints.append((rel, parser))
        else:
            other.append((rel, parser))

    print(f"── Blueprints ({len(blueprints)}) ──")
    for rel, p in blueprints:
        funcs = [e.object_name for e in p.exports if e.class_name == "Function"]
        print(f"  {rel}")
        if funcs:
            print(f"    Functions: {', '.join(funcs[:10])}")
            if len(funcs) > 10:
                print(f"    ... +{len(funcs)-10} more")
        print(f"    Names: {len(p.names)} | Imports: {len(p.imports)} | Exports: {len(p.exports)}")

    if datatables:
        print(f"\n── DataTables ({len(datatables)}) ──")
        for rel, p in datatables:
            print(f"  {rel}  ({len(p.names)} names, {len(p.exports)} exports)")

    if other:
        print(f"\n── Other ({len(other)}) ──")
        for rel, p in other:
            print(f"  {rel}  ({len(p.names)} names)")


# ═══════════════════════════════════════════════════════════════════════
# DUMP SINGLE ASSET
# ═══════════════════════════════════════════════════════════════════════

def dump_asset(uasset_path: str, names_only: bool = False, kismet_only: bool = False):
    """Full dump of a .uasset file."""
    parser = UAssetParser(uasset_path)
    if not parser.parse():
        return

    p = parser
    h = p.header

    if not names_only and not kismet_only:
        print(f"═══ {Path(uasset_path).name} ═══\n")
        print(f"── Header ──")
        print(f"  UE4 Version:   {h.get('file_version_ue4', '?')}")
        print(f"  Licensee:      {h.get('file_version_licensee', '?')}")
        print(f"  Header Size:   {h.get('total_header_size', '?')} bytes")
        print(f"  Folder:        {h.get('folder_name', '?')}")
        print(f"  Package Flags: 0x{h.get('package_flags', 0):08X}")
        print(f"  Names:         {h.get('name_count', 0)}")
        print(f"  Imports:       {h.get('import_count', 0)}")
        print(f"  Exports:       {h.get('export_count', 0)}")
        print(f"  Is Blueprint:  {p.is_blueprint()}")

    # ── Names ──
    if not kismet_only:
        print(f"\n── Name Table ({len(p.names)} entries) ──")
        for i, name in enumerate(p.names):
            print(f"  [{i:4d}] {name}")

    if names_only:
        return

    if not kismet_only:
        # ── Imports ──
        print(f"\n── Imports ({len(p.imports)} entries) ──")
        for i, imp in enumerate(p.imports):
            outer = p._resolve_index(imp.outer_index) if imp.outer_index != 0 else ""
            outer_str = f" (outer: {outer})" if outer else ""
            print(f"  [{i:3d}] {imp.class_package}::{imp.class_name} -> {imp.object_name}{outer_str}")

        # ── Exports ──
        print(f"\n── Exports ({len(p.exports)} entries) ──")
        for i, exp in enumerate(p.exports):
            print(f"  [{i:3d}] {exp.object_name}")
            print(f"         class={exp.class_name}  super={exp.super_name}")
            print(f"         offset={exp.serial_offset}  size={exp.serial_size}  flags=0x{exp.object_flags:08X}")

    # ── Kismet Bytecode ──
    func_exports = p.find_function_exports()
    if func_exports:
        print(f"\n── Kismet Bytecode ({len(func_exports)} functions) ──")
        decompiler = KismetDecompiler(p.names)

        for exp in func_exports:
            data = p.get_serial_data(exp)
            if not data:
                continue

            print(f"\n  ┌─ {exp.object_name} (class={exp.class_name}, {len(data)} bytes) ─┐")

            # Try to find Kismet bytecode in the serialized data.
            # Blueprint function data has: FStructProperty list, then script bytecode.
            # The bytecode often starts after a recognizable pattern.
            # Look for EX_EndOfScript (0x53) preceded by typical opcodes.

            # Strategy 1: Try decompiling from different offsets
            best_lines = []
            best_offset = 0

            for try_offset in range(0, min(len(data), 256), 4):
                chunk = data[try_offset:]
                lines = decompiler.decompile(chunk, max_ops=50)

                # Score: more recognized opcodes = better offset
                recognized = sum(1 for l in lines if "UNKNOWN" not in l)
                if recognized > len(best_lines):
                    best_lines = lines
                    best_offset = try_offset

            if best_lines:
                print(f"  │ Bytecode at +0x{best_offset:04X}:")
                for line in best_lines:
                    print(f"  │ {line}")
            else:
                print(f"  │ (no decodable bytecode found)")

            # Also show raw strings found in the function data
            strings = extract_strings(data, min_len=3)
            interesting = [s for s in strings if not s[1].startswith('\x00')]
            if interesting:
                print(f"  │")
                print(f"  │ Strings found:")
                for off, s in interesting[:30]:
                    print(f"  │   +0x{off:04X}: \"{s}\"")

            print(f"  └{'─' * 60}┘")
    else:
        # No function exports found — dump strings from .uexp
        if p.uexp_data:
            print(f"\n── Strings in .uexp ({len(p.uexp_data)} bytes) ──")
            strings = extract_strings(p.uexp_data, min_len=4)
            for off, s in strings[:100]:
                print(f"  +0x{off:04X}: \"{s}\"")
            if len(strings) > 100:
                print(f"  ... +{len(strings)-100} more strings")


# ═══════════════════════════════════════════════════════════════════════
# MAIN
# ═══════════════════════════════════════════════════════════════════════

def main():
    parser = argparse.ArgumentParser(description="RE4 VR UAsset Blueprint Decompiler")
    parser.add_argument("file", nargs="?", help="Path to .uasset file")
    parser.add_argument("--names", action="store_true", help="Dump name table only")
    parser.add_argument("--kismet", action="store_true", help="Dump Kismet bytecode only")
    parser.add_argument("--scan", type=str, help="Scan directory for Blueprint assets")
    parser.add_argument("--search", type=str, help="Search pattern (with --scan)")
    args = parser.parse_args()

    if args.scan:
        scan_directory(args.scan, args.search)
    elif args.file:
        dump_asset(args.file, names_only=args.names, kismet_only=args.kismet)
    else:
        parser.print_help()
        print("\nExamples:")
        print("  python tools/dump_uasset.py PAKS_extracted/VR4/Content/Blueprints/Debug/DebugMenu/DebugMenu.uasset")
        print("  python tools/dump_uasset.py --scan PAKS_extracted/VR4/Content/Blueprints/Debug/")
        print("  python tools/dump_uasset.py --scan PAKS_extracted/ --search 'DebugMenu'")


if __name__ == "__main__":
    main()
