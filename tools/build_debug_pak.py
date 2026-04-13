#!/usr/bin/env python3
"""
RE4 VR Debug Menu PAK — Blueprint Uncooker & Rebuilder
=======================================================
Modifies REAL Blueprint assets (DataTable + widget CDO defaults),
then repacks into a .pak that overrides the stock debug menu.

The modloader's pak_mounter hooks FPakPlatformFile::Mount early in
engine boot, mounting this PAK at priority 1000+ so it overrides
all stock debug menu assets BEFORE the first level loads.

Modifications:
  1. DebugMenuOptions DataTable — inject "Mods" entry on Main page
  2. DebugVBoxList CDO          — increase MaxVisible from 5 → 20
  3. All other assets           — copied verbatim from stock PAK

The Lua DebugMenuAPI v21 cooperates with these PAK changes:
  - PostHook DoAction detects "Mods" confirm on Main → NewMenu(100)
  - PostHook NewMenu appends custom items after Blueprint builds a page
  - Blueprint handles all native input, scrolling, highlighting, Back

Usage:
    python build_debug_pak.py              # Build .pak
    python build_debug_pak.py --deploy     # Build + push to device
    python build_debug_pak.py --dry-run    # Preview changes only
    python build_debug_pak.py --analyze    # Dump DataTable state
    python build_debug_pak.py --verify     # Verify PAK contents
    python build_debug_pak.py --clean      # Remove build artifacts
"""

import json
import os
import sys
import shutil
import subprocess
import argparse
import hashlib
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
OUTPUT_PAK     = PROJECT_ROOT / "PAKS" / "DebugMenu_P.pak"  # _P suffix for higher priority
CONTENT_PATH   = "VR4/Content/Blueprints/Debug/DebugMenu"
DEVICE_PAK_DIR = "/storage/emulated/0/Android/data/com.Armature.VR4/files/paks/"

# All Blueprint assets in the DebugMenu folder
ALL_ASSETS = [
    "DebugMenu", "DebugMenuWidget", "DebugVBoxList",
    "DebugOptionWidget", "DebugMenuOptions", "DebugMenuInterface",
    "DebugMenuType", "DebugOptionType", "DebugMenuOption",
    "DebugLevelList", "DebugRoomKeys_DT", "LevelListEntry",
    "RoomKeyEntry", "SupportedPlatforms",
    "Widget3DNoDepth", "Widget3DNoDepth_NotOccluded",
]

# Assets we modify — everything else is copied verbatim
MODIFIED_ASSETS = {"DebugMenuOptions", "DebugVBoxList", "DebugMenuWidget"}

# ═══════════════════════════════════════════════════════════════════════
# DataTable Property FNames (from DebugMenuOption struct reflection)
# ═══════════════════════════════════════════════════════════════════════

PROP_MENU     = "Menu_2_FC14ABF34A3BEEF2A2BA5EB6F8817E07"
PROP_OPTNAME  = "OptionName_18_B5CB955F4DCD6593D3C178A7B5B6E209"
PROP_OPTTYPE  = "OptionType_14_FC7672FD4D793F9A7FCB339D85D74420"
PROP_OPTLIST  = "OptionList_19_50B4C55340F0C79563450892A9763530"
PROP_MENULINK = "MenuLink_22_B72C81534C1A2B71B456A68CC3771F1D"
PROP_COMMAND  = "ConsoleCommand_25_F504660540357CBD90F7788B8B727173"

# Existing enum FNames — NOT modified, already in the game.
# CRITICAL: Only use FNames that ALREADY exist in the asset's NameMap.
# The stock DataTable uses DebugOptionType values 0, 1, 3, 5, 7 only.
# NewEnumerator2, 4, 6 do NOT exist in the NameMap → UAssetGUI will crash.
MAIN_PAGE_ENUM = "DebugMenuType::NewEnumerator0"   # byte 1 = Main/Debug Menu
NONE_PAGE_ENUM = "DebugMenuType::NewEnumerator5"   # byte 0 = None
OPT_SETTING    = "DebugOptionType::NewEnumerator0"  # Setting type (cycles SettingList)
OPT_LINK       = "DebugOptionType::NewEnumerator1"  # Link type (navigates to MenuLink page)
OPT_TOGGLE     = "DebugOptionType::NewEnumerator3"  # Toggle type
OPT_SELECT     = "DebugOptionType::NewEnumerator5"  # Select type
OPT_COMMAND    = "DebugOptionType::NewEnumerator7"  # Console command type

# DebugVBoxList CDO modifications
VBOXLIST_MAX_VISIBLE = 20  # Stock: 5 — too few for VR. NewMenu overrides per-page anyway.

# DebugMenuWidget SizeBox modifications
SIZEBOX_HEIGHT = 2000.0    # Stock: 1200 — clips items when many are visible (1200/50px ≈ 24 max)


# ═══════════════════════════════════════════════════════════════════════
# DataTable Row Builder
# ═══════════════════════════════════════════════════════════════════════

def make_row(row_name, option_name, option_type, menu_enum,
             menu_link_enum=None, console_command=None, option_list=None):
    """Create a single DataTable row using existing enum FNames.

    Parameters:
        row_name:       Unique row identifier (FName)
        option_name:    Display text in the debug menu
        option_type:    DebugOptionType enum FName
        menu_enum:      DebugMenuType enum FName for which page this appears on
        menu_link_enum: DebugMenuType FName for page navigation (optional)
        console_command: Console command string (optional)
        option_list:    List of strings for SettingList (optional)
    """
    opt_list_values = []
    if option_list:
        for val in option_list:
            opt_list_values.append({
                "$type": "UAssetAPI.PropertyTypes.Objects.StrPropertyData, UAssetAPI",
                "Name": str(len(opt_list_values)),
                "ArrayIndex": 0, "IsZero": False,
                "PropertyTagFlags": "None",
                "PropertyTagExtensions": "NoExtension",
                "Value": val
            })

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
                "Value": opt_list_values
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
# Modification 1: DataTable — Inject "Mods" on Main page
# ═══════════════════════════════════════════════════════════════════════

def inject_mods_entry(dt_json):
    """Inject 'Mods' Setting item on the Main page (page 1).

    Uses OptionType=Setting (OPT_SETTING) with empty SettingList.
    Blueprint's DoAction calls OptionIncremented() — no-op on empty list.
    Lua PostHook on DoAction then intercepts "Mods" selection and calls
    NewMenu(100) to enter the custom Mods page.

    The DebugMenuType enum is NOT modified — fully enumless design.
    Byte values 100+ fall through the Blueprint's switch to the generic
    DataTable-driven path, and Lua PostHook NewMenu fills them.

    IMPORTANT: Only uses FNames that ALREADY exist in the asset's NameMap.
    The OptionType must be one of {0, 1, 3, 5, 7} — these are the only
    DebugOptionType values present in the stock NameMap.
    """
    table_data = dt_json["Exports"][0]["Table"]["Data"]
    name_map = dt_json.get("NameMap", [])

    # Add required FNames to NameMap. UAssetGUI uses NameMap index for FName
    # serialization — any FName referenced in the data MUST be in NameMap.
    for fname in ["Main_Mods", "Mods"]:
        if fname not in name_map:
            name_map.append(fname)
            print(f"     Added '{fname}' to NameMap (index {len(name_map) - 1})")

    # Check if already injected (idempotent)
    for row in table_data:
        if row.get("Name") == "Main_Mods":
            print("  ℹ️  'Mods' entry already exists — skipping injection")
            return True

    # Create the Mods row: Setting type on Main page, empty SettingList.
    # Setting with no SettingList → widget shows ActionText display mode.
    # DoAction calls OptionIncremented (no-op), then Lua PostHook catches it.
    mods_row = make_row(
        row_name="Main_Mods",
        option_name="Mods",
        option_type=OPT_SETTING,
        menu_enum=MAIN_PAGE_ENUM,
    )

    # Insert as LAST Main page item (after all stock entries)
    # Find the last row where Menu == MAIN_PAGE_ENUM
    last_main_idx = None
    for i, row in enumerate(table_data):
        for prop in row.get("Value", []):
            if prop.get("Name") == PROP_MENU and prop.get("EnumValue") == MAIN_PAGE_ENUM:
                last_main_idx = i
                break

    if last_main_idx is not None:
        table_data.insert(last_main_idx + 1, mods_row)
        print(f"  ✅ Injected 'Mods' after position {last_main_idx} (last on Main page)")
    else:
        table_data.append(mods_row)
        print(f"  ✅ Appended 'Mods' at end of DataTable")

    print(f"     DataTable: {len(table_data)} total rows (was {len(table_data) - 1})")
    return True


# ═══════════════════════════════════════════════════════════════════════
# Modification 2: DebugVBoxList CDO — Increase MaxVisible
# ═══════════════════════════════════════════════════════════════════════

def patch_vboxlist_maxvisible(vbl_json, new_max=VBOXLIST_MAX_VISIBLE):
    """Patch MaxVisible in DebugVBoxList_C's class default object.

    Stock value: 5 (way too small for VR — causes excessive scrolling).
    NewMenu's switch statement overrides MaxVisible per-page, but:
      - Most pages set it to 0 (unlimited) — our change makes no difference
      - Level select pages set it to 4-10 — still overridden by Blueprint
      - Custom pages (100+) fall through without setting it → our CDO default
        of 20 takes effect instead of 5

    This is a SAFE modification — it only changes the starting default,
    and all stock pages that care still override it explicitly.
    """
    exports = vbl_json.get("Exports", [])
    patched = False

    for export in exports:
        obj_name = export.get("ObjectName", "")
        # CDO is "Default__DebugVBoxList_C", WidgetArchetype also has MaxVisible
        if "DebugVBoxList" not in obj_name:
            continue

        data = export.get("Data", [])
        for prop in data:
            if prop.get("Name") == "MaxVisible":
                old_val = prop.get("Value", "?")
                prop["Value"] = new_max
                print(f"  ✅ {obj_name}: MaxVisible {old_val} → {new_max}")
                patched = True

    if not patched:
        # MaxVisible not found in CDO — need to add it
        # Find the CDO export
        for export in exports:
            flags = export.get("ObjectFlags", "")
            if "RF_ClassDefaultObject" in flags:
                data = export.get("Data", [])
                # Insert MaxVisible property at the beginning
                mv_prop = {
                    "$type": "UAssetAPI.PropertyTypes.Objects.IntPropertyData, UAssetAPI",
                    "Name": "MaxVisible",
                    "ArrayIndex": 0,
                    "IsZero": False,
                    "PropertyTagFlags": "None",
                    "PropertyTagExtensions": "NoExtension",
                    "Value": new_max
                }
                data.insert(0, mv_prop)
                print(f"  ✅ CDO: Added MaxVisible = {new_max} (was not serialized)")
                patched = True
                break

    if not patched:
        print(f"  ⚠️  Could not find MaxVisible in DebugVBoxList CDO")

    return patched


# ═══════════════════════════════════════════════════════════════════════
# Modification 3: DebugMenuWidget — Extend SizeBox viewport height
# ═══════════════════════════════════════════════════════════════════════

def patch_widget_sizebox(widget_json, new_height=SIZEBOX_HEIGHT):
    """Increase SizeBox_90 HeightOverride in DebugMenuWidget_C.

    The DebugMenuWidget_C uses a SizeBox (named SizeBox_90) as the root
    container for all debug menu content.  Stock values:
        WidthOverride  = 1920.0
        HeightOverride = 1200.0

    With 1200px height and ~50px per option widget, only ~22 items fit
    before content is clipped below the SizeBox boundary.  When the Lua
    mod sets MaxVisible=50 and adds many items, they overflow and are
    cut out of sight.

    The WidgetComponent (on DebugMenu_C) has bDrawAtDesiredSize=true,
    so the render target automatically resizes to match the widget's
    desired size.  Increasing the SizeBox height makes the 3D panel
    proportionally taller in VR — acceptable for a debug menu.

    Both SizeBox_90 instances (runtime + editor-only) are patched.
    """
    exports = widget_json.get("Exports", [])
    patched = 0

    for export in exports:
        obj_name = export.get("ObjectName", "")
        if obj_name != "SizeBox_90":
            continue

        data = export.get("Data", [])
        for prop in data:
            if prop.get("Name") == "HeightOverride":
                old_val = prop.get("Value", "?")
                prop["Value"] = new_height
                print(f"  ✅ {obj_name} (outer={export.get('OuterIndex','?')}): "
                      f"HeightOverride {old_val} → {new_height}")
                patched += 1

    if patched == 0:
        print(f"  ⚠️  Could not find SizeBox_90 HeightOverride in DebugMenuWidget")
    else:
        print(f"     Patched {patched} SizeBox_90 instance(s)")

    return patched > 0


# ═══════════════════════════════════════════════════════════════════════
# JSON ↔ Asset Conversion
# ═══════════════════════════════════════════════════════════════════════

def load_json(name):
    """Load a JSON dump from json_dumps/."""
    path = JSON_DIR / f"{name}.json"
    if not path.exists():
        print(f"  ❌ JSON not found: {path}")
        return None
    with open(path, "r", encoding="utf-8") as f:
        return json.load(f)


def save_json_to_uasset(asset_name, data, output_dir):
    """Convert JSON → .uasset/.uexp via UAssetGUI tojson/fromjson."""
    output_dir = Path(output_dir)
    output_dir.mkdir(parents=True, exist_ok=True)

    temp_json = PROJECT_ROOT / "PAKS_extracted" / f"_temp_{asset_name}.json"
    try:
        with open(temp_json, "w", encoding="utf-8") as f:
            json.dump(data, f)

        output_uasset = output_dir / f"{asset_name}.uasset"
        result = subprocess.run(
            [str(UASSETGUI), "fromjson", str(temp_json), str(output_uasset), "VER_UE4_25"],
            capture_output=True, text=True, timeout=60
        )

        if result.returncode != 0:
            print(f"  ❌ fromjson failed for {asset_name}:")
            if result.stderr:
                for line in result.stderr.strip().split("\n")[:5]:
                    print(f"     {line}")
            return False

        output_uexp = output_dir / f"{asset_name}.uexp"
        sizes = []
        if output_uasset.exists():
            sizes.append(f".uasset ({output_uasset.stat().st_size:,}B)")
        if output_uexp.exists():
            sizes.append(f".uexp ({output_uexp.stat().st_size:,}B)")
        print(f"  📦 Built {asset_name}: {', '.join(sizes)}")
        return True

    finally:
        if temp_json.exists():
            temp_json.unlink()


def verify_tools():
    """Check required tools exist."""
    ok = True
    for name, path in [("UAssetGUI", UASSETGUI), ("repak", REPAK)]:
        if not path.exists():
            print(f"  ❌ {name} not found: {path}")
            ok = False
        else:
            print(f"  ✅ {name}: {path}")
    if not EXTRACTED_DIR.exists():
        print(f"  ❌ Extracted assets not found: {EXTRACTED_DIR}")
        ok = False
    else:
        print(f"  ✅ Assets: {EXTRACTED_DIR}")
    if not JSON_DIR.exists():
        print(f"  ❌ JSON dumps not found: {JSON_DIR}")
        ok = False
    else:
        print(f"  ✅ JSON: {JSON_DIR}")
    return ok


# ═══════════════════════════════════════════════════════════════════════
# File Operations
# ═══════════════════════════════════════════════════════════════════════

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


def file_md5(path):
    """Compute MD5 hash of a file."""
    h = hashlib.md5()
    with open(path, "rb") as f:
        for chunk in iter(lambda: f.read(8192), b""):
            h.update(chunk)
    return h.hexdigest()


# ═══════════════════════════════════════════════════════════════════════
# PAK Packing
# ═══════════════════════════════════════════════════════════════════════

def pack_to_pak():
    """Pack build directory into .pak using repak."""
    OUTPUT_PAK.parent.mkdir(parents=True, exist_ok=True)

    result = subprocess.run(
        [str(REPAK), "pack", "--version", "V9", str(BUILD_DIR)],
        capture_output=True, text=True, timeout=120
    )

    # repak outputs to <dir>.pak
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

    # Verify PAK contents
    list_result = subprocess.run(
        [str(REPAK), "list", str(OUTPUT_PAK)],
        capture_output=True, text=True
    )
    files = [f for f in list_result.stdout.strip().split("\n") if f.strip()] if list_result.stdout.strip() else []

    print(f"\n{'═'*60}")
    print(f"  ✅ PAK built: {OUTPUT_PAK.name}")
    print(f"     Path:  {OUTPUT_PAK}")
    print(f"     Size:  {OUTPUT_PAK.stat().st_size:,} bytes")
    print(f"     Files: {len(files)}")
    print(f"     MD5:   {file_md5(OUTPUT_PAK)}")
    for f in files[:10]:
        print(f"       {f}")
    if len(files) > 10:
        print(f"       ... and {len(files) - 10} more")
    print(f"{'═'*60}")
    return True


# ═══════════════════════════════════════════════════════════════════════
# Device Deployment
# ═══════════════════════════════════════════════════════════════════════

def deploy_pak():
    """Push PAK to device via ADB."""
    if not OUTPUT_PAK.exists():
        print(f"  ❌ PAK not found: {OUTPUT_PAK}")
        return False

    device_path = DEVICE_PAK_DIR + OUTPUT_PAK.name
    print(f"\n  Deploying → {device_path}")
    print(f"  Size: {OUTPUT_PAK.stat().st_size:,} bytes")

    result = subprocess.run(
        ["adb", "push", str(OUTPUT_PAK), device_path],
        capture_output=True, text=True, timeout=60
    )

    if result.returncode == 0:
        print(f"  ✅ Deployed successfully")
        print(f"     {result.stdout.strip()}")
        return True

    print(f"  ❌ ADB push failed: {result.stderr.strip()}")
    return False


# ═══════════════════════════════════════════════════════════════════════
# Main Build Pipeline
# ═══════════════════════════════════════════════════════════════════════

def build(dry_run=False, do_deploy=False):
    """Full build pipeline: modify assets → pack → deploy."""
    print("═" * 60)
    print("  RE4 VR Debug Menu PAK — Blueprint Uncooker & Rebuilder")
    print("═" * 60)
    print()
    print("  Modifications:")
    print("    1. DataTable: Inject 'Mods' Action on Main page")
    print(f"    2. VBoxList:  MaxVisible 5 → {VBOXLIST_MAX_VISIBLE}")
    print(f"    3. Widget:    SizeBox HeightOverride 1200 → {SIZEBOX_HEIGHT:.0f}")
    print("    4. All other assets: copied verbatim from stock")
    print()

    # ── Verify tools ──
    print("── Verify Tools ──")
    if not verify_tools():
        return False
    print()

    # ── Step 1: Modify DebugMenuOptions DataTable ──
    print("── Step 1: DebugMenuOptions DataTable ──")
    dt_data = load_json("DebugMenuOptions")
    if not dt_data:
        return False
    if not inject_mods_entry(dt_data):
        return False
    print()

    # ── Step 2: Modify DebugVBoxList CDO ──
    print("── Step 2: DebugVBoxList CDO ──")
    vbl_data = load_json("DebugVBoxList")
    if not vbl_data:
        return False
    if not patch_vboxlist_maxvisible(vbl_data, VBOXLIST_MAX_VISIBLE):
        print("  ⚠️  Continuing without MaxVisible patch")
    print()

    # ── Step 3: Modify DebugMenuWidget SizeBox ──
    print("── Step 3: DebugMenuWidget SizeBox ──")
    dmw_data = load_json("DebugMenuWidget")
    if not dmw_data:
        return False
    if not patch_widget_sizebox(dmw_data, SIZEBOX_HEIGHT):
        print("  ⚠️  Continuing without SizeBox patch")
    print()

    if dry_run:
        print("  [DRY RUN] No files written.")
        return True

    # ── Step 4: Build assets ──
    print("── Step 4: Build Assets ──")
    if BUILD_DIR.exists():
        shutil.rmtree(BUILD_DIR)
    BUILD_DIR.mkdir(parents=True, exist_ok=True)

    output_dir = BUILD_DIR / CONTENT_PATH

    # Copy unmodified assets verbatim
    copied = copy_unmodified(output_dir)
    print(f"  📋 Copied {copied} unmodified asset files")

    # Build modified DataTable
    print(f"\n  Building DebugMenuOptions (DataTable with 'Mods' injection)...")
    if not save_json_to_uasset("DebugMenuOptions", dt_data, output_dir):
        return False

    # Build modified DebugVBoxList
    print(f"\n  Building DebugVBoxList (MaxVisible={VBOXLIST_MAX_VISIBLE})...")
    if not save_json_to_uasset("DebugVBoxList", vbl_data, output_dir):
        print("  ⚠️  DebugVBoxList build failed — falling back to stock copy")
        for ext in [".uasset", ".uexp"]:
            src = EXTRACTED_DIR / f"DebugVBoxList{ext}"
            if src.exists():
                shutil.copy2(src, output_dir / f"DebugVBoxList{ext}")

    # Build modified DebugMenuWidget
    print(f"\n  Building DebugMenuWidget (SizeBox height={SIZEBOX_HEIGHT:.0f})...")
    if not save_json_to_uasset("DebugMenuWidget", dmw_data, output_dir):
        print("  ⚠️  DebugMenuWidget build failed — falling back to stock copy")
        for ext in [".uasset", ".uexp"]:
            src = EXTRACTED_DIR / f"DebugMenuWidget{ext}"
            if src.exists():
                shutil.copy2(src, output_dir / f"DebugMenuWidget{ext}")
    print()

    # ── Step 5: Verify file counts ──
    print("── Step 5: Verify Build ──")
    built_files = list(output_dir.glob("*"))
    uasset_count = sum(1 for f in built_files if f.suffix == ".uasset")
    uexp_count = sum(1 for f in built_files if f.suffix == ".uexp")
    print(f"  Files: {len(built_files)} total ({uasset_count} .uasset, {uexp_count} .uexp)")

    # Check every expected asset exists
    missing = []
    for name in ALL_ASSETS:
        uasset = output_dir / f"{name}.uasset"
        if not uasset.exists():
            missing.append(name)
    if missing:
        print(f"  ⚠️  Missing assets: {', '.join(missing)}")
    else:
        print(f"  ✅ All {len(ALL_ASSETS)} assets present")

    # Compare modified vs stock file sizes
    for name in sorted(MODIFIED_ASSETS):
        for ext in [".uasset", ".uexp"]:
            stock = EXTRACTED_DIR / f"{name}{ext}"
            built = output_dir / f"{name}{ext}"
            if stock.exists() and built.exists():
                delta = built.stat().st_size - stock.stat().st_size
                sign = "+" if delta > 0 else ""
                print(f"     {name}{ext}: {stock.stat().st_size:,}B → {built.stat().st_size:,}B ({sign}{delta:,}B)")
    print()

    # ── Step 6: Pack .pak ──
    print("── Step 6: Pack .pak ──")
    if not pack_to_pak():
        return False

    # ── Step 7: Deploy (optional) ──
    if do_deploy:
        print("\n── Step 7: Deploy ──")
        deploy_pak()

    print(f"\n🎉 Build complete!")
    print(f"   PAK:     {OUTPUT_PAK}")
    print(f"   Deploy:  adb push \"{OUTPUT_PAK}\" {DEVICE_PAK_DIR}")
    print(f"   Or:      python tools/deploy.py mods  (auto-detects PAKs)")
    return True


# ═══════════════════════════════════════════════════════════════════════
# Analysis & Verification Commands
# ═══════════════════════════════════════════════════════════════════════

OPTION_TYPE_LABELS = {
    "DebugOptionType::NewEnumerator0": "SET",  # Setting (cycle values)
    "DebugOptionType::NewEnumerator1": "LNK",  # Link (navigate to page)
    "DebugOptionType::NewEnumerator3": "TOG",  # Toggle
    "DebugOptionType::NewEnumerator4": "ACT",  # Action
    "DebugOptionType::NewEnumerator5": "SEL",  # Select
    "DebugOptionType::NewEnumerator7": "CMD",  # Console command
}

MENU_PAGE_LABELS = {
    "DebugMenuType::NewEnumerator5":  "0:None",
    "DebugMenuType::NewEnumerator0":  "1:Main",
    "DebugMenuType::NewEnumerator1":  "2:Difficulty",
    "DebugMenuType::NewEnumerator2":  "6:Sounds",
    "DebugMenuType::NewEnumerator3":  "7:Rendering",
    "DebugMenuType::NewEnumerator4":  "8:Gameplay",
    "DebugMenuType::NewEnumerator6":  "9:Player",
    "DebugMenuType::NewEnumerator7":  "10:Items",
    "DebugMenuType::NewEnumerator8":  "12:Misc",
    "DebugMenuType::NewEnumerator9":  "13:Cheats",
    "DebugMenuType::NewEnumerator10": "24:1POptions",
    "DebugMenuType::NewEnumerator11": "25:Enemy",
    "DebugMenuType::NewEnumerator12": "27:Effects",
    "DebugMenuType::NewEnumerator13": "28:OptionMenu",
    "DebugMenuType::NewEnumerator14": "30:Profiles",
    "DebugMenuType::NewEnumerator15": "31:AI",
    "DebugMenuType::NewEnumerator16": "33:Streaming",
}


def analyze():
    """Dump the current DataTable state in readable format."""
    dt_data = load_json("DebugMenuOptions")
    if not dt_data:
        return
    table_data = dt_data["Exports"][0]["Table"]["Data"]

    print(f"═══ DebugMenuOptions DataTable ({len(table_data)} rows) ═══\n")
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
        label = MENU_PAGE_LABELS.get(page_enum, page_enum)
        print(f"  Page: {label} ({len(items)} items)")
        for name, otype, rname in items:
            ot_short = OPTION_TYPE_LABELS.get(otype, "???")
            print(f"    {ot_short} {name:45s} [{rname}]")
        print()

    has_mods = any(row.get("Name") == "Main_Mods" for row in table_data)
    print(f"  Mods entry: {'✅ Present' if has_mods else '❌ Missing (will be injected by build)'}")


def verify():
    """Verify the built PAK contents."""
    if not OUTPUT_PAK.exists():
        print(f"❌ PAK not found: {OUTPUT_PAK}")
        print(f"   Run: python build_debug_pak.py")
        return

    print(f"═══ PAK Verification: {OUTPUT_PAK.name} ═══\n")
    print(f"  Size: {OUTPUT_PAK.stat().st_size:,} bytes")
    print(f"  MD5:  {file_md5(OUTPUT_PAK)}")

    list_result = subprocess.run(
        [str(REPAK), "list", str(OUTPUT_PAK)],
        capture_output=True, text=True
    )
    files = [f.strip() for f in list_result.stdout.strip().split("\n") if f.strip()]
    print(f"  Files: {len(files)}\n")

    for f in sorted(files):
        print(f"    {f}")

    # Check all expected assets
    print(f"\n  ── Asset check ──")
    for name in ALL_ASSETS:
        uasset_path = f"{CONTENT_PATH}/{name}.uasset"
        uexp_path = f"{CONTENT_PATH}/{name}.uexp"
        has_uasset = any(uasset_path in f for f in files)
        has_uexp = any(uexp_path in f for f in files)
        status = "✅" if (has_uasset or has_uexp) else "❌"
        print(f"    {status} {name}")


def clean():
    """Remove build artifacts."""
    removed = 0
    for path, label in [(BUILD_DIR, "build dir"), (OUTPUT_PAK, "PAK file")]:
        if isinstance(path, Path) and path.exists():
            if path.is_dir():
                shutil.rmtree(path)
            else:
                path.unlink()
            print(f"  🗑️  Removed {label}: {path}")
            removed += 1
    if removed == 0:
        print("  Nothing to clean.")


# ═══════════════════════════════════════════════════════════════════════
# CLI Entry Point
# ═══════════════════════════════════════════════════════════════════════

def main():
    parser = argparse.ArgumentParser(
        description="RE4 VR Debug Menu PAK — Blueprint Uncooker & Rebuilder",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples:
  python build_debug_pak.py              Build the PAK
  python build_debug_pak.py --deploy     Build + push to Quest 3
  python build_debug_pak.py --analyze    Dump DataTable structure
  python build_debug_pak.py --verify     Verify built PAK contents
  python build_debug_pak.py --dry-run    Preview without writing files
  python build_debug_pak.py --clean      Remove build artifacts

Pipeline:
  1. Extract stock PAK → PAKS_extracted/VR4/ (already done via repak)
  2. Dump to JSON → json_dumps/ (already done via UAssetGUI tojson)
  3. Modify JSON (this script)
  4. Convert JSON → .uasset/.uexp (via UAssetGUI fromjson)
  5. Pack → .pak (via repak pack)
  6. Deploy to device → /storage/emulated/0/Android/data/.../files/paks/
  7. Modloader mounts at priority 1000+ during engine boot
""")
    parser.add_argument("--deploy", action="store_true", help="Build + push to device")
    parser.add_argument("--dry-run", action="store_true", help="Preview changes only")
    parser.add_argument("--analyze", action="store_true", help="Dump DataTable state")
    parser.add_argument("--verify", action="store_true", help="Verify PAK contents")
    parser.add_argument("--clean", action="store_true", help="Remove build artifacts")
    args = parser.parse_args()

    if args.clean:
        clean()
        return
    if args.analyze:
        analyze()
        return
    if args.verify:
        verify()
        return

    success = build(dry_run=args.dry_run, do_deploy=args.deploy)
    sys.exit(0 if success else 1)


if __name__ == "__main__":
    main()
