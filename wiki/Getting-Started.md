# Getting Started

This guide walks you through setting up Quest UE4 Modloader and creating your first mod.

## Prerequisites

| Requirement | Details |
| --- | --- |
| **Meta Quest** | Quest 2, Quest 3, or Quest Pro |
| **Developer Mode** | Enabled in the Oculus app on your phone |
| **Root Access** | Required for library injection |
| **ADB** | Android Debug Bridge (comes with Android SDK Platform-Tools) |
| **Python 3.8+** | For deployment tools (optional but recommended) |

### For Building from Source (Optional)

| Requirement | Details |
| --- | --- |
| **Android NDK** | r23c (23.1.7779620) |
| **CMake** | 3.22 or newer |
| **Ninja** | Latest version |

## Installation

### Step 1: Download

Get the latest release from [GitHub Releases](https://github.com/xAstroBoy/quest-ue4-modloader/releases).

The release zip contains:
```
quest-ue4-modloader-vX.Y.Z/
├── libmodloader.so     # The modloader binary
├── mods/               # Included mods
├── examples/           # Example mods for learning
├── tools/deploy.py     # Deployment tool
├── LUA_API.md          # API reference
└── README.md
```

### Step 2: Connect Your Quest

```bash
# USB connection
adb devices
# Should show your Quest serial number

# Wireless connection (optional)
adb tcpip 5555
adb connect <quest-ip>:5555
```

### Step 3: Deploy

**Option A: Using the deploy script (recommended)**

```bash
# Edit tools/deploy.py to set your device IP/serial, then:
python tools/deploy.py all       # Push modloader + all mods
python tools/deploy.py launch    # Launch the game
```

**Option B: Manual ADB push**

```bash
# Push the modloader
adb push libmodloader.so /sdcard/UE4Mods/libmodloader.so

# Push mods
adb push mods/ /sdcard/UE4Mods/mods/

# Launch the game
adb shell am force-stop com.Armature.VR4
adb shell am start com.Armature.VR4/com.epicgames.ue4.SplashActivity
```

### Step 4: Verify

```bash
# Check logs for successful loading
python tools/deploy.py log

# Or manually:
adb pull /sdcard/UE4Mods/UEModLoader.log
```

You should see lines like:
```
[ModLoader] v3.0.0-arm64 initialized
[ModLoader] Loaded mod: GodMode (v7.0)
[ModLoader] Loaded mod: DebugMenuAPI (v20.0)
...
```

## Your First Mod

### Step 1: Create the mod folder

Create `mods/MyFirstMod/main.lua`:

```lua
-- mods/MyFirstMod/main.lua
Log("MyFirstMod: Hello from my first mod!")

-- Wait for the game to fully load, then do something
ExecuteWithDelay(5000, function()
    local pc = FindFirstOf("PlayerController")
    if pc and pc:IsValid() then
        Log("MyFirstMod: Found player: " .. pc:GetName())
    end
end)
```

### Step 2: Deploy

```bash
python tools/deploy.py mods MyFirstMod
python tools/deploy.py launch
```

### Step 3: Check the log

```bash
python tools/deploy.py log
# Look for "MyFirstMod: Hello from my first mod!"
```

### Step 4: Live testing with the bridge

```bash
python tools/deploy.py forward    # Set up port forwarding
python tools/deploy.py console    # Open interactive console

> exec_lua return "Hello from bridge!"
> exec_lua local pc = FindFirstOf("PlayerController"); return pc:GetName()
```

## Next Steps

- Read [Creating Mods](Creating-Mods.md) for the full development guide
- Browse the [Lua API Reference](Lua-API-Reference.md) for all available functions
- Check the [examples/](../examples/) directory for more patterns
- Set up the [Debug Menu API](Debug-Menu-API.md) for in-game toggles

## Directory Structure

After installation, your device should have:
```
/sdcard/UE4Mods/
├── libmodloader.so          # Injected at game launch
├── UEModLoader.log          # Runtime log
├── config/                  # ModConfig JSON files
│   ├── GodMode.json
│   └── ...
├── mods/
│   ├── DebugMenuAPI/
│   │   └── main.lua
│   ├── GodMode/
│   │   └── main.lua
│   ├── MyFirstMod/
│   │   └── main.lua
│   └── ...
└── sdk/                     # Auto-generated SDK dump
    ├── Classes/
    ├── Structs/
    └── Enums/
```
