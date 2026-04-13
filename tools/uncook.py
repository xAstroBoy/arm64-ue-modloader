#!/usr/bin/env python3
"""
UE4/UE5 Quest Modloader — Asset Uncooker Pipeline
══════════════════════════════════════════════════
Full pipeline: PAK → extract → uasset→JSON → edit → JSON→uasset → repack PAK

Commands:
  uncook.py extract [--game re4|pfx] [--filter <pattern>]   Extract PAK → raw assets
  uncook.py dump    <uasset> [--json] [--kismet]             Dump uasset to JSON/text
  uncook.py dump-all [--dir <path>] [--json]                 Dump all uassets in a dir
  uncook.py edit    <uasset.json>                             Open in UAssetGUI
  uncook.py cook    <uasset.json> [-o <uasset>]              JSON → cooked uasset
  uncook.py repack  <dir> [-o <output.pak>] [--version <ver>] Repack dir → PAK
  uncook.py patch   <pak> --asset <path> --json <file>       In-place: replace one asset
  uncook.py info    [--game re4|pfx]                          Show PAK info
  uncook.py search  <pattern> [--game re4|pfx]               Search assets in PAKs
  uncook.py list    [--game re4|pfx]                          List all assets
  uncook.py roundtrip <uasset>                                Extract→dump→cook→verify

Requires:
  - repak   (cargo install repak-cli, or tools/repak/repak.exe)
  - UAssetGUI (tools/bin/UAssetGUI.exe) for JSON↔uasset conversion

Author: xAstroBoy
"""

import os, sys, subprocess, shutil, argparse, json, hashlib, tempfile
from pathlib import Path
from typing import Optional, List, Tuple

# ═══════════════════════════════════════════════════════════════════════
# Paths
# ═══════════════════════════════════════════════════════════════════════

ROOT = Path(__file__).resolve().parent.parent

GAME_PAKS = {
    "re4": ROOT / "PAKS",
    "pfx": ROOT / "Pinball FX VR Patches" / "PAKS",
}

GAME_EXTRACT = {
    "re4": ROOT / "PAKS_extracted",
    "pfx": ROOT / "Pinball FX VR Patches" / "PAKS_extracted",
}

GAME_UE_VERSION = {
    "re4": "VER_UE4_25",    # UE4.25
    "pfx": "VER_UE5_1",     # UE5.x
}

REPAK_BUILTIN = ROOT / "tools" / "repak" / "repak.exe"
UASSETGUI     = ROOT / "tools" / "bin" / "UAssetGUI.exe"
UASSETAPI_CLI = ROOT / "tools" / "bin" / "UAssetAPI.exe"  # if available

# ═══════════════════════════════════════════════════════════════════════
# Tool Discovery
# ═══════════════════════════════════════════════════════════════════════

def find_repak() -> Optional[str]:
    """Find repak executable."""
    if REPAK_BUILTIN.exists():
        return str(REPAK_BUILTIN)
    r = shutil.which("repak")
    if r:
        return r
    for p in [
        Path.home() / ".cargo" / "bin" / "repak.exe",
        Path.home() / ".cargo" / "bin" / "repak",
    ]:
        if p.exists():
            return str(p)
    return None


def find_uassetgui() -> Optional[str]:
    """Find UAssetGUI executable."""
    if UASSETGUI.exists():
        return str(UASSETGUI)
    r = shutil.which("UAssetGUI")
    if r:
        return r
    return None


def require_repak() -> str:
    r = find_repak()
    if not r:
        print("ERROR: repak not found!")
        print("  Install:  cargo install repak-cli")
        print("  Or place repak.exe in tools/repak/")
        sys.exit(1)
    return r


def require_uassetgui() -> str:
    r = find_uassetgui()
    if not r:
        print("ERROR: UAssetGUI.exe not found!")
        print("  Download from: https://github.com/atenfyr/UAssetGUI/releases")
        print("  Place in tools/bin/UAssetGUI.exe")
        sys.exit(1)
    return r


def get_paks_dir(game: str) -> Path:
    d = GAME_PAKS.get(game)
    if not d or not d.exists():
        print(f"ERROR: PAK directory not found for game '{game}': {d}")
        sys.exit(1)
    return d


def get_extract_dir(game: str) -> Path:
    return GAME_EXTRACT.get(game, ROOT / f"{game}_extracted")


def get_ue_version(game: str) -> str:
    return GAME_UE_VERSION.get(game, "VER_UE4_25")


# ═══════════════════════════════════════════════════════════════════════
# PAK Operations (via repak)
# ═══════════════════════════════════════════════════════════════════════

def pak_list(repak: str, pak_path: Path) -> List[str]:
    """List contents of a .pak file."""
    r = subprocess.run([repak, "list", str(pak_path)],
                       capture_output=True, text=True, encoding="utf-8", errors="replace")
    if r.returncode != 0:
        print(f"  ⚠ Error listing {pak_path.name}: {r.stderr.strip()[:200]}")
        return []
    return [l.strip() for l in r.stdout.splitlines() if l.strip()]


def pak_info(repak: str, pak_path: Path) -> str:
    """Get info about a .pak file."""
    r = subprocess.run([repak, "info", str(pak_path)],
                       capture_output=True, text=True, encoding="utf-8", errors="replace")
    return r.stdout.strip() if r.returncode == 0 else f"Error: {r.stderr.strip()}"


def pak_extract(repak: str, pak_path: Path, out_dir: Path) -> int:
    """Extract a .pak file to out_dir. Returns file count."""
    out_dir.mkdir(parents=True, exist_ok=True)
    r = subprocess.run([repak, "unpack", str(pak_path), "-o", str(out_dir)],
                       capture_output=True, text=True, encoding="utf-8", errors="replace")
    if r.returncode != 0:
        print(f"  ⚠ Unpack error: {r.stderr.strip()[:200]}")
        return 0
    return sum(1 for _ in out_dir.rglob("*") if _.is_file())


def pak_repack(repak: str, source_dir: Path, out_pak: Path, version: Optional[str] = None) -> bool:
    """Repack a directory into a .pak file."""
    out_pak.parent.mkdir(parents=True, exist_ok=True)
    cmd = [repak, "pack", str(source_dir), str(out_pak)]
    if version:
        cmd.extend(["--version", version])
    r = subprocess.run(cmd, capture_output=True, text=True, encoding="utf-8", errors="replace")
    if r.returncode != 0:
        print(f"  ⚠ Pack error: {r.stderr.strip()[:300]}")
        return False
    return True


# ═══════════════════════════════════════════════════════════════════════
# UAsset Operations (via UAssetGUI CLI / UAssetAPI)
# ═══════════════════════════════════════════════════════════════════════

def uasset_to_json(uassetgui: str, uasset_path: Path, json_path: Path,
                   ue_version: str = "VER_UE4_25") -> bool:
    """Convert a .uasset to JSON using UAssetGUI CLI mode."""
    json_path.parent.mkdir(parents=True, exist_ok=True)

    # UAssetGUI CLI: UAssetGUI.exe <input.uasset> <UE_version> <output.json>
    # Also needs the .uexp alongside the .uasset
    uexp = uasset_path.with_suffix(".uexp")
    if not uexp.exists():
        print(f"  ⚠ Missing .uexp for {uasset_path.name}")
        return False

    r = subprocess.run(
        [uassetgui, str(uasset_path), ue_version, str(json_path)],
        capture_output=True, text=True, encoding="utf-8", errors="replace",
        timeout=60,
    )
    if r.returncode != 0:
        # Try tojson subcommand (newer UAssetGUI versions)
        r2 = subprocess.run(
            [uassetgui, "tojson", str(uasset_path), str(json_path), "-v", ue_version],
            capture_output=True, text=True, encoding="utf-8", errors="replace",
            timeout=60,
        )
        if r2.returncode != 0:
            print(f"  ⚠ JSON export failed: {r.stderr.strip()[:200]}")
            print(f"     Alt attempt: {r2.stderr.strip()[:200]}")
            return False

    if json_path.exists() and json_path.stat().st_size > 0:
        return True
    print(f"  ⚠ JSON output empty or missing")
    return False


def json_to_uasset(uassetgui: str, json_path: Path, uasset_path: Path,
                   ue_version: str = "VER_UE4_25") -> bool:
    """Convert a JSON back to .uasset/.uexp using UAssetGUI CLI mode."""
    uasset_path.parent.mkdir(parents=True, exist_ok=True)

    # UAssetGUI CLI: UAssetGUI.exe <input.json> <UE_version> <output.uasset>
    r = subprocess.run(
        [uassetgui, str(json_path), ue_version, str(uasset_path)],
        capture_output=True, text=True, encoding="utf-8", errors="replace",
        timeout=60,
    )
    if r.returncode != 0:
        # Try fromjson subcommand
        r2 = subprocess.run(
            [uassetgui, "fromjson", str(json_path), str(uasset_path), "-v", ue_version],
            capture_output=True, text=True, encoding="utf-8", errors="replace",
            timeout=60,
        )
        if r2.returncode != 0:
            print(f"  ⚠ Cook from JSON failed: {r.stderr.strip()[:200]}")
            print(f"     Alt attempt: {r2.stderr.strip()[:200]}")
            return False

    if uasset_path.exists():
        return True
    print(f"  ⚠ Cooked output missing")
    return False


# ═══════════════════════════════════════════════════════════════════════
# File Hashing (for roundtrip verification)
# ═══════════════════════════════════════════════════════════════════════

def file_sha256(path: Path) -> str:
    h = hashlib.sha256()
    with open(path, "rb") as f:
        for chunk in iter(lambda: f.read(65536), b""):
            h.update(chunk)
    return h.hexdigest()


# ═══════════════════════════════════════════════════════════════════════
# Commands
# ═══════════════════════════════════════════════════════════════════════

def cmd_info(args):
    """Show PAK info."""
    repak = require_repak()
    paks_dir = get_paks_dir(args.game)
    paks = sorted(paks_dir.glob("*.pak"))
    print(f"═══ PAK Info — {args.game.upper()} ({len(paks)} files) ═══\n")
    total_size = 0
    total_assets = 0
    for pak in paks:
        size_mb = pak.stat().st_size / (1024 * 1024)
        total_size += size_mb
        count = len(pak_list(repak, pak))
        total_assets += count
        print(f"  {pak.name:50s}  {size_mb:>8.1f} MB  {count:>6} assets")
        info = pak_info(repak, pak)
        for line in info.splitlines():
            print(f"    {line}")
    print(f"\n  {'TOTAL':50s}  {total_size:>8.1f} MB  {total_assets:>6} assets")


def cmd_list(args):
    """List all assets in PAKs."""
    repak = require_repak()
    paks_dir = get_paks_dir(args.game)
    paks = sorted(paks_dir.glob("*.pak"))
    all_files = {}
    for pak in paks:
        for f in pak_list(repak, pak):
            all_files[f] = pak.name

    # Group by extension
    by_ext = {}
    for path in sorted(all_files.keys()):
        ext = Path(path).suffix.lower()
        by_ext.setdefault(ext, []).append(path)

    print(f"═══ Asset Listing — {args.game.upper()} ({len(all_files)} total) ═══\n")
    for ext in sorted(by_ext.keys()):
        files = by_ext[ext]
        print(f"── {ext or '(no ext)'} ({len(files)} files) ──")
        for f in files[:30]:
            print(f"  {f}  [{all_files[f]}]")
        if len(files) > 30:
            print(f"  ... and {len(files) - 30} more")

    print(f"\n═══ Summary ═══")
    for ext in sorted(by_ext.keys(), key=lambda e: -len(by_ext[e])):
        print(f"  {ext or '(none)':>10}: {len(by_ext[ext]):>5} files")
    print(f"  {'TOTAL':>10}: {len(all_files):>5} files")


def cmd_search(args):
    """Search for assets matching a pattern."""
    import re as regex_mod
    repak = require_repak()
    paks_dir = get_paks_dir(args.game)
    paks = sorted(paks_dir.glob("*.pak"))
    pat = regex_mod.compile(args.pattern, regex_mod.IGNORECASE)
    matches = []
    for pak in paks:
        for f in pak_list(repak, pak):
            if pat.search(f):
                matches.append((f, pak.name))

    print(f"═══ Search: '{args.pattern}' — {args.game.upper()} ({len(matches)} matches) ═══\n")
    for path, pak in matches:
        print(f"  {path}  [{pak}]")


def cmd_extract(args):
    """Extract PAK files to raw assets."""
    repak = require_repak()
    paks_dir = get_paks_dir(args.game)
    out_dir = get_extract_dir(args.game)
    paks = sorted(paks_dir.glob("*.pak"))

    # Sort: base first, optional second (optional overrides base)
    base = [p for p in paks if "optional" not in p.name.lower()]
    opt = [p for p in paks if "optional" in p.name.lower()]
    paks = base + opt

    if args.filter:
        print(f"═══ Extracting {args.game.upper()} (filter: {args.filter}) ═══\n")
    else:
        print(f"═══ Extracting {args.game.upper()} (all assets) ═══\n")

    out_dir.mkdir(parents=True, exist_ok=True)
    total = 0

    for pak in paks:
        size_mb = pak.stat().st_size / (1024 * 1024)
        sys.stdout.write(f"  {pak.name} ({size_mb:.0f} MB)...")
        sys.stdout.flush()

        if args.filter:
            # Check if pak has matching files
            files = pak_list(repak, pak)
            import re as regex_mod
            pat = regex_mod.compile(args.filter, regex_mod.IGNORECASE)
            targets = [f for f in files if pat.search(f)]
            if not targets:
                print(" skip (no matches)")
                continue
            # Extract full pak, matching files only live in the output
            count = pak_extract(repak, pak, out_dir)
        else:
            count = pak_extract(repak, pak, out_dir)

        total += count
        print(f" {count} files")

    # Summary
    uassets = list(out_dir.rglob("*.uasset"))
    uexps = list(out_dir.rglob("*.uexp"))
    ubulks = list(out_dir.rglob("*.ubulk"))
    print(f"\n═══ Extraction Complete ═══")
    print(f"  Output:     {out_dir}")
    print(f"  .uasset:    {len(uassets)}")
    print(f"  .uexp:      {len(uexps)}")
    print(f"  .ubulk:     {len(ubulks)}")
    print(f"  Total:      {total}")

    print(f"\n═══ Next Steps ═══")
    print(f"  Dump single:   python tools/uncook.py dump <file.uasset> --json")
    print(f"  Dump all:      python tools/uncook.py dump-all --dir {out_dir}")
    print(f"  Open in GUI:   python tools/uncook.py edit <file.json>")


def cmd_dump(args):
    """Dump a single .uasset to JSON or text."""
    uasset_path = Path(args.uasset).resolve()
    if not uasset_path.exists():
        print(f"ERROR: File not found: {uasset_path}")
        sys.exit(1)

    game = args.game
    ue_ver = get_ue_version(game)

    if args.json:
        uassetgui = require_uassetgui()
        json_out = uasset_path.with_suffix(".json")
        if args.output:
            json_out = Path(args.output)

        print(f"═══ Dumping to JSON: {uasset_path.name} ═══")
        print(f"  UE Version: {ue_ver}")
        if uasset_to_json(uassetgui, uasset_path, json_out, ue_ver):
            size = json_out.stat().st_size / 1024
            print(f"  ✅ Output: {json_out} ({size:.1f} KB)")
            # Parse and show summary
            try:
                data = json.loads(json_out.read_text(encoding="utf-8"))
                exports = data.get("Exports", [])
                names = data.get("NameMap", [])
                imports = data.get("Imports", [])
                print(f"  Names:   {len(names)}")
                print(f"  Imports: {len(imports)}")
                print(f"  Exports: {len(exports)}")
                for i, exp in enumerate(exports):
                    obj_name = exp.get("ObjectName", f"Export_{i}")
                    obj_type = exp.get("$type", "").split(".")[-1].split(",")[0]
                    print(f"    [{i}] {obj_name} ({obj_type})")
            except Exception:
                pass
        else:
            print(f"  ❌ Failed to dump")
            sys.exit(1)
    elif args.kismet:
        # Use our built-in kismet decompiler
        decompiler = ROOT / "tools" / "kismet_decompiler.py"
        if decompiler.exists():
            # First dump to JSON, then decompile
            uassetgui = require_uassetgui()
            with tempfile.NamedTemporaryFile(suffix=".json", delete=False) as tmp:
                tmp_json = Path(tmp.name)
            try:
                if uasset_to_json(uassetgui, uasset_path, tmp_json, ue_ver):
                    subprocess.run([sys.executable, str(decompiler), str(tmp_json)],
                                   cwd=str(ROOT))
                else:
                    print("Failed to export JSON for Kismet decompilation")
            finally:
                tmp_json.unlink(missing_ok=True)
        else:
            print("kismet_decompiler.py not found")
    else:
        # Use our dump_uasset.py for text dump
        dumper = ROOT / "tools" / "dump_uasset.py"
        if dumper.exists():
            subprocess.run([sys.executable, str(dumper), str(uasset_path)], cwd=str(ROOT))
        else:
            print("dump_uasset.py not found — use --json for UAssetGUI JSON dump")


def cmd_dump_all(args):
    """Dump all .uasset files in a directory to JSON."""
    uassetgui = require_uassetgui()
    target_dir = Path(args.dir).resolve() if args.dir else get_extract_dir(args.game)
    ue_ver = get_ue_version(args.game)

    if not target_dir.exists():
        print(f"ERROR: Directory not found: {target_dir}")
        print(f"  Run first: python tools/uncook.py extract --game {args.game}")
        sys.exit(1)

    uassets = sorted(target_dir.rglob("*.uasset"))
    print(f"═══ Batch Dump — {len(uassets)} .uasset files ═══\n")
    print(f"  Source:  {target_dir}")
    print(f"  UE Ver:  {ue_ver}")

    json_dir = target_dir.parent / f"{target_dir.name}_json"
    json_dir.mkdir(parents=True, exist_ok=True)
    print(f"  Output:  {json_dir}\n")

    ok = 0
    fail = 0
    for i, uasset in enumerate(uassets, 1):
        rel = uasset.relative_to(target_dir)
        json_out = json_dir / rel.with_suffix(".json")
        sys.stdout.write(f"  [{i}/{len(uassets)}] {rel}...")
        sys.stdout.flush()

        if json_out.exists() and not args.force:
            print(" skip (exists, use --force)")
            ok += 1
            continue

        if uasset_to_json(uassetgui, uasset, json_out, ue_ver):
            size = json_out.stat().st_size / 1024
            print(f" ✅ ({size:.1f} KB)")
            ok += 1
        else:
            print(f" ❌")
            fail += 1

    print(f"\n═══ Batch Dump Complete ═══")
    print(f"  Success: {ok}")
    print(f"  Failed:  {fail}")
    print(f"  Output:  {json_dir}")


def cmd_edit(args):
    """Open a JSON file in UAssetGUI for visual editing."""
    uassetgui = require_uassetgui()
    target = Path(args.file).resolve()
    if not target.exists():
        print(f"ERROR: File not found: {target}")
        sys.exit(1)

    print(f"═══ Opening in UAssetGUI: {target.name} ═══")
    subprocess.Popen([uassetgui, str(target)], cwd=str(target.parent))
    print("  Launched UAssetGUI. Edit and save, then use:")
    print(f"  python tools/uncook.py cook {target}")


def cmd_cook(args):
    """Convert a JSON file back to .uasset/.uexp (re-cook)."""
    uassetgui = require_uassetgui()
    json_path = Path(args.json_file).resolve()
    ue_ver = get_ue_version(args.game)

    if not json_path.exists():
        print(f"ERROR: File not found: {json_path}")
        sys.exit(1)

    if args.output:
        uasset_out = Path(args.output).resolve()
    else:
        uasset_out = json_path.with_suffix(".uasset")

    print(f"═══ Cooking: {json_path.name} → {uasset_out.name} ═══")
    print(f"  UE Version: {ue_ver}")

    if json_to_uasset(uassetgui, json_path, uasset_out, ue_ver):
        uexp = uasset_out.with_suffix(".uexp")
        print(f"  ✅ {uasset_out.name} ({uasset_out.stat().st_size:,} bytes)")
        if uexp.exists():
            print(f"  ✅ {uexp.name} ({uexp.stat().st_size:,} bytes)")
    else:
        print(f"  ❌ Cook failed")
        sys.exit(1)


def cmd_repack(args):
    """Repack a directory of assets into a .pak file."""
    repak = require_repak()
    source_dir = Path(args.dir).resolve()
    if not source_dir.exists():
        print(f"ERROR: Directory not found: {source_dir}")
        sys.exit(1)

    if args.output:
        out_pak = Path(args.output).resolve()
    else:
        out_pak = source_dir.parent / f"{source_dir.name}_p.pak"

    version = args.version  # e.g. "V8B" for UE4.25, "V11" for UE5

    print(f"═══ Repacking: {source_dir.name} → {out_pak.name} ═══")
    files = list(source_dir.rglob("*"))
    file_count = sum(1 for f in files if f.is_file())
    print(f"  Assets: {file_count}")

    if pak_repack(repak, source_dir, out_pak, version):
        size_mb = out_pak.stat().st_size / (1024 * 1024)
        print(f"  ✅ {out_pak} ({size_mb:.2f} MB)")
        print(f"\n  Deploy to Quest:")
        print(f"    adb push {out_pak} /sdcard/Android/data/<package>/files/UE4SS/Mods/")
    else:
        print(f"  ❌ Repack failed")
        sys.exit(1)


def cmd_patch(args):
    """Patch a single asset in a PAK: extract → replace → repack."""
    repak = require_repak()
    uassetgui = require_uassetgui()
    ue_ver = get_ue_version(args.game)

    pak_path = Path(args.pak).resolve()
    json_path = Path(args.json).resolve()
    asset_path = args.asset  # e.g. "VR4/Content/Blueprints/Debug/DebugMenu/DebugMenu"

    if not pak_path.exists():
        print(f"ERROR: PAK not found: {pak_path}")
        sys.exit(1)
    if not json_path.exists():
        print(f"ERROR: JSON not found: {json_path}")
        sys.exit(1)

    print(f"═══ Patching: {pak_path.name} ═══")
    print(f"  Asset:  {asset_path}")
    print(f"  JSON:   {json_path.name}")

    with tempfile.TemporaryDirectory(prefix="uncook_patch_") as tmpdir:
        tmp = Path(tmpdir)

        # 1. Extract original PAK
        print(f"  [1/4] Extracting PAK...")
        pak_extract(repak, pak_path, tmp / "extracted")

        # 2. Cook the new JSON → uasset/uexp
        uasset_target = tmp / "extracted" / (asset_path + ".uasset")
        print(f"  [2/4] Cooking JSON → uasset...")
        if not json_to_uasset(uassetgui, json_path, uasset_target, ue_ver):
            print(f"  ❌ Cook failed")
            sys.exit(1)

        # 3. Repack
        out_pak = pak_path.parent / f"{pak_path.stem}_patched.pak"
        print(f"  [3/4] Repacking...")
        if not pak_repack(repak, tmp / "extracted", out_pak):
            print(f"  ❌ Repack failed")
            sys.exit(1)

        # 4. Done
        size_mb = out_pak.stat().st_size / (1024 * 1024)
        print(f"  [4/4] ✅ Patched PAK: {out_pak} ({size_mb:.2f} MB)")


def cmd_roundtrip(args):
    """Roundtrip test: uasset → JSON → uasset → verify byte-identical."""
    uassetgui = require_uassetgui()
    uasset_path = Path(args.uasset).resolve()
    ue_ver = get_ue_version(args.game)

    if not uasset_path.exists():
        print(f"ERROR: File not found: {uasset_path}")
        sys.exit(1)

    print(f"═══ Roundtrip Test: {uasset_path.name} ═══")
    print(f"  UE Version: {ue_ver}")

    # Original hashes
    orig_uasset_hash = file_sha256(uasset_path)
    uexp_path = uasset_path.with_suffix(".uexp")
    orig_uexp_hash = file_sha256(uexp_path) if uexp_path.exists() else None

    with tempfile.TemporaryDirectory(prefix="uncook_rt_") as tmpdir:
        tmp = Path(tmpdir)

        # Step 1: uasset → JSON
        json_out = tmp / "export.json"
        print(f"  [1/3] Export to JSON...")
        if not uasset_to_json(uassetgui, uasset_path, json_out, ue_ver):
            print(f"  ❌ Export failed")
            sys.exit(1)
        print(f"         JSON: {json_out.stat().st_size / 1024:.1f} KB")

        # Step 2: JSON → uasset
        rt_uasset = tmp / uasset_path.name
        print(f"  [2/3] Cook from JSON...")
        if not json_to_uasset(uassetgui, json_out, rt_uasset, ue_ver):
            print(f"  ❌ Cook failed")
            sys.exit(1)

        # Step 3: Compare
        rt_hash = file_sha256(rt_uasset)
        rt_uexp = rt_uasset.with_suffix(".uexp")
        rt_uexp_hash = file_sha256(rt_uexp) if rt_uexp.exists() else None

        print(f"  [3/3] Comparing...")

        uasset_match = orig_uasset_hash == rt_hash
        uexp_match = orig_uexp_hash == rt_uexp_hash if orig_uexp_hash else True

        if uasset_match:
            print(f"  ✅ .uasset: IDENTICAL ({orig_uasset_hash[:16]}...)")
        else:
            orig_size = uasset_path.stat().st_size
            rt_size = rt_uasset.stat().st_size
            print(f"  ⚠ .uasset: DIFFERENT")
            print(f"      Original:  {orig_size:>10,} bytes  sha256={orig_uasset_hash[:16]}...")
            print(f"      Roundtrip: {rt_size:>10,} bytes  sha256={rt_hash[:16]}...")
            print(f"      Delta:     {rt_size - orig_size:>+10,} bytes")

        if uexp_match:
            print(f"  ✅ .uexp:   IDENTICAL")
        elif orig_uexp_hash:
            orig_size = uexp_path.stat().st_size
            rt_size = rt_uexp.stat().st_size if rt_uexp.exists() else 0
            print(f"  ⚠ .uexp:   DIFFERENT")
            print(f"      Original:  {orig_size:>10,} bytes")
            print(f"      Roundtrip: {rt_size:>10,} bytes")
            print(f"      Delta:     {rt_size - orig_size:>+10,} bytes")

        if uasset_match and uexp_match:
            print(f"\n  ✅ ROUNDTRIP PERFECT — asset can be safely edited and re-cooked")
        else:
            print(f"\n  ⚠ ROUNDTRIP MISMATCH — edits may work but byte-identical cooking not guaranteed")
            print(f"     This is normal for some asset types (padding, alignment differences)")


# ═══════════════════════════════════════════════════════════════════════
# CLI
# ═══════════════════════════════════════════════════════════════════════

def main():
    parser = argparse.ArgumentParser(
        description="UE4/UE5 Asset Uncooker Pipeline",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""\
Examples:
  uncook.py info                                          Show PAK info
  uncook.py extract --game re4                            Extract all RE4 PAKs
  uncook.py extract --game re4 --filter Debug             Extract only Debug assets
  uncook.py search DebugMenu --game re4                   Find DebugMenu assets
  uncook.py dump path/to/File.uasset --json               Dump to JSON
  uncook.py dump-all --game re4                           Dump all extracted assets
  uncook.py edit path/to/File.json                        Open in UAssetGUI
  uncook.py cook path/to/File.json -o File.uasset         Re-cook from JSON
  uncook.py repack path/to/dir -o mod.pak                 Repack dir → PAK
  uncook.py roundtrip path/to/File.uasset                 Verify roundtrip fidelity
  uncook.py patch orig.pak --asset VR4/Content/X --json X.json  Patch single asset
""",
    )

    sub = parser.add_subparsers(dest="command", help="Command")

    # info
    p = sub.add_parser("info", help="Show PAK file info")
    p.add_argument("--game", default="re4", choices=["re4", "pfx"])

    # list
    p = sub.add_parser("list", help="List all assets in PAKs")
    p.add_argument("--game", default="re4", choices=["re4", "pfx"])

    # search
    p = sub.add_parser("search", help="Search for assets in PAKs")
    p.add_argument("pattern", help="Regex pattern to search")
    p.add_argument("--game", default="re4", choices=["re4", "pfx"])

    # extract
    p = sub.add_parser("extract", help="Extract PAK files to raw assets")
    p.add_argument("--game", default="re4", choices=["re4", "pfx"])
    p.add_argument("--filter", help="Regex filter for asset paths")

    # dump
    p = sub.add_parser("dump", help="Dump a .uasset to JSON or text")
    p.add_argument("uasset", help="Path to .uasset file")
    p.add_argument("--json", action="store_true", help="Export to JSON via UAssetGUI")
    p.add_argument("--kismet", action="store_true", help="Decompile Kismet bytecode")
    p.add_argument("--game", default="re4", choices=["re4", "pfx"])
    p.add_argument("-o", "--output", help="Output path")

    # dump-all
    p = sub.add_parser("dump-all", help="Dump all .uasset files to JSON")
    p.add_argument("--dir", help="Directory to scan (default: extracted dir)")
    p.add_argument("--game", default="re4", choices=["re4", "pfx"])
    p.add_argument("--force", action="store_true", help="Overwrite existing JSON")

    # edit
    p = sub.add_parser("edit", help="Open JSON in UAssetGUI for editing")
    p.add_argument("file", help="Path to .json file")

    # cook
    p = sub.add_parser("cook", help="Re-cook JSON → .uasset/.uexp")
    p.add_argument("json_file", help="Path to JSON file")
    p.add_argument("--game", default="re4", choices=["re4", "pfx"])
    p.add_argument("-o", "--output", help="Output .uasset path")

    # repack
    p = sub.add_parser("repack", help="Repack directory → .pak")
    p.add_argument("dir", help="Directory to pack")
    p.add_argument("-o", "--output", help="Output .pak path")
    p.add_argument("--version", help="PAK version (e.g. V8B for UE4.25, V11 for UE5)")

    # patch
    p = sub.add_parser("patch", help="Patch single asset in a PAK")
    p.add_argument("pak", help="Path to .pak file")
    p.add_argument("--asset", required=True, help="Asset path inside PAK (no extension)")
    p.add_argument("--json", required=True, dest="json", help="Modified JSON file")
    p.add_argument("--game", default="re4", choices=["re4", "pfx"])

    # roundtrip
    p = sub.add_parser("roundtrip", help="Roundtrip verify: uasset→JSON→uasset")
    p.add_argument("uasset", help="Path to .uasset file")
    p.add_argument("--game", default="re4", choices=["re4", "pfx"])

    args = parser.parse_args()
    if not args.command:
        parser.print_help()
        sys.exit(0)

    cmds = {
        "info": cmd_info,
        "list": cmd_list,
        "search": cmd_search,
        "extract": cmd_extract,
        "dump": cmd_dump,
        "dump-all": cmd_dump_all,
        "edit": cmd_edit,
        "cook": cmd_cook,
        "repack": cmd_repack,
        "patch": cmd_patch,
        "roundtrip": cmd_roundtrip,
    }
    cmds[args.command](args)


if __name__ == "__main__":
    main()
