#!/usr/bin/env python3
"""
RE4 VR Debug Menu PAK — Enumless Builder
==========================================
Injects a "Mods" entry point on the Main page DataTable.
NO enum modification. Fully enumless design.

The Blueprint dispatches via byte==byte comparison (EqualEqual_ByteByte).
The DebugMenuType enum is cosmetic metadata — NOT a hard constraint.
Lua DebugMenuAPI handles all custom pages dynamically via arrays.

Usage:
    python build_debug_pak.py              # Build .pak
    python build_debug_pak.py --deploy     # Build + push to device
    python build_debug_pak.py --dry-run    # Preview changes only
    python build_debug_pak.py --analyze    # Dump DataTable state
    python build_debug_pak.py --clean      # Remove build artifacts
"""

import json
import os
import sys
import shutil
import subprocess
import argparse
from pathlib import Path

# ═══════════════════════════════════════════════════════════════════════
# Paths
# ═══════════════════════════════════════════════════════════════════════

PROJECT_ROOT = Path(__file__).parent.parent
TOOLS_DIR    = PROJECT_ROOT / "tools"
UASSETGUI    = TOOLS_DIR / "bin" / "UAssetGUI.exe"

_REPAK_CANDIDATES = [
    TOOLS_DIR / "repak" / "repak.exe",
    Path(os.environ.get("USERPROFILE", "")) / ".cargo" / "bin" / "repak.exe",
]
REPAK = next((p for p in _REPAK_CANDIDATES if p.exists()), _REPAK_CANDIDATES[0])

EXTRACTED_DIR  = PROJECT_ROOT / "PAKS_extracted" / "VR4" / "Content" / "Blueprints" / "Debug" / "DebugMenu"
JSON_DIR       = PROJECT_ROOT / "PAKS_extracted" / "json_dumps"
BUILD_DIR      = PROJECT_ROOT / "PAKS_extracted" / "pak_build"
OUTPUT_PAK     = PROJECT_ROOT / "PAKS_extracted" / "DebugMenu_mod.pak"
CONTENT_PATH   = "VR4/Content/Blueprints/Debug/DebugMenu"
DEVICE_PAK_DIR = "/storage/emulated/0/Android/data/com.Armature.VR4/files/paks/"

ALL_ASSETS = [
    "DebugMenu", "DebugMenuWidget", "DebugVBoxList",
    "DebugOptionWidget", "DebugMenuOptions", "DebugMenuInterface",
    "DebugMenuType", "DebugOptionType", "DebugMenuOption",
    "DebugLevelList", "DebugRoomKeys_DT", "LevelListEntry",
    "RoomKeyEntry", "SupportedPlatforms",
    "Widget3DNoDepth", "Widget3DNoDepth_NotOccluded",
]

# ── ENUMLESS: ONLY modify DataTable. Enum stays UNTOUCHED. ──
MODIFIED_ASSETS = {"DebugMenuOptions"}

# ═══════════════════════════════════════════════════════════════════════
# DataTable Property FNames (from DebugMenuOption struct)
# ═══════════════════════════════════════════════════════════════════════

PROP_MENU     = "Menu_2_FC14ABF34A3BEEF2A2BA5EB6F8817E07"
PROP_OPTNAME  = "OptionName_18_B5CB955F4DCD6593D3C178A7B5B6E209"
PROP_OPTTYPE  = "OptionType_14_FC7672FD4D793F9A7FCB339D85D74420"
PROP_OPTLIST  = "OptionList_19_50B4C55340F0C79563450892A9763530"
PROP_MENULINK = "MenuLink_22_B72C81534C1A2B71B456A68CC3771F1D"
PROP_COMMAND  = "ConsoleCommand_25_F504660540357CBD90F7788B8B727173"

# Existing enum FNames we reference (NOT modified — already in game)
MAIN_PAGE_ENUM = "DebugMenuType::NewEnumerator0"   # byte value 1 = Main/Debug Menu
NONE_PAGE_ENUM = "DebugMenuType::NewEnumerator5"   # byte value 0 = None
OPT_SETTING    = "DebugOptionType::NewEnumerator0"  # Setting type


# ═══════════════════════════════════════════════════════════════════════
# Row Builder
# ═══════════════════════════════════════════════════════════════════════

def make_row(row_name, option_name, option_type, menu_enum,
             menu_link_enum=None, console_command=None):
    """Create a single DataTable row using existing enum FNames."""
    return {
        "$type": "UAssetAPI.PropertyTypes.Structs.StructPropertyData, UAssetAPI",
        "StructType": "DebugMenuOption",
        "SerializeNone": True,
        "StructGUID": "{00000000-0000-0000-0000-000000000000}",
        "SerializationControl": "NoExtension",
        "Operation": "None",
        "Name": row_name,
        "ArrayIndex": 0,
        "IsZero": False,
        "PropertyTagFlags": "None",
        "PropertyTagExtensions": "NoExtension",
        "Value": [
            {
                "$type": "UAssetAPI.PropertyTypes.Objects.BytePropertyData, UAssetAPI",
                "ByteType": "FName",
                "EnumType": "DebugMenuType",
                "EnumValue": menu_enum,
                "Name": PROP_MENU,
                "ArrayIndex": 0, "IsZero": False,
                "PropertyTagFlags": "None",
                "PropertyTagExtensions": "NoExtension"
            },
            {
                "$type": "UAssetAPI.PropertyTypes.Objects.StrPropertyData, UAssetAPI",
                "Name": PROP_OPTNAME,
                "ArrayIndex": 0, "IsZero": False,
                "PropertyTagFlags": "None",
                "PropertyTagExtensions": "NoExtension",
                "Value": option_name
            },
            {
                "$type": "UAssetAPI.PropertyTypes.Objects.BytePropertyData, UAssetAPI",
                "ByteType": "FName",
                "EnumType": "DebugOptionType",
                "EnumValue": option_type,
                "Name": PROP_OPTTYPE,
                "ArrayIndex": 0, "IsZero": False,
                "PropertyTagFlags": "None",
                "PropertyTagExtensions": "NoExtension"
            },
            {
                "$type": "UAssetAPI.PropertyTypes.Objects.ArrayPropertyData, UAssetAPI",
                "ArrayType": "StrProperty",
                "DummyStruct": None,
                "Name": PROP_OPTLIST,
                "ArrayIndex": 0, "IsZero": False,
                "PropertyTagFlags": "None",
                "PropertyTagExtensions": "NoExtension",
                "Value": []
            },
            {
                "$type": "UAssetAPI.PropertyTypes.Objects.BytePropertyData, UAssetAPI",
                "ByteType": "FName",
                "EnumType": "DebugMenuType",
                "EnumValue": menu_link_enum or NONE_PAGE_ENUM,
                "Name": PROP_MENULINK,
                "ArrayIndex": 0, "IsZero": False,
                "PropertyTagFlags": "None",
                "PropertyTagExtensions": "NoExtension"
            },
            {
                "$type": "UAssetAPI.PropertyTypes.Objects.StrPropertyData, UAssetAPI",
                "Name": PROP_COMMAND,
                "ArrayIndex": 0, "IsZero": False,
                "PropertyTagFlags": "None",
                "PropertyTagExtensions": "NoExtension",
                "Value": console_command
            }
        ]
    }


# ═══════════════════════════════════════════════════════════════════════
# DataTable Injection — Add "Mods" entry to Main page
# ═══════════════════════════════════════════════════════════════════════

def inject_mods_entry(dt_json):
    """Inject 'Mods' Setting item on the Main page (page 1).

    Uses OptionType=Setting (OPT_SETTING). When confirmed, Blueprint's
    DoAction won't match "Mods" against any page-1 string — safe no-op.
    Lua pre-hooks InputActionConfirm to intercept and enter mods mode.

    NO enum modification needed. We use existing FNames only.
    """
    table_data = dt_json["Exports"][0]["Table"]["Data"]
    name_map = dt_json.get("NameMap", [])

    # Add row name to NameMap if needed
    if "Main_Mods" not in name_map:
        name_map.append("Main_Mods")

    # Check if already injected (idempotent)
    for row in table_data:
        if row.get("Name") == "Main_Mods":
            print("  ℹ️  'Mods' entry already exists — skipping injection")
            return True

    # Create the Mods row: Setting type on Main page, no link, no command
    mods_row = make_row(
        row_name="Main_Mods",
        option_name="Mods",
        option_type=OPT_SETTING,
        menu_enum=MAIN_PAGE_ENUM,
    )

    # Insert as first Main page item (after the Back row)
    insert_idx = None
    for i, row in enumerate(table_data):
        for prop in row.get("Value", []):
            if prop.get("Name") == PROP_MENU and prop.get("EnumValue") == MAIN_PAGE_ENUM:
                insert_idx = i
                break
        if insert_idx is not None:
            break

    if insert_idx is not None:
        table_data.insert(insert_idx, mods_row)
        print(f"  ✅ Injected 'Mods' at position {insert_idx} (first on Main page)")
    else:
        table_data.append(mods_row)
        print(f"  ✅ Appended 'Mods' at end of DataTable")

    print(f"     DataTable: {len(table_data)} total rows (was {len(table_data) - 1})")
    print(f"     Enum: UNTOUCHED (enumless)")
    return True


# ═══════════════════════════════════════════════════════════════════════
# Build Pipeline
# ═══════════════════════════════════════════════════════════════════════

def load_json(name):
    """Load a JSON dump."""
    path = JSON_DIR / f"{name}.json"
    with open(path, "r", encoding="utf-8") as f:
        return json.load(f)


def verify_tools():
    """Check required tools exist."""
    ok = True
    if not UASSETGUI.exists():
        print(f"❌ UAssetGUI not found: {UASSETGUI}")
        ok = False
    if not REPAK.exists():
        print(f"❌ repak not found: {REPAK}")
        ok = False
    if not EXTRACTED_DIR.exists():
        print(f"❌ Extracted assets not found: {EXTRACTED_DIR}")
        ok = False
    return ok


def save_json_to_uasset(asset_name, data, output_dir):
    """Convert JSON → .uasset via UAssetGUI."""
    output_dir = Path(output_dir)
    output_dir.mkdir(parents=True, exist_ok=True)

    temp_json = PROJECT_ROOT / "PAKS_extracted" / f"_temp_{asset_name}.json"
    with open(temp_json, "w", encoding="utf-8") as f:
        json.dump(data, f)

    output_uasset = output_dir / f"{asset_name}.uasset"
    result = subprocess.run(
        [str(UASSETGUI), "fromjson", str(temp_json), str(output_uasset), "VER_UE4_25"],
        capture_output=True, text=True
    )

    if temp_json.exists():
        temp_json.unlink()

    if result.returncode != 0:
        print(f"  ❌ fromjson failed for {asset_name}: {result.stderr.strip()}")
        return False

    output_uexp = output_dir / f"{asset_name}.uexp"
    sizes = []
    if output_uasset.exists():
        sizes.append(f".uasset ({output_uasset.stat().st_size:,}B)")
    if output_uexp.exists():
        sizes.append(f".uexp ({output_uexp.stat().st_size:,}B)")
    print(f"  📦 Built {asset_name}: {', '.join(sizes)}")
    return True


def copy_unmodified(output_dir):
    """Copy unmodified assets from extracted PAK."""
    output_dir = Path(output_dir)
    output_dir.mkdir(parents=True, exist_ok=True)
    copied = 0
    for name in ALL_ASSETS:
        if name in MODIFIED_ASSETS:
            continue
        for ext in [".uasset", ".uexp"]:
            src = EXTRACTED_DIR / f"{name}{ext}"
            if src.exists():
                shutil.copy2(src, output_dir / f"{name}{ext}")
                copied += 1
    return copied


def pack_to_pak():
    """Pack build directory into .pak."""
    result = subprocess.run(
        [str(REPAK), "pack", "--version", "V9", str(BUILD_DIR)],
        capture_output=True, text=True
    )

    default_pak = Path(str(BUILD_DIR) + ".pak")
    if not default_pak.exists():
        print(f"  ❌ repak pack failed")
        if result.stderr:
            print(f"     {result.stderr.strip()}")
        return False

    if default_pak != OUTPUT_PAK:
        if OUTPUT_PAK.exists():
            OUTPUT_PAK.unlink()
        shutil.move(str(default_pak), str(OUTPUT_PAK))

    list_result = subprocess.run(
        [str(REPAK), "list", str(OUTPUT_PAK)],
        capture_output=True, text=True
    )
    file_count = len(list_result.stdout.strip().split("\n")) if list_result.stdout.strip() else 0

    print(f"\n{'='*60}")
    print(f"  ✅ PAK built: {OUTPUT_PAK.name}")
    print(f"     Size: {OUTPUT_PAK.stat().st_size:,} bytes")
    print(f"     Files: {file_count}")
    print(f"{'='*60}")
    return True


def deploy_pak():
    """Push PAK to device via ADB."""
    if not OUTPUT_PAK.exists():
        print(f"❌ PAK not found: {OUTPUT_PAK}")
        return False
    device_path = DEVICE_PAK_DIR + OUTPUT_PAK.name
    print(f"\n  Deploying → {device_path}")
    result = subprocess.run(
        ["adb", "push", str(OUTPUT_PAK), device_path],
        capture_output=True, text=True
    )
    if result.returncode == 0:
        print(f"  ✅ Deployed successfully")
        return True
    print(f"  ❌ ADB push failed: {result.stderr.strip()}")
    return False


def build(dry_run=False, do_deploy=False):
    """Main build pipeline."""
    print("=" * 60)
    print("  RE4 VR Debug Menu PAK — Enumless Builder")
    print("  DataTable injection only. NO enum changes.")
    print("=" * 60)

    if not verify_tools():
        return False

    # Step 1: Inject Mods entry into DataTable
    print("\n── Step 1: Inject 'Mods' entry into DebugMenuOptions ──")
    dt_data = load_json("DebugMenuOptions")
    if not inject_mods_entry(dt_data):
        return False

    if dry_run:
        print("\n  [DRY RUN] No files written.")
        return True

    # Step 2: Build assets
    print("\n── Step 2: Build Assets ──")
    if BUILD_DIR.exists():
        shutil.rmtree(BUILD_DIR)
    BUILD_DIR.mkdir(parents=True, exist_ok=True)

    output_dir = BUILD_DIR / CONTENT_PATH
    copied = copy_unmodified(output_dir)
    print(f"  📋 Copied {copied} unmodified asset files (including original DebugMenuType)")

    print(f"\n  Building DebugMenuOptions...")
    if not save_json_to_uasset("DebugMenuOptions", dt_data, output_dir):
        return False

    # Step 3: Pack
    print("\n── Step 3: Pack .pak ──")
    if not pack_to_pak():
        return False

    # Step 4: Deploy (optional)
    if do_deploy:
        print("\n── Step 4: Deploy ──")
        deploy_pak()

    print(f"\n🎉 Done!")
    print(f"   PAK:    {OUTPUT_PAK}")
    print(f"   Enum:   UNTOUCHED (enumless — Lua manages custom pages)")
    print(f"   Deploy: adb push {OUTPUT_PAK} {DEVICE_PAK_DIR}")
    return True


def analyze():
    """Dump the current DataTable state."""
    dt_data = load_json("DebugMenuOptions")
    table_data = dt_data["Exports"][0]["Table"]["Data"]

    print(f"── DebugMenuOptions DataTable ({len(table_data)} rows) ──\n")
    pages = {}
    for row in table_data:
        menu_val = "?"
        opt_name = "?"
        opt_type = "?"
        for prop in row.get("Value", []):
            name = prop.get("Name", "")
            if name == PROP_MENU:
                menu_val = prop.get("EnumValue", "?")
            elif name == PROP_OPTNAME:
                opt_name = prop.get("Value", "?")
            elif name == PROP_OPTTYPE:
                opt_type = prop.get("EnumValue", "?")
        pages.setdefault(menu_val, []).append((opt_name, opt_type, row.get("Name", "?")))

    for page_enum in sorted(pages.keys()):
        items = pages[page_enum]
        print(f"  Page: {page_enum} ({len(items)} items)")
        for name, otype, rname in items:
            ot_short = {"DebugOptionType::NewEnumerator0": "SET",
                        "DebugOptionType::NewEnumerator1": "LNK",
                        "DebugOptionType::NewEnumerator3": "TOG",
                        "DebugOptionType::NewEnumerator5": "SEL",
                        "DebugOptionType::NewEnumerator7": "CMD"}.get(otype, "???")
            print(f"    {ot_short} {name:45s} [{rname}]")
        print()

    has_mods = any(row.get("Name") == "Main_Mods" for row in table_data)
    print(f"  Mods entry: {'✅ Present' if has_mods else '❌ Missing'}")


def clean():
    """Remove build artifacts."""
    removed = 0
    if BUILD_DIR.exists():
        shutil.rmtree(BUILD_DIR)
        print(f"  🗑️  Removed {BUILD_DIR}")
        removed += 1
    if OUTPUT_PAK.exists():
        OUTPUT_PAK.unlink()
        print(f"  🗑️  Removed {OUTPUT_PAK}")
        removed += 1
    if removed == 0:
        print("  Nothing to clean.")


def main():
    parser = argparse.ArgumentParser(
        description="RE4 VR Debug Menu PAK — Enumless Builder",
        formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--deploy", action="store_true", help="Build + push to device")
    parser.add_argument("--dry-run", action="store_true", help="Preview changes only")
    parser.add_argument("--analyze", action="store_true", help="Dump DataTable state")
    parser.add_argument("--clean", action="store_true", help="Remove build artifacts")
    args = parser.parse_args()

    if args.clean:
        clean()
        return
    if args.analyze:
        analyze()
        return

    success = build(dry_run=args.dry_run, do_deploy=args.deploy)
    sys.exit(0 if success else 1)


if __name__ == "__main__":
    main()
