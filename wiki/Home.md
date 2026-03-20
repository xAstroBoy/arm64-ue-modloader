# Home

Welcome to the **Quest UE4 Modloader** wiki — a universal Lua modding framework for Unreal Engine 4 games on Meta Quest.

## What is this?

Quest UE4 Modloader is a C++ shared library (`libmodloader.so`) that injects into UE4 games on Meta Quest headsets, providing:

- **Lua 5.4 scripting** — Write mods in Lua with a rich API
- **UE4 reflection access** — Read/write any property, call any function
- **ProcessEvent hooking** — Intercept any Blueprint or C++ UFunction
- **Native ARM64 hooking** — Hook stripped C++ functions with Dobby
- **Live debugging** — TCP bridge for real-time code execution via ADB
- **PAK injection** — Mount custom content packs before engine init
- **SDK generation** — Auto-dump all game classes, structs, and enums

## Quick Navigation

| Page | Description |
| --- | --- |
| [Getting Started](Getting-Started.md) | Setup, installation, your first mod |
| [Creating Mods](Creating-Mods.md) | Mod development guide with examples |
| [Lua API Reference](Lua-API-Reference.md) | Complete API documentation |
| [Debug Menu API](Debug-Menu-API.md) | In-game mod menu system |
| [Architecture](Architecture.md) | Internal design and components |
| [Porting Guide](Porting-Guide.md) | Adapt to other UE4 Quest games |
| [Troubleshooting](Troubleshooting.md) | Common problems and solutions |

## Supported Games

| Game | Status |
| --- | --- |
| Resident Evil 4 VR | ✅ Fully supported |
| Other UE4 Quest titles | 🔄 Universal design — see [Porting Guide](Porting-Guide.md) |

## Community

- [GitHub Issues](https://github.com/xAstroBoy/quest-ue4-modloader/issues) — Bug reports & feature requests
- [GitHub Discussions](https://github.com/xAstroBoy/quest-ue4-modloader/discussions) — Questions & community
