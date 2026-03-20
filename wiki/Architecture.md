# Architecture

This document explains how Quest UE4 Modloader works internally.

## Overview

```
┌─────────────────────────────────────────────────────────────┐
│                      Meta Quest Device                       │
│                                                              │
│  ┌──────────────────────────────────────────────────────┐   │
│  │               UE4 Game Process                        │   │
│  │                                                       │   │
│  │  ┌─────────────┐  JNI_OnLoad  ┌──────────────────┐  │   │
│  │  │ Game .so     │ ──────────►  │ libmodloader.so  │  │   │
│  │  │ (UE4 engine) │              │                  │  │   │
│  │  └──────┬───────┘              │  ┌────────────┐  │  │   │
│  │         │                      │  │ Lua Engine │  │  │   │
│  │         │ ProcessEvent         │  │ (sol2+5.4) │  │  │   │
│  │         │ hooks (Dobby)        │  └─────┬──────┘  │  │   │
│  │         │◄─────────────────────│        │         │  │   │
│  │         │                      │  ┌─────▼──────┐  │  │   │
│  │         │                      │  │   Mods     │  │  │   │
│  │         │                      │  │ main.lua   │  │  │   │
│  │         │                      │  │ main.lua   │  │  │   │
│  │         │                      │  └────────────┘  │  │   │
│  │         │                      │                  │  │   │
│  │         │         TCP:19420    │  ┌────────────┐  │  │   │
│  │         │       ◄──────────────│  │ ADB Bridge │  │  │   │
│  │         │                      │  └────────────┘  │  │   │
│  │         │                      └──────────────────┘  │   │
│  └─────────┴────────────────────────────────────────────┘   │
│                              ▲                               │
│                              │ ADB                           │
└──────────────────────────────┼───────────────────────────────┘
                               │
                        ┌──────▼──────┐
                        │  PC Tools   │
                        │  deploy.py  │
                        │  bridge     │
                        └─────────────┘
```

## Boot Sequence

The modloader starts via two threads launched from `JNI_OnLoad`:

### Thread 1: Early PAK Hook (immediate)
1. `JNI_OnLoad` called when the .so is loaded
2. Hooks `FPakPlatformFile::Mount` via Dobby
3. Mounts custom .pak files before engine processes them
4. This runs before UE4 initializes assets

### Thread 2: Main Init (5-second delay)
1. Waits ~5 seconds for UE4 engine to initialize
2. Resolves symbols (`GUObjectArray`, `FName::Init`, `ProcessEvent`, etc.)
3. Installs ProcessEvent Dobby hook
4. Initializes Lua engine (sol2 + Lua 5.4)
5. Registers all Lua bindings
6. Starts ADB bridge server (TCP port 19420)
7. Discovers and loads all mods from `mods/*/main.lua`

## Component Details

### Core (`src/core/`)

| File | Purpose |
| --- | --- |
| `init.cpp` | Main initialization, thread management |
| `config.cpp` | Configuration loading |
| `symbols.cpp` | UE4 symbol resolution (dlsym, ELF phdr, pattern scan) |
| `paths.cpp` | Path management for mods, config, SDK |
| `pattern_scanner.cpp` | ARM64 byte pattern scanning for stripped symbols |
| `object_monitor.cpp` | NotifyOnNewObject — watches GUObjectArray for spawns |

### Hook System (`src/hook/`)

| File | Purpose |
| --- | --- |
| `process_event_hook.cpp` | Main ProcessEvent Dobby hook — dispatches to Lua callbacks |
| `pe_trace.cpp` | ProcessEvent tracing for debugging (logs all UFunction calls) |
| `native_hooks.cpp` | RegisterNativeHook — Dobby inline hooks on arbitrary ARM64 functions |

**ProcessEvent Hook Flow:**
```
UObject::ProcessEvent(UFunction*, void* Parms)
  │
  ├─► Check hook registry for this UFunction
  │     ├─► Run pre-hooks (Lua callbacks)
  │     │     └─► If any returns "BLOCK" → skip original
  │     ├─► Call original ProcessEvent (if not blocked)
  │     └─► Run post-hooks (Lua callbacks)
  │
  └─► Return
```

### Lua Engine (`src/lua/`)

| File | Purpose |
| --- | --- |
| `lua_engine.cpp` | sol2 state creation, mod loading |
| `lua_bindings.cpp` | Register all Lua API functions |
| `lua_uobject.cpp` | UObject userdata with `__index`/`__newindex`/`__call` metamethods |
| `lua_types.cpp` | FName, FString, FText Lua wrappers |
| `lua_ustruct.cpp` | LuaUStruct — struct field access via reflection |
| `lua_tarray.cpp` | TArray Lua wrapper (1-indexed) |
| `lua_enums.cpp` | Enum loading and Enums global table |
| `lua_enum_ext.cpp` | AppendEnumValue — runtime enum extension |
| `lua_ue4ss_types.cpp` | UE4SS-compatible type wrappers |
| `lua_ue4ss_globals.cpp` | UE4SS-compatible global functions |
| `lua_delayed_actions.cpp` | Timer system (ExecuteWithDelay, LoopAsync, etc.) |

### Reflection (`src/reflection/`)

| File | Purpose |
| --- | --- |
| `reflection_walker.cpp` | Walk UE4 reflection tree (classes → properties → functions) |
| `lua_dump_generator.cpp` | Generate SDK dump (Lua files for each class/struct/enum) |
| `class_rebuilder.cpp` | RebuildClass API — high-level class wrappers |

### Other Components

| Directory | Purpose |
| --- | --- |
| `src/mods/` | Mod discovery and loading |
| `src/pak/` | PAK file mounting before engine init |
| `src/bridge/` | ADB TCP bridge (JSON protocol, port 19420) |
| `src/util/` | Logger, crash handler (SIGSEGV/SIGABRT), UMG notifications |

## UObject Reflection

The modloader accesses game objects entirely through UE4's built-in reflection system:

```
UObject
  └─► UClass (GetClass())
        └─► FProperty chain (linked list)
              ├─► FBoolProperty
              ├─► FIntProperty
              ├─► FFloatProperty
              ├─► FStrProperty (FString)
              ├─► FTextProperty (FText)
              ├─► FNameProperty (FName)
              ├─► FStructProperty → UScriptStruct → nested FProperty chain
              ├─► FObjectProperty → UObject*
              ├─► FArrayProperty → TArray<T>
              ├─► FMapProperty → TMap<K,V>
              ├─► FEnumProperty → UEnum
              └─► ...
```

`Get()` walks the property chain, finds the property by name, reads the value at the correct offset, and converts to the appropriate Lua type.

`Set()` does the reverse — converts from Lua to the UE4 type and writes to the property offset.

`Call()` finds the UFunction, allocates a parameter buffer, fills parameters from Lua args (including struct serialization from tables), and calls `ProcessEvent`.

## Third-Party Libraries

| Library | Version | Purpose | License |
| --- | --- | --- | --- |
| [Lua](https://www.lua.org/) | 5.4 | Scripting language runtime | MIT |
| [sol2](https://github.com/ThePhD/sol2) | 3.x | C++↔Lua binding library | MIT |
| [Dobby](https://github.com/jmpews/Dobby) | latest | ARM64 inline hooking framework | Apache 2.0 |
| [nlohmann/json](https://github.com/nlohmann/json) | 3.x | JSON parsing/generation | MIT |

## Memory Safety

- All memory read/write functions use `msync` probing on ARM64
- Invalid pointer reads return 0 instead of crashing
- UObject validity is checked via `IsValid()` before access
- Dobby hooks have crash guards (SIGSEGV handler)
- Lua `pcall` isolates mod errors from crashing the game

## Bridge Protocol

The ADB bridge listens on TCP port 19420 (localhost only, forwarded via ADB).

**Request format (JSON, newline-terminated):**
```json
{"cmd": "exec_lua", "code": "return FindFirstOf('PlayerController'):GetName()"}
```

**Response format (JSON):**
```json
{"ok": true, "result": "VR4PlayerController_BP_C_0"}
```

Built-in commands: `ping`, `exec_lua`, `list_mods`, `dump_sdk`, plus any registered via `RegisterBridgeCommand`.
