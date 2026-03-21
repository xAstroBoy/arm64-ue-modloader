<p align="center">
  <img src="docs/assets/banner.png" alt="Quest Modloader Banner" width="800"/>
</p>

<h1 align="center">Quest UE4 Modloader</h1>

<p align="center">
  <strong>A universal Lua modding framework for Unreal Engine 4 games on Meta Quest</strong>
</p>

<p align="center">
  <a href="https://github.com/xAstroBoy/quest-ue4-modloader/actions/workflows/build.yml"><img src="https://github.com/xAstroBoy/quest-ue4-modloader/actions/workflows/build.yml/badge.svg" alt="Build"></a>
  <a href="https://github.com/xAstroBoy/quest-ue4-modloader/releases/latest"><img src="https://img.shields.io/github/v/release/xAstroBoy/quest-ue4-modloader?include_prereleases&label=release" alt="Release"></a>
  <a href="LICENSE"><img src="https://img.shields.io/github/license/xAstroBoy/quest-ue4-modloader" alt="License"></a>
  <a href="https://github.com/xAstroBoy/quest-ue4-modloader/wiki"><img src="https://img.shields.io/badge/docs-wiki-blue" alt="Wiki"></a>
</p>

<p align="center">
  <a href="#features">Features</a> •
  <a href="#quick-start">Quick Start</a> •
  <a href="#building">Building</a> •
  <a href="#creating-mods">Creating Mods</a> •
  <a href="#lua-api">Lua API</a> •
  <a href="#wiki">Wiki</a> •
  <a href="#contributing">Contributing</a>
</p>

---

## Overview

**Quest UE4 Modloader** is a C++ injection framework that adds [UE4SS](https://github.com/UE4SS-RE/RE-UE4SS)-compatible Lua scripting to Unreal Engine 4 games running on Meta Quest (Android ARM64). Originally built for **Resident Evil 4 VR**, it is designed to be **universal** — adaptable to any UE4 title on Quest with minimal changes.

The modloader is injected as a shared library (`libmodloader.so`) and provides:

- **Full UE4 reflection access** — read/write any UObject property, call any UFunction
- **Lua 5.4 scripting** — write mods in Lua with a rich, documented API
- **Hot-reload** — push mod changes without rebuilding the modloader
- **Live debugging** — TCP bridge for real-time `exec_lua` commands via ADB
- **ProcessEvent hooking** — pre/post hooks on any Blueprint or native UFunction
- **Native ARM64 hooking** — Dobby-powered inline hooks on stripped native functions
- **PAK mounting** — inject custom .pak content before engine initialization
- **SDK generation** — auto-dump all classes, structs, and enums from the running game

## Features

### Core Engine
| Feature | Description |
|---|---|
| 🔧 **UObject Reflection** | Read/write properties, call functions via `Get`/`Set`/`Call` |
| 🪝 **ProcessEvent Hooks** | Pre/post hooks on any UFunction — block, modify, or observe |
| 🔩 **Native Hooks** | Dobby ARM64 inline hooks for stripped C++ functions |
| 📦 **PAK Mounting** | Mount custom .pak files before engine init |
| 🧩 **Mod Loader** | Auto-discovers and loads Lua mods from `mods/<Name>/main.lua` |
| 🌐 **ADB Bridge** | TCP JSON bridge (port 19420) for live `exec_lua` and commands |
| 📋 **SDK Dumper** | Auto-generates full SDK (classes/structs/enums) from reflection |

### Lua API Highlights
| Feature | Description |
|---|---|
| 🔍 `FindFirstOf` / `FindAllOf` | Find live UObject instances by class name |
| 📝 `obj:Get` / `obj:Set` | Read/write properties via reflection |
| 📞 `obj:Call` | Invoke UFunctions via ProcessEvent |
| 🏗️ `CreateWidget` | Create UMG widgets via WidgetBlueprintLibrary |
| 📐 **LuaUStruct** | Full struct support (FVector, FRotator, etc.) with field access |
| ⏱️ Timers | `ExecuteWithDelay`, `LoopAsync`, `ExecuteInGameThread` |
| 💾 `ModConfig` | Per-mod JSON config persistence |
| 🔗 `SharedAPI` | Cross-mod communication table |

### Developer Tools
| Tool | Description |
|---|---|
| `deploy.py` | All-in-one deploy script (mods, modloader, logs, launch, SDK) |
| Bridge Console | Interactive REPL for live Lua execution on device |
| SDK Dump | Full class/struct/enum dump for IntelliSense |

## Quick Start

### Prerequisites

- **Meta Quest** (Quest 2/3/Pro) with developer mode enabled
- **ADB** installed and device connected (USB or wireless)
- *APK Patching* Required to load libmodloader.so 
- **Android NDK r23c** (for building from source — see [Building](#building))
- **Python 3.8+** (for deployment tools)

### Installation

1. **Download** the latest release from [Releases](https://github.com/xAstroBoy/quest-ue4-modloader/releases)
2. **Extract** the archive — you'll get `libmodloader.so` and the `mods/` folder
3. **Push** to your Quest:
   ```bash
   adb push libmodloader.so /sdcard/UE4Mods/libmodloader.so
   adb push mods/ /sdcard/UE4Mods/mods/
   ```
4. **Launch** the game — mods load automatically

### Deploy Script (Recommended)

```bash
# Configure your device in tools/deploy.py, then:
python tools/deploy.py all       # Push modloader + all mods
python tools/deploy.py launch    # Kill + relaunch game
python tools/deploy.py log       # Pull latest log
python tools/deploy.py console   # Interactive bridge REPL
```

## Building

### Requirements

- **CMake** 3.22+
- **Ninja** build system
- **Android NDK** r23c (23.1.7779620)
- C++17 compiler (provided by NDK)

### Build from Source

```bash
# Clone with submodules
git clone --recursive https://github.com/xAstroBoy/quest-ue4-modloader.git
cd quest-ue4-modloader

# Set NDK path (or edit modloader/build.bat)
set NDK=C:\Android\ndk\23.1.7779620

# Build
cd modloader
.\build.bat          # Windows
# OR
./build.sh           # Linux/macOS (CI)
```

The output `libmodloader.so` will be in `modloader/build/`.

### GitHub Actions

Every push to `main` and every pull request triggers an automatic build via GitHub Actions. Pre-built binaries are attached to every [Release](https://github.com/xAstroBoy/quest-ue4-modloader/releases).

## Creating Mods

Mods are Lua scripts placed in `mods/<ModName>/main.lua`. They're loaded automatically on game start.

### Minimal Example

```lua
-- mods/HelloWorld/main.lua
Log("Hello from HelloWorld mod!")

-- Find a game object
local player = FindFirstOf("PlayerController")
if player and player:IsValid() then
    Log("Player found: " .. player:GetName())
end
```

### Toggle Mod (with Debug Menu)

```lua
-- mods/MyToggle/main.lua
local enabled = false

-- Register in the debug menu (requires DebugMenuAPI mod)
if SharedAPI and SharedAPI.DebugMenu then
    SharedAPI.DebugMenu.RegisterToggle("MyToggle", "My Feature", false, function(state)
        enabled = state
        Log("MyToggle: " .. (state and "ON" or "OFF"))
    end)
end

-- Hook a game function
RegisterPostHook("/Script/Game.MyClass:MyFunction", function(self, func, parms)
    if not enabled then return end
    -- Modify behavior when enabled
    local obj = self:get()
    obj:Set("SomeProperty", 42)
end)
```

### ProcessEvent Hook

```lua
-- Pre-hook: return "BLOCK" to prevent the original from running
RegisterPreHook("/Script/Game.DamageSystem:ApplyDamage", function(self, func, parms)
    return "BLOCK"  -- Block all damage
end)

-- Post-hook: modify return values or read results
RegisterPostHook("/Script/Game.Player:GetHealth", function(self, func, parms)
    WriteU8(parms, 100)  -- Override return to always 100
end)
```

See the [examples/](examples/) directory and the [Wiki](https://github.com/xAstroBoy/quest-ue4-modloader/wiki) for more.

## Lua API

Full API reference: **[docs/LUA_API.md](docs/LUA_API.md)** | **[Wiki: Lua API](https://github.com/xAstroBoy/quest-ue4-modloader/wiki/Lua-API-Reference)**

<details>
<summary><strong>API Categories (click to expand)</strong></summary>

| # | Category | Key Functions |
|---|---|---|
| 1 | Logging | `Log`, `LogWarn`, `LogError`, `print` |
| 2 | Notifications | `Notify(title, body)` |
| 3 | Object Finding | `FindFirstOf`, `FindAllOf`, `FindClass`, `FindObject` |
| 4 | Construction | `ConstructObject`, `CreateWidget` |
| 5 | UObject Methods | `Get`, `Set`, `Call`, `IsValid`, `GetName`, `GetClass` |
| 6 | ProcessEvent Hooks | `RegisterPreHook`, `RegisterPostHook`, `RegisterHook` |
| 7 | Native Hooks | `RegisterNativeHook`, `CallNative` |
| 8 | Memory R/W | `ReadU8`–`ReadU64`, `WriteU8`–`WriteU64`, `ReadFloat` |
| 9 | Timers | `ExecuteWithDelay`, `LoopAsync`, `ExecuteInGameThread` |
| 10 | Structs | LuaUStruct — `Clone`, `CopyFrom`, `GetFields`, field access |
| 11 | TArray | 1-indexed, `ForEach`, `Add`, `#arr` |
| 12 | Enums | `Enums.*`, `FindEnum`, `GetEnumTable`, `AppendEnumValue` |
| 13 | Config | `ModConfig.Load`, `ModConfig.Save` |
| 14 | File I/O | `ReadTextFile`, `WriteTextFile`, `FileExists` |
| 15 | Bridge | `RegisterBridgeCommand` |

</details>

## Project Structure

```
quest-ue4-modloader/
├── modloader/                  # C++ modloader core
│   ├── CMakeLists.txt          # CMake build config
│   ├── build.bat / build.sh    # Build scripts
│   ├── src/                    # Source files
│   │   ├── main.cpp            # Entry point (JNI_OnLoad)
│   │   ├── core/               # Init, config, symbols, pattern scanner
│   │   ├── hook/               # ProcessEvent + native Dobby hooks
│   │   ├── lua/                # Lua 5.4 bindings (sol2)
│   │   ├── reflection/         # UE4 reflection walker, SDK dump
│   │   ├── mods/               # Mod discovery and loading
│   │   ├── pak/                # Custom PAK mounting
│   │   ├── bridge/             # ADB TCP bridge
│   │   └── util/               # Logger, crash handler, notifications
│   ├── include/                # Header files
│   └── third_party/            # Vendored deps (Lua 5.4, sol2, Dobby, nlohmann/json)
│
├── mods/                       # Lua mods (each in own folder)
│   ├── DebugMenuAPI/           # In-game mod menu system
│   ├── GodMode/                # Invincibility
│   ├── NoRecoil/               # Remove weapon recoil
│   └── ...                     # 21 mods included
│
├── examples/                   # Example mods for learning
│   ├── HelloWorld/
│   ├── SimpleToggle/
│   ├── PropertyHook/
│   └── NativeHook/
│
├── tools/                      # Python deployment & testing tools
│   └── deploy.py               # Main deploy/test/console tool
│
├── docs/                       # Documentation
│   └── LUA_API.md              # Complete API reference
│
└── wiki/                       # GitHub Wiki source pages
```

## Supported Games

| Game | Platform | Status |
|---|---|---|
| **Resident Evil 4 VR** | Quest 2/3 | ✅ Fully supported (primary target) |
| Other UE4 Quest titles | Quest 2/3 | 🔄 Adaptable (universal design) |

> **Making it universal:** The modloader's core (reflection, hooks, Lua bindings) is game-agnostic. Game-specific parts are limited to symbol addresses and mod scripts. See the [Wiki: Porting Guide](https://github.com/xAstroBoy/quest-ue4-modloader/wiki/Porting-Guide) for adapting to other titles.

## Wiki

The **[Wiki](https://github.com/xAstroBoy/quest-ue4-modloader/wiki)** contains detailed documentation:

| Page | Description |
|---|---|
| [Home](https://github.com/xAstroBoy/quest-ue4-modloader/wiki) | Overview and navigation |
| [Getting Started](https://github.com/xAstroBoy/quest-ue4-modloader/wiki/Getting-Started) | Setup, installation, first mod |
| [Lua API Reference](https://github.com/xAstroBoy/quest-ue4-modloader/wiki/Lua-API-Reference) | Complete API documentation |
| [Creating Mods](https://github.com/xAstroBoy/quest-ue4-modloader/wiki/Creating-Mods) | Mod development guide |
| [Architecture](https://github.com/xAstroBoy/quest-ue4-modloader/wiki/Architecture) | How the modloader works internally |
| [Debug Menu API](https://github.com/xAstroBoy/quest-ue4-modloader/wiki/Debug-Menu-API) | In-game menu system for mods |
| [Porting Guide](https://github.com/xAstroBoy/quest-ue4-modloader/wiki/Porting-Guide) | Adapting to other UE4 Quest games |
| [Troubleshooting](https://github.com/xAstroBoy/quest-ue4-modloader/wiki/Troubleshooting) | Common issues and solutions |

## Contributing

Contributions are welcome! Please read [CONTRIBUTING.md](CONTRIBUTING.md) before submitting a PR.

- 🐛 **Bug reports** — use the [Bug Report](https://github.com/xAstroBoy/quest-ue4-modloader/issues/new?template=bug_report.yml) template
- ✨ **Feature requests** — use the [Feature Request](https://github.com/xAstroBoy/quest-ue4-modloader/issues/new?template=feature_request.yml) template
- 🔧 **Mod submissions** — PRs welcome for new example mods
- 📝 **Documentation** — Wiki improvements always appreciated

## License

This project is licensed under the **MIT License** — see [LICENSE](LICENSE) for details.

## Credits

- **[Dobby](https://github.com/jmpews/Dobby)** — ARM64 inline hooking framework
- **[sol2](https://github.com/ThePhD/sol2)** — C++/Lua binding library
- **[Lua 5.4](https://www.lua.org/)** — Scripting language
- **[nlohmann/json](https://github.com/nlohmann/json)** — JSON library
- **[UE4SS](https://github.com/UE4SS-RE/RE-UE4SS)** — Inspiration for the Lua API design

---

<p align="center">
  Made with ❤️ for the Quest modding community
</p>
