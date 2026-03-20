#!/usr/bin/env python3
"""
Kismet Bytecode Decompiler for UE4.25 Blueprints
=================================================
Reads UAssetGUI JSON exports and produces human-readable pseudo-code.
Supports all expression types found in RE4 VR Debug Menu Blueprints.

Usage:
    python kismet_decompiler.py <input.json> [--output <output.txt>] [--function <name>] [--list]
    python kismet_decompiler.py --all  (decompile all JSON files in json_dumps/)
"""

import json
import sys
import os
import argparse
from pathlib import Path


class KismetDecompiler:
    """Decompiles Kismet bytecode from UAssetGUI JSON into pseudo-code."""

    def __init__(self, asset_data, verbose=False):
        self.asset = asset_data
        self.verbose = verbose
        self.name_map = asset_data.get("NameMap", [])
        self.imports = asset_data.get("Imports", [])
        self.exports = asset_data.get("Exports", [])
        self.indent = 0

    # ── Utility ──────────────────────────────────────────────────────────

    def _type(self, expr):
        """Extract short type name from $type field."""
        if not expr or "$type" not in expr:
            return "Unknown"
        return expr["$type"].split(".")[-1].split(",")[0]

    def _indent(self):
        return "    " * self.indent

    def _resolve_export(self, idx):
        """Resolve export index (1-based) to object name."""
        if isinstance(idx, int) and 1 <= idx <= len(self.exports):
            return self.exports[idx - 1].get("ObjectName", f"Export_{idx}")
        return f"Export_{idx}"

    def _resolve_import(self, idx):
        """Resolve import index (negative, -1 based) to object name."""
        if isinstance(idx, int) and idx < 0:
            real_idx = (-idx) - 1
            if 0 <= real_idx < len(self.imports):
                imp = self.imports[real_idx]
                return imp.get("ObjectName", f"Import_{idx}")
        return f"Ref_{idx}"

    def _resolve_package_index(self, idx):
        """Resolve a FPackageIndex: positive = export, negative = import, 0 = null."""
        if isinstance(idx, int):
            if idx > 0:
                return self._resolve_export(idx)
            elif idx < 0:
                return self._resolve_import(idx)
        return "None"

    def _get_property_path(self, prop_pointer):
        """Extract property path string from KismetPropertyPointer."""
        if not prop_pointer:
            return "???"
        new = prop_pointer.get("New")
        if new:
            path = new.get("Path", [])
            if path:
                return ".".join(path)
        old = prop_pointer.get("Old")
        if old:
            return self._resolve_package_index(old)
        return "???"

    # ── Expression Decompilation ─────────────────────────────────────────

    def decompile_expr(self, expr):
        """Decompile a single Kismet expression to string."""
        if expr is None:
            return "null"

        t = self._type(expr)
        handler = getattr(self, f"_expr_{t}", None)
        if handler:
            return handler(expr)

        if self.verbose:
            return f"/* UNHANDLED: {t} */"
        return f"<{t}>"

    def _expr_EX_LocalVariable(self, expr):
        return self._get_property_path(expr.get("Variable"))

    def _expr_EX_InstanceVariable(self, expr):
        return f"self.{self._get_property_path(expr.get('Variable'))}"

    def _expr_EX_LocalOutVariable(self, expr):
        return self._get_property_path(expr.get("Variable"))

    def _expr_EX_Nothing(self, expr):
        return ""

    def _expr_EX_EndOfScript(self, expr):
        return "// EndOfScript"

    def _expr_EX_Return(self, expr):
        ret = expr.get("ReturnExpression")
        val = self.decompile_expr(ret)
        if val and val.strip():
            return f"return {val}"
        return "return"

    def _expr_EX_Self(self, expr):
        return "self"

    def _expr_EX_True(self, expr):
        return "true"

    def _expr_EX_False(self, expr):
        return "false"

    def _expr_EX_NoObject(self, expr):
        return "None"

    def _expr_EX_IntConst(self, expr):
        return str(expr.get("Value", 0))

    def _expr_EX_FloatConst(self, expr):
        return str(expr.get("Value", 0.0))

    def _expr_EX_ByteConst(self, expr):
        return str(expr.get("Value", 0))

    def _expr_EX_StringConst(self, expr):
        val = expr.get("Value", "")
        return f'"{val}"'

    def _expr_EX_NameConst(self, expr):
        val = expr.get("Value")
        if isinstance(val, dict):
            return f"FName(\"{val.get('Value', val)}\")"
        return f"FName(\"{val}\")"

    def _expr_EX_TextConst(self, expr):
        val = expr.get("Value")
        if isinstance(val, dict):
            # FScriptText has multiple history types
            text_type = val.get("TextLiteralType", "")
            # Try to extract the actual string from various FText formats
            invariant = val.get("InvariantLiteralString")
            if isinstance(invariant, dict):
                # Nested expression (e.g. EX_StringConst inside FScriptText)
                inner_val = invariant.get("Value", "")
                if inner_val:
                    return f'FText("{inner_val}")'
            literal = val.get("LiteralString")
            if isinstance(literal, dict):
                inner_val = literal.get("Value", "")
                if inner_val:
                    return f'FText("{inner_val}")'
            src_string = val.get("SourceString", "")
            if src_string:
                return f'FText("{src_string}")'
            local_str = val.get("LocalizedString", "")
            if local_str:
                return f'FText("{local_str}")'
            if "Empty" in str(text_type):
                return 'FText("")'
        return f'FText("{val}")'

    def _expr_EX_ObjectConst(self, expr):
        val = expr.get("Value")
        if isinstance(val, int):
            return self._resolve_package_index(val)
        return str(val)

    def _expr_EX_VectorConst(self, expr):
        val = expr.get("Value", {})
        x = val.get("X", 0)
        y = val.get("Y", 0)
        z = val.get("Z", 0)
        return f"FVector({x}, {y}, {z})"

    def _expr_EX_RotationConst(self, expr):
        val = expr.get("Value", {})
        p = val.get("Pitch", 0)
        y = val.get("Yaw", 0)
        r = val.get("Roll", 0)
        return f"FRotator({p}, {y}, {r})"

    def _expr_EX_StructConst(self, expr):
        struct = expr.get("Struct")
        struct_name = self._resolve_package_index(struct) if isinstance(struct, int) else str(struct)
        props = expr.get("Value", [])
        fields = []
        for p in props:
            fields.append(self.decompile_expr(p))
        return f"{struct_name}({', '.join(fields)})"

    def _expr_EX_SkipOffsetConst(self, expr):
        return f"/* skip_offset={expr.get('Value', 0)} */"

    # ── Assignments ──────────────────────────────────────────────────────

    def _expr_EX_Let(self, expr):
        var = self.decompile_expr(expr.get("Variable"))
        val = self.decompile_expr(expr.get("Expression"))
        return f"{var} = {val}"

    def _expr_EX_LetBool(self, expr):
        var = self.decompile_expr(expr.get("VariableExpression"))
        val = self.decompile_expr(expr.get("AssignmentExpression"))
        return f"{var} = {val}"

    def _expr_EX_LetObj(self, expr):
        var = self.decompile_expr(expr.get("VariableExpression"))
        val = self.decompile_expr(expr.get("AssignmentExpression"))
        return f"{var} = {val}"

    def _expr_EX_LetValueOnPersistentFrame(self, expr):
        dest = self._get_property_path(expr.get("DestinationProperty"))
        val = self.decompile_expr(expr.get("AssignmentExpression"))
        return f"PersistentFrame.{dest} = {val}"

    # ── Context / Member Access ──────────────────────────────────────────

    def _expr_EX_Context(self, expr):
        obj = self.decompile_expr(expr.get("ObjectExpression"))
        ctx = self.decompile_expr(expr.get("ContextExpression"))
        if obj == "self":
            return ctx
        return f"{obj}.{ctx}"

    def _expr_EX_InterfaceContext(self, expr):
        return self.decompile_expr(expr.get("InterfaceValue"))

    def _expr_EX_StructMemberContext(self, expr):
        struct = self.decompile_expr(expr.get("StructExpression"))
        prop = self._get_property_path(expr.get("StructMemberExpression"))
        return f"{struct}.{prop}"

    # ── Function Calls ───────────────────────────────────────────────────

    def _decompile_params(self, params):
        """Decompile a parameter list, filtering out empty/nothing entries."""
        if not params:
            return ""
        args = []
        for p in params:
            val = self.decompile_expr(p)
            if val and val != "// EndOfScript":
                args.append(val)
        return ", ".join(args)

    def _expr_EX_VirtualFunction(self, expr):
        name = expr.get("VirtualFunctionName", "???")
        params = self._decompile_params(expr.get("Parameters"))
        return f"{name}({params})"

    def _expr_EX_LocalVirtualFunction(self, expr):
        name = expr.get("VirtualFunctionName", "???")
        params = self._decompile_params(expr.get("Parameters"))
        return f"{name}({params})"

    def _expr_EX_FinalFunction(self, expr):
        stack_node = expr.get("StackNode")
        name = self._resolve_package_index(stack_node) if isinstance(stack_node, int) else str(stack_node)
        params = self._decompile_params(expr.get("Parameters"))
        return f"{name}({params})"

    def _expr_EX_LocalFinalFunction(self, expr):
        stack_node = expr.get("StackNode")
        name = self._resolve_package_index(stack_node) if isinstance(stack_node, int) else str(stack_node)
        params = self._decompile_params(expr.get("Parameters"))
        return f"{name}({params})"

    def _expr_EX_CallMath(self, expr):
        stack_node = expr.get("StackNode")
        name = self._resolve_package_index(stack_node) if isinstance(stack_node, int) else str(stack_node)
        params = self._decompile_params(expr.get("Parameters"))
        # Simplify common math ops
        return self._simplify_math(name, params, expr.get("Parameters", []))

    def _simplify_math(self, name, params_str, params_list):
        """Simplify common UE4 math function calls into operators."""
        simple_ops = {
            "NotEqual_IntInt": ("!=", 2),
            "EqualEqual_IntInt": ("==", 2),
            "Equal_IntInt": ("==", 2),
            "Less_IntInt": ("<", 2),
            "LessEqual_IntInt": ("<=", 2),
            "Greater_IntInt": (">", 2),
            "GreaterEqual_IntInt": (">=", 2),
            "Add_IntInt": ("+", 2),
            "Subtract_IntInt": ("-", 2),
            "Multiply_IntInt": ("*", 2),
            "Divide_IntInt": ("/", 2),
            "Percent_IntInt": ("%", 2),
            "NotEqual_StrStr": ("!=", 2),
            "EqualEqual_StrStr": ("==", 2),
            "NotEqual_NameName": ("!=", 2),
            "EqualEqual_NameName": ("==", 2),
            "EqualEqual_BoolBool": ("==", 2),
            "NotEqual_BoolBool": ("!=", 2),
            "Not_PreBool": ("!", 1),
            "BooleanAND": ("&&", 2),
            "BooleanOR": ("||", 2),
            "NotEqual_ObjectObject": ("!=", 2),
            "EqualEqual_ObjectObject": ("==", 2),
            "EqualEqual_ByteByte": ("==", 2),
            "NotEqual_ByteByte": ("!=", 2),
            "Less_FloatFloat": ("<", 2),
            "Greater_FloatFloat": (">", 2),
            "Add_FloatFloat": ("+", 2),
            "Subtract_FloatFloat": ("-", 2),
            "Multiply_FloatFloat": ("*", 2),
            "Divide_FloatFloat": ("/", 2),
        }

        if name in simple_ops:
            op, arity = simple_ops[name]
            args = [self.decompile_expr(p) for p in params_list if self._type(p) != "EX_Nothing"]
            if arity == 1 and len(args) >= 1:
                return f"({op}{args[0]})"
            elif arity == 2 and len(args) >= 2:
                return f"({args[0]} {op} {args[1]})"

        conv_ops = {
            "Conv_IntToString": "ToString",
            "Conv_FloatToString": "ToString",
            "Conv_BoolToString": "ToString",
            "Conv_NameToString": "ToString",
            "Conv_StringToName": "ToName",
            "Conv_IntToFloat": "float",
            "Conv_FloatToInt": "int",
            "Conv_IntToByte": "byte",
            "Conv_ByteToInt": "int",
            "Conv_BoolToInt": "int",
            "Conv_StringToText": "FText",
            "Conv_TextToString": "ToString",
        }

        if name in conv_ops:
            args = [self.decompile_expr(p) for p in params_list if self._type(p) != "EX_Nothing"]
            if args:
                return f"{conv_ops[name]}({args[0]})"

        return f"{name}({params_str})"

    # ── Control Flow ─────────────────────────────────────────────────────

    def _expr_EX_Jump(self, expr):
        offset = expr.get("CodeOffset", 0)
        return f"goto @{offset}"

    def _expr_EX_JumpIfNot(self, expr):
        cond = self.decompile_expr(expr.get("BooleanExpression"))
        offset = expr.get("CodeOffset", 0)
        return f"if (!({cond})) goto @{offset}"

    def _expr_EX_PushExecutionFlow(self, expr):
        offset = expr.get("PushingAddress", 0)
        return f"// push_flow @{offset}"

    def _expr_EX_PopExecutionFlow(self, expr):
        return "// pop_flow (end branch)"

    def _expr_EX_PopExecutionFlowIfNot(self, expr):
        cond = self.decompile_expr(expr.get("BooleanExpression"))
        return f"if (!({cond})) pop_flow  // abort branch"

    def _expr_EX_ComputedJump(self, expr):
        target = self.decompile_expr(expr.get("CodeOffsetExpression"))
        return f"goto computed({target})"

    # ── Switch ───────────────────────────────────────────────────────────

    def _expr_EX_SwitchValue(self, expr):
        index = self.decompile_expr(expr.get("IndexTerm"))
        default = self.decompile_expr(expr.get("DefaultTerm"))
        cases = expr.get("Cases", [])
        lines = [f"switch ({index}) {{"]
        for case in cases:
            case_val = self.decompile_expr(case.get("CaseIndexValueTerm"))
            case_result = self.decompile_expr(case.get("CaseTerm"))
            lines.append(f"    case {case_val}: {case_result}")
        lines.append(f"    default: {default}")
        lines.append("}")
        return "\n".join(lines)

    # ── Array Operations ─────────────────────────────────────────────────

    def _expr_EX_SetArray(self, expr):
        assign = self.decompile_expr(expr.get("AssigningProperty"))
        elements = expr.get("Elements", [])
        vals = [self.decompile_expr(e) for e in elements]
        return f"{assign} = [{', '.join(vals)}]"

    def _expr_EX_ArrayConst(self, expr):
        elements = expr.get("Elements", [])
        vals = [self.decompile_expr(e) for e in elements]
        return f"[{', '.join(vals)}]"

    def _expr_EX_SetMap(self, expr):
        elements = expr.get("Elements", [])
        pairs = []
        for i in range(0, len(elements) - 1, 2):
            k = self.decompile_expr(elements[i])
            v = self.decompile_expr(elements[i + 1])
            pairs.append(f"{k}: {v}")
        return f"{{{', '.join(pairs)}}}"

    # ── Casts ────────────────────────────────────────────────────────────

    def _expr_EX_PrimitiveCast(self, expr):
        inner = self.decompile_expr(expr.get("Target"))
        cast_type = expr.get("ConversionType", "")
        return f"({cast_type}){inner}" if cast_type else inner

    def _expr_EX_DynamicCast(self, expr):
        cls = expr.get("ClassPtr")
        cls_name = self._resolve_package_index(cls) if isinstance(cls, int) else str(cls)
        target = self.decompile_expr(expr.get("TargetExpression"))
        return f"Cast<{cls_name}>({target})"

    def _expr_EX_ObjToInterfaceCast(self, expr):
        cls = expr.get("ClassPtr")
        cls_name = self._resolve_package_index(cls) if isinstance(cls, int) else str(cls)
        target = self.decompile_expr(expr.get("Target"))
        return f"InterfaceCast<{cls_name}>({target})"

    # ── Delegates ────────────────────────────────────────────────────────

    def _expr_EX_BindDelegate(self, expr):
        func_name = expr.get("FunctionName", "???")
        delegate = self.decompile_expr(expr.get("Delegate"))
        obj = self.decompile_expr(expr.get("ObjectTerm"))
        return f"BindDelegate({delegate}, {obj}.{func_name})"

    # ── Main Decompile Logic ─────────────────────────────────────────────

    def decompile_function(self, func_export):
        """Decompile a FunctionExport into readable pseudo-code."""
        name = func_export.get("ObjectName", "Unknown")
        bytecode = func_export.get("ScriptBytecode", [])
        func_flags_raw = func_export.get("FunctionFlags", 0)

        # Parse function metadata
        super_idx = func_export.get("SuperIndex", 0)

        lines = []
        lines.append(f"// ═══════════════════════════════════════════════════════════════")
        lines.append(f"// Function: {name}")
        lines.append(f"// Bytecode expressions: {len(bytecode)}")
        if func_flags_raw:
            flags = self._parse_function_flags(func_flags_raw)
            if flags:
                lines.append(f"// Flags: {', '.join(flags)}")
        lines.append(f"// ═══════════════════════════════════════════════════════════════")
        lines.append(f"function {name}()")
        lines.append("{")

        self.indent = 1
        for i, expr in enumerate(bytecode):
            code = self.decompile_expr(expr)
            if code and code.strip():
                # Handle multi-line outputs (switches)
                for line in code.split("\n"):
                    lines.append(f"{self._indent()}{line}")

        lines.append("}")
        lines.append("")
        return "\n".join(lines)

    def _parse_function_flags(self, flags):
        """Parse EFunctionFlags bitmask."""
        if not isinstance(flags, int):
            return []
        flag_names = []
        flag_map = {
            0x00000001: "Final",
            0x00000400: "BlueprintAuthorityOnly",
            0x00000800: "BlueprintCosmetic",
            0x00004000: "Net",
            0x00010000: "NetReliable",
            0x00020000: "NetRequest",
            0x00040000: "Exec",
            0x00080000: "Native",
            0x00100000: "Event",
            0x00200000: "NetResponse",
            0x00400000: "Static",
            0x00800000: "NetMulticast",
            0x01000000: "UbergraphFunction",
            0x04000000: "HasOutParms",
            0x08000000: "HasDefaults",
            0x10000000: "DLLImport",
            0x20000000: "BlueprintCallable",
            0x40000000: "BlueprintEvent",
            0x80000000: "BlueprintPure",
        }
        for bit, name in flag_map.items():
            if flags & bit:
                flag_names.append(name)
        return flag_names

    def decompile_all_functions(self):
        """Decompile all FunctionExports in the asset."""
        output = []
        class_name = "Unknown"

        # Find the ClassExport for the header
        for exp in self.exports:
            if "ClassExport" in exp.get("$type", ""):
                class_name = exp.get("ObjectName", "Unknown")
                break

        output.append(f"// ╔═══════════════════════════════════════════════════════════════╗")
        output.append(f"// ║  Decompiled Blueprint: {class_name:<38}║")
        output.append(f"// ║  Generated by Kismet Decompiler                              ║")
        output.append(f"// ║  Source: UAssetGUI JSON export → Pseudo-code                 ║")
        output.append(f"// ╚═══════════════════════════════════════════════════════════════╝")
        output.append("")

        # List all functions first
        functions = [e for e in self.exports if "FunctionExport" in e.get("$type", "")]
        output.append(f"// Functions ({len(functions)}):")
        for f in functions:
            bc = f.get("ScriptBytecode", [])
            output.append(f"//   - {f.get('ObjectName', '?')} ({len(bc)} expressions)")
        output.append("")

        # Decompile each
        for func in functions:
            output.append(self.decompile_function(func))

        return "\n".join(output)

    def get_function_names(self):
        """Return list of function names."""
        return [e.get("ObjectName", "?") for e in self.exports
                if "FunctionExport" in e.get("$type", "")]

    def decompile_single_function(self, name):
        """Decompile a single function by name."""
        for exp in self.exports:
            if "FunctionExport" in exp.get("$type", "") and exp.get("ObjectName") == name:
                return self.decompile_function(exp)
        return f"// Function '{name}' not found"


def decompile_json_file(json_path, output_path=None, function_name=None, list_only=False):
    """Load a UAssetGUI JSON file and decompile it."""
    print(f"Loading: {json_path}")
    with open(json_path, "r", encoding="utf-8") as f:
        data = json.load(f)

    decomp = KismetDecompiler(data)

    if list_only:
        names = decomp.get_function_names()
        if names:
            print(f"Functions in {Path(json_path).stem}:")
            for n in names:
                print(f"  - {n}")
        else:
            print(f"No functions found in {Path(json_path).stem}")
        return

    if function_name:
        result = decomp.decompile_single_function(function_name)
    else:
        result = decomp.decompile_all_functions()

    if output_path:
        os.makedirs(os.path.dirname(output_path), exist_ok=True)
        with open(output_path, "w", encoding="utf-8") as f:
            f.write(result)
        print(f"Written: {output_path} ({len(result)} chars)")
    else:
        print(result)


def main():
    parser = argparse.ArgumentParser(description="Kismet Bytecode Decompiler")
    parser.add_argument("input", nargs="?", help="Input JSON file (from UAssetGUI tojson)")
    parser.add_argument("--output", "-o", help="Output file path")
    parser.add_argument("--function", "-f", help="Decompile only this function")
    parser.add_argument("--list", "-l", action="store_true", help="List function names only")
    parser.add_argument("--all", "-a", action="store_true", help="Decompile all JSON files in json_dumps/")
    args = parser.parse_args()

    if args.all:
        # Process all JSON files
        json_dir = Path(__file__).parent.parent / "PAKS_extracted" / "json_dumps"
        output_dir = Path(__file__).parent.parent / "PAKS_extracted" / "decompiled"
        output_dir.mkdir(parents=True, exist_ok=True)

        json_files = sorted(json_dir.glob("*.json"))
        if not json_files:
            print(f"No JSON files found in {json_dir}")
            return

        print(f"Found {len(json_files)} JSON files to decompile")
        for jf in json_files:
            out = output_dir / f"{jf.stem}.txt"
            try:
                decompile_json_file(str(jf), str(out))
            except Exception as e:
                print(f"ERROR processing {jf.name}: {e}")

        print(f"\nAll decompiled files written to: {output_dir}")
        return

    if not args.input:
        parser.print_help()
        return

    decompile_json_file(args.input, args.output, args.function, args.list)


if __name__ == "__main__":
    main()
