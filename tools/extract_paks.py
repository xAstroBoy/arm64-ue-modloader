#!/usr/bin/env python3
"""
RE4 VR — PAK Extraction Tool
═════════════════════════════
Uses repak to extract .pak files from PAKS/ directory.

Usage:
  python tools/extract_paks.py                      # Extract ALL paks to PAKS_extracted/
  python tools/extract_paks.py --debug-only         # Extract only Debug Blueprint assets
  python tools/extract_paks.py --list               # List contents without extracting
  python tools/extract_paks.py --search <pattern>   # Search for assets matching pattern
  python tools/extract_paks.py --info               # Show pak file info

Requires: repak (install via: cargo install repak-cli)
"""
import os, sys, subprocess, shutil, argparse, re
from pathlib import Path

ROOT     = Path(__file__).resolve().parent.parent
PAKS_DIR = ROOT / "PAKS"
OUT_DIR  = ROOT / "PAKS_extracted"

# ── Key asset paths we care about ────────────────────────────────────
DEBUG_BLUEPRINT_PREFIX = "VR4/Content/Blueprints/Debug/"
KEY_ASSETS = [
    "VR4/Content/Blueprints/Debug/DebugMenu/DebugMenu",
    "VR4/Content/Blueprints/Debug/DebugMenu/DebugMenuWidget",
    "VR4/Content/Blueprints/Debug/DebugMenu/DebugOptionWidget",
    "VR4/Content/Blueprints/Debug/DebugMenu/DebugVBoxList",
    "VR4/Content/Blueprints/Debug/DebugMenu/DebugMenuInterface",
    "VR4/Content/Blueprints/Debug/DebugMenu/DebugMenuOption",
    "VR4/Content/Blueprints/Debug/DebugMenu/DebugMenuOptions",
    "VR4/Content/Blueprints/Debug/DebugMenu/DebugMenuType",
    "VR4/Content/Blueprints/Debug/DebugMenu/DebugOptionType",
    "VR4/Content/Blueprints/Debug/DebugMenu/DebugRoomKeys_DT",
    "VR4/Content/Blueprints/Debug/DebugMenu/DebugLevelList",
    "VR4/Content/Blueprints/Debug/DebugManager_BP",
]


def find_repak():
    """Find repak executable."""
    r = shutil.which("repak")
    if r:
        return r
    # Check common locations
    for p in [
        Path.home() / ".cargo" / "bin" / "repak.exe",
        Path.home() / ".cargo" / "bin" / "repak",
        ROOT / "tools" / "repak.exe",
    ]:
        if p.exists():
            return str(p)
    return None


def get_paks():
    """Get sorted list of .pak files (non-optional first, then optional)."""
    paks = sorted(PAKS_DIR.glob("*.pak"))
    # Sort: non-optional first, then optional (so optional overrides base)
    base = [p for p in paks if "optional" not in p.name]
    opt  = [p for p in paks if "optional" in p.name]
    return base + opt


def repak_list(repak_cmd, pak_path):
    """List contents of a pak file. Returns list of paths."""
    result = subprocess.run(
        [repak_cmd, "list", str(pak_path)],
        capture_output=True, text=True, encoding='utf-8', errors='replace'
    )
    if result.returncode != 0:
        print(f"  ⚠ Error listing {pak_path.name}: {result.stderr.strip()}")
        return []
    return [line.strip() for line in result.stdout.splitlines() if line.strip()]


def repak_info(repak_cmd, pak_path):
    """Get info about a pak file."""
    result = subprocess.run(
        [repak_cmd, "info", str(pak_path)],
        capture_output=True, text=True, encoding='utf-8', errors='replace'
    )
    return result.stdout.strip() if result.returncode == 0 else f"Error: {result.stderr.strip()}"


def repak_unpack(repak_cmd, pak_path, output_dir, filter_prefix=None):
    """
    Unpack a pak file to output_dir.
    If filter_prefix is set, only extract files under that prefix.
    """
    output_dir.mkdir(parents=True, exist_ok=True)

    if filter_prefix:
        # Get file list, filter, extract individually
        files = repak_list(repak_cmd, pak_path)
        targets = [f for f in files if f.startswith(filter_prefix)]
        if not targets:
            return 0

        # repak unpack extracts everything - we'll extract all and clean up later
        # OR we can use repak's built-in filtering if available
        # For now, extract full pak and let caller filter
        cmd = [repak_cmd, "unpack", str(pak_path), "-o", str(output_dir)]
        result = subprocess.run(cmd, capture_output=True, text=True, encoding='utf-8', errors='replace')
        if result.returncode != 0:
            print(f"  ⚠ Unpack error: {result.stderr.strip()[:200]}")
            return 0
        return len(targets)
    else:
        cmd = [repak_cmd, "unpack", str(pak_path), "-o", str(output_dir)]
        result = subprocess.run(cmd, capture_output=True, text=True, encoding='utf-8', errors='replace')
        if result.returncode != 0:
            print(f"  ⚠ Unpack error: {result.stderr.strip()[:200]}")
            return 0
        # Count extracted
        count = sum(1 for _ in output_dir.rglob("*") if _.is_file())
        return count


def cmd_info(repak_cmd):
    """Show info for all pak files."""
    paks = get_paks()
    print(f"═══ PAK Info ({len(paks)} files) ═══\n")
    for pak in paks:
        size_mb = pak.stat().st_size / (1024*1024)
        print(f"── {pak.name} ({size_mb:.1f} MB) ──")
        info = repak_info(repak_cmd, pak)
        for line in info.splitlines():
            print(f"  {line}")
        print()


def cmd_list(repak_cmd, search=None):
    """List contents of all paks, optionally filtered."""
    paks = get_paks()
    all_files = {}  # path -> pak_name

    for pak in paks:
        files = repak_list(repak_cmd, pak)
        for f in files:
            all_files[f] = pak.name

    if search:
        pattern = re.compile(search, re.IGNORECASE)
        all_files = {k: v for k, v in all_files.items() if pattern.search(k)}

    print(f"═══ Asset Listing ({len(all_files)} files) ═══\n")

    # Group by extension
    by_ext = {}
    for path in sorted(all_files.keys()):
        ext = Path(path).suffix.lower()
        by_ext.setdefault(ext, []).append(path)

    for ext in sorted(by_ext.keys()):
        files = by_ext[ext]
        print(f"\n── {ext or '(no ext)'} ({len(files)} files) ──")
        for f in files[:50]:
            print(f"  {f}  [{all_files[f]}]")
        if len(files) > 50:
            print(f"  ... and {len(files) - 50} more")

    # Summary
    print(f"\n═══ Summary ═══")
    for ext in sorted(by_ext.keys(), key=lambda e: -len(by_ext[e])):
        print(f"  {ext or '(none)':>8}: {len(by_ext[ext]):>5} files")
    print(f"  {'TOTAL':>8}: {len(all_files):>5} files")


def cmd_search(repak_cmd, pattern):
    """Search for assets matching a pattern across all paks."""
    paks = get_paks()
    regex = re.compile(pattern, re.IGNORECASE)
    matches = []

    for pak in paks:
        files = repak_list(repak_cmd, pak)
        for f in files:
            if regex.search(f):
                matches.append((f, pak.name))

    print(f"═══ Search: '{pattern}' ({len(matches)} matches) ═══\n")
    for path, pak in matches:
        print(f"  {path}  [{pak}]")


def cmd_extract(repak_cmd, debug_only=False):
    """Extract pak files."""
    paks = get_paks()

    if debug_only:
        print(f"═══ Extracting Debug Blueprints Only ═══\n")
        prefix = DEBUG_BLUEPRINT_PREFIX
    else:
        print(f"═══ Full PAK Extraction ═══\n")
        prefix = None

    # Use a single merged output directory
    OUT_DIR.mkdir(parents=True, exist_ok=True)
    total_files = 0

    for pak in paks:
        size_mb = pak.stat().st_size / (1024*1024)
        sys.stdout.write(f"  Extracting {pak.name} ({size_mb:.0f} MB)...")
        sys.stdout.flush()

        # Check if this pak has files we want
        if debug_only:
            files = repak_list(repak_cmd, pak)
            targets = [f for f in files if f.startswith(prefix)]
            if not targets:
                print(f" skip (no debug assets)")
                continue

        count = repak_unpack(repak_cmd, pak, OUT_DIR, prefix)
        total_files += count
        print(f" done ({count} files)")

    # If debug-only, clean up non-debug files
    if debug_only and OUT_DIR.exists():
        for root, dirs, files in os.walk(OUT_DIR, topdown=False):
            root_path = Path(root)
            rel = root_path.relative_to(OUT_DIR)
            # Keep only VR4/Content/Blueprints/Debug/
            if not str(rel).startswith("VR4"):
                # Remove Engine/ and other non-game files
                if root_path != OUT_DIR:
                    shutil.rmtree(root_path, ignore_errors=True)

    # Show what we got
    if OUT_DIR.exists():
        print(f"\n═══ Extraction Complete ═══")
        print(f"  Output: {OUT_DIR}")

        # List key assets
        uassets = list(OUT_DIR.rglob("*.uasset"))
        uexps = list(OUT_DIR.rglob("*.uexp"))
        print(f"  .uasset files: {len(uassets)}")
        print(f"  .uexp files:   {len(uexps)}")

        # Highlight key debug menu assets
        print(f"\n── Key Debug Menu Assets ──")
        for key in KEY_ASSETS:
            uasset = OUT_DIR / (key + ".uasset")
            uexp   = OUT_DIR / (key + ".uexp")
            name   = Path(key).name
            if uasset.exists() and uexp.exists():
                size_a = uasset.stat().st_size
                size_e = uexp.stat().st_size
                print(f"  ✓ {name:30s}  .uasset={size_a:>8,} B  .uexp={size_e:>8,} B")
            elif uasset.exists():
                print(f"  ~ {name:30s}  .uasset only")
            else:
                print(f"  ✗ {name:30s}  NOT FOUND")

        # Also show DataTables
        print(f"\n── DataTables ──")
        for f in sorted(uassets):
            if "_DT" in f.name or "DataTable" in f.name or "DT." in f.name:
                rel = f.relative_to(OUT_DIR)
                print(f"  {rel}")

        print(f"\n═══ Next Steps ═══")
        print(f"  Dump Blueprint:  python tools/dump_uasset.py <file.uasset>")
        print(f"  Scan all:        python tools/dump_uasset.py --scan {OUT_DIR}")
        print(f"  Dump DebugMenu:  python tools/dump_uasset.py {OUT_DIR / 'VR4/Content/Blueprints/Debug/DebugMenu/DebugMenu.uasset'}")


def main():
    parser = argparse.ArgumentParser(description="RE4 VR PAK Extraction Tool")
    parser.add_argument("--info", action="store_true", help="Show pak file info")
    parser.add_argument("--list", action="store_true", help="List all pak contents")
    parser.add_argument("--search", type=str, help="Search for assets matching pattern")
    parser.add_argument("--debug-only", action="store_true", help="Extract only Debug Blueprint assets")
    parser.add_argument("--full", action="store_true", help="Full extraction (WARNING: ~7GB+)")
    args = parser.parse_args()

    repak_cmd = find_repak()
    if not repak_cmd:
        print("ERROR: repak not found!")
        print("Install:  cargo install repak-cli")
        print("Or download from: https://github.com/trumank/repak/releases")
        sys.exit(1)

    print(f"repak: {repak_cmd}")

    if not PAKS_DIR.exists():
        print(f"ERROR: PAKS directory not found: {PAKS_DIR}")
        sys.exit(1)

    if args.info:
        cmd_info(repak_cmd)
    elif args.list:
        cmd_list(repak_cmd)
    elif args.search:
        cmd_search(repak_cmd, args.search)
    elif args.full:
        cmd_extract(repak_cmd, debug_only=False)
    else:
        # Default: extract debug-only (fast, focused)
        cmd_extract(repak_cmd, debug_only=True)


if __name__ == "__main__":
    main()
