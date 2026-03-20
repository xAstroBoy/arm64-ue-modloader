# Changelog

All notable changes to Quest UE4 Modloader will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

### Added
- Public GitHub repository with CI/CD
- GitHub Actions auto-build for ARM64
- Example mods for learning
- Wiki documentation
- Contributing guidelines

## [3.0.0] - 2026-03-20

### Added
- **LuaUStruct** — full struct support with `__index`/`__newindex` field access
  - `FVector`, `FRotator`, `IntPoint`, `Vector2D` etc. returned as userdata
  - Direct field read/write: `struct.X`, `struct.Y = 500`
  - Methods: `Clone()`, `CopyFrom()`, `GetFields()`, `GetTypeName()`, `GetSize()`
- **FText full support** — read returns string, write accepts string
- **Call() with table args** — `obj:Call("SetDrawSize", {X=500, Y=2000})` works
- **Set() with table args** — `obj:Set("DrawSize", {X=500, Y=2000})` works
- **Enum API** — `Enums.*` global table, `FindEnum`, `GetEnumTable`, `AppendEnumValue`
- **CreateWidget()** — proper UMG widget factory via WidgetBlueprintLibrary
- **DebugMenuAPI v20** — cooperative enumless design, SharedAPI for all mods
- **Native hook crash guard** — Dobby SIGSEGV caught gracefully
- **Pattern scanner** — ARM64 byte pattern scanning for stripped symbols
- **Object monitor** — `NotifyOnNewObject` with class filtering

### Changed
- Modloader architecture: sol2 Lua 5.4 replaces Frida JavaScript
- All UObject access via reflection API (no raw memory offsets)
- TArray is now 1-indexed (Lua convention)
- ProcessEvent hooks use UE4SS-compatible API signatures

### Fixed
- Crash on GC'd UObject access in hooks (null-safe msync probe)
- Memory read/write functions are null-safe on ARM64
- PAK mounting race condition with engine init

## [2.0.0] - 2026-01-15

### Added
- Dobby ARM64 inline hooking for native C++ functions
- `RegisterNativeHook` / `RegisterNativeHookAt` Lua API
- `CallNative` / `CallNativeBySymbol` for calling native functions
- PAK mounter — inject custom .pak before engine init
- ADB bridge — TCP JSON protocol on port 19420
- `exec_lua` bridge command for live code execution
- SDK dumper — auto-generate class/struct/enum dumps
- Deploy script (`tools/deploy.py`) with 12+ commands

### Changed
- Switched from Frida to native C++ modloader (`libmodloader.so`)
- Full rewrite of hooking system

## [1.0.0] - 2025-10-01

### Added
- Initial Lua modding framework
- ProcessEvent hooking via Dobby
- Basic UObject reflection (`Get`/`Set`/`Call`)
- `FindFirstOf` / `FindAllOf` object finding
- Timer system (`ExecuteWithDelay`, `LoopAsync`)
- `ModConfig` JSON persistence
- Mod auto-loading from `mods/<Name>/main.lua`
- Basic logging system

[Unreleased]: https://github.com/xAstroBoy/quest-ue4-modloader/compare/v3.0.0...HEAD
[3.0.0]: https://github.com/xAstroBoy/quest-ue4-modloader/compare/v2.0.0...v3.0.0
[2.0.0]: https://github.com/xAstroBoy/quest-ue4-modloader/compare/v1.0.0...v2.0.0
[1.0.0]: https://github.com/xAstroBoy/quest-ue4-modloader/releases/tag/v1.0.0
