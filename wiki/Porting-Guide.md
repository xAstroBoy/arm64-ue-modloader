# Porting Guide

This guide covers how to adapt Quest UE4 Modloader for other Unreal Engine 4 games on Meta Quest.

## What's Universal vs Game-Specific

### Universal (works on any UE4 Quest game)

- **Reflection system** — UE4's reflection is consistent across all UE4 titles
- **ProcessEvent hooking** — same virtual function table layout
- **Lua API** — `Get`/`Set`/`Call`/`FindFirstOf` etc. are game-agnostic
- **sol2/Lua 5.4** — scripting runtime
- **Dobby ARM64 hooking** — instruction-level hooks work on any ARM64 binary
- **Timer system** — all timer functions
- **File I/O / ModConfig** — filesystem operations
- **TArray / TMap / FName / FString / FText** — UE4 core types

### Game-Specific (needs adaptation)

| Component | What to Change |
| --- | --- |
| **Symbol addresses** | Different games have different binaries |
| **Package name** | `com.Armature.VR4` → your game's package |
| **Library name** | The main game .so to inject into |
| **GUObjectArray offset** | Found via pattern scan or analysis |
| **FName::Init** | Symbol or pattern for FName construction |
| **ProcessEvent** | Virtual function index (usually consistent) |
| **Mod scripts** | Hooks reference game-specific classes |
| **PAK files** | Game-specific content mods |

## Porting Steps

### Step 1: Identify the Target

1. Install the game on your Quest
2. Find the package name: `adb shell pm list packages | grep <keyword>`
3. Find the game library: `adb shell ls /data/app/<package>/lib/arm64/`
4. The main library is usually `libUE4.so` or similar

### Step 2: Update Paths

In `src/core/paths.cpp`, update:
```cpp
// Package name for the target game
static const char* PACKAGE_NAME = "com.your.game";

// Mod directory on device
static const char* MOD_DIR = "/sdcard/UE4Mods/";
```

In `tools/deploy.py`, update:
```python
PACKAGE = "com.your.game"
```

### Step 3: Resolve Symbols

The modloader needs these key symbols/addresses:

| Symbol | Purpose | How to Find |
| --- | --- | --- |
| `GUObjectArray` | Object iteration | Pattern scan or Ghidra/IDA |
| `FName::Init` | FName construction | `dlsym` or pattern |
| `ProcessEvent` | Function hooks | VTable offset (usually index 68) |
| `StaticFindObject` | Object lookup | `dlsym` or pattern |
| `GWorld` | World context | Pattern scan |

The modloader uses a resolution chain: `dlsym` → ELF phdr scan → pattern scan → fallback.

Update patterns in `src/core/symbols.cpp` for your target binary.

### Step 4: Generate SDK

Once the modloader boots and can access `GUObjectArray`:

```bash
python tools/deploy.py console
> exec_lua DumpSDK()
> exit
python tools/deploy.py sdk    # Pull generated SDK
```

This creates class/struct/enum dumps for the target game.

### Step 5: Write Game-Specific Mods

With the SDK dump, you can see all classes and their properties/functions. Write mods using the standard Lua API:

```lua
-- Example: Find game-specific class
local player = FindFirstOf("YourGamePlayerCharacter")
if player and player:IsValid() then
    -- Use SDK to know property names
    local health = player:Get("Health")
    Log("Health: " .. tostring(health))
end
```

### Step 6: Update Deploy Tool

Edit `tools/deploy.py` to match your game:
- Package name
- Device IP (if different)
- Mod directory paths
- Launch activity name

## Tips for New Games

1. **Start with logging** — Get the modloader to boot and produce logs first
2. **Dump the SDK early** — The class dump tells you everything about the game
3. **Use the bridge** — `exec_lua` lets you explore interactively
4. **Pattern scan** — If symbols are stripped, use `FindPattern` for key functions
5. **Check UE4 version** — The modloader targets UE4.25; other versions may have different struct layouts
6. **VTable indices** — ProcessEvent's VTable index may differ between UE4 versions

## Known Compatible UE4 Versions

| UE4 Version | Status | Notes |
| --- | --- | --- |
| 4.25 | ✅ Tested | RE4 VR |
| 4.26-4.27 | 🔄 Should work | Minor offset differences possible |
| 5.x | ⚠️ Untested | Significant struct layout changes likely |

## Community Ports

If you successfully port to a new game, please consider:
1. Opening a PR with your changes
2. Documenting game-specific symbols/patterns
3. Sharing your SDK dump (if the game allows it)
4. Adding the game to the supported games table

This helps the community and makes the modloader truly universal!
