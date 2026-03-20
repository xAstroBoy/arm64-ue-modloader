# RE4 VR Modloader — Complete Lua API Reference

> **Modloader**: C++ sol2 Lua 5.4 + Dobby hooks on ARM64 Android  
> **Game**: RE4 VR (com.Armature.VR4) — UE4.25 — Meta Quest 3  
> **Updated**: Auto-generated from modloader source analysis

---

## 1. Logging
| Function | Signature | Description |
|---|---|---|
| `Log` | `Log(msg: string)` | Info-level log |
| `LogWarn` | `LogWarn(msg: string)` | Warning log |
| `LogError` | `LogError(msg: string)` | Error log |
| `print` | `print(...)` | Tab-separated, auto-formats UObject/lightuserdata/bool/nil |

## 2. Notifications
| Function | Signature | Description |
|---|---|---|
| `Notify` | `Notify(title, body, id?)` | Post a notification |

## 3. Object Finding & Loading
| Function | Signature | Returns | Description |
|---|---|---|---|
| `FindClass` | `FindClass(name)` | UObject\|nil | Find UClass by name |
| `FindObject` | `FindObject(name)` | UObject\|nil | Find UObject by short name |
| `StaticFindObject` | `StaticFindObject(name)` | UObject\|nil | Full path lookup (wide) |
| `StaticFindObjectEx` | `StaticFindObjectEx(class, outer, name, exact?)` | UObject\|nil | Extended find |
| `StaticLoadObject` | `StaticLoadObject(name)` | UObject\|nil | Load UClass |
| `LoadObject` | `LoadObject(name)` | UObject\|nil | Load UObject |
| `FindFirstOf` | `FindFirstOf(className)` | UObject\|nil | **MOST USED** — first live instance |
| `FindAllOf` | `FindAllOf(className)` | table\|nil | All live instances |
| `FindObjects` | `FindObjects(num?, className, reqFlags?, banFlags?)` | table\|nil | Flag-filtered search |
| `LoadAsset` / `LoadExport` | `LoadAsset(path)` | UObject\|nil | Load asset |
| `FindEnum` | `FindEnum(name)` | UObject\|nil | Find UEnum |
| `FindStruct` | `FindStruct(name)` | UObject\|nil | Find UScriptStruct |
| `GetCDO` | `GetCDO(class)` | UObject\|nil | Class Default Object |
| `GetWorldContext` | `GetWorldContext()` | UObject\|nil | Current UWorld |
| `CreateInvalidObject` | `CreateInvalidObject()` | UObject | Null-ptr wrapper |
| `UObjectFromPtr` | `UObjectFromPtr(ptr)` | UObject\|nil | Wrap raw pointer |

## 4. Object Construction
| Function | Signature | Returns |
|---|---|---|
| `ConstructObject` | `ConstructObject(class, outer?, name?, flags?, template?)` | UObject\|nil |

## 5. UObject Methods (on all instances)
| Method | Signature | Returns | Description |
|---|---|---|---|
| `IsValid` | `obj:IsValid()` | bool | Non-null and valid |
| `GetName` | `obj:GetName()` | string | Short name |
| `GetFullName` | `obj:GetFullName()` | string | Full path |
| `GetClass` | `obj:GetClass()` | UObject\|nil | UClass |
| `GetClassName` | `obj:GetClassName()` | string | Class short name |
| `GetAddress` | `obj:GetAddress()` | lightuserdata | Raw pointer |
| `ToHex` | `obj:ToHex()` | string | Hex address |
| `Get` / `GetProp` | `obj:Get(propName)` | any\|nil | Read property via reflection |
| `Set` / `SetProp` | `obj:Set(propName, value)` | bool | Write property via reflection |
| `Call` / `CallFunc` | `obj:Call(funcName, args...)` | any\|nil | Call UFunction via ProcessEvent |
| `HookProp` | `obj:HookProp(propName, callback)` | uint64 | Per-instance property hook |
| `HookFunc` | `obj:HookFunc(funcName, callback)` | uint64 | Per-instance function hook |
| **Dynamic** | `obj.PropertyName` | any | `__index` reads property via reflection |
| **Dynamic** | `obj.PropertyName = val` | void | `__newindex` writes property |
| **Dynamic** | `obj:FunctionName(args...)` | any | `__index` returns callable bound function |

## 6. UObject Extension Functions
| Function | Signature | Returns | Description |
|---|---|---|---|
| `UObject_GetFName` | `(obj)` | FName\|nil | FName of object |
| `UObject_GetOuter` | `(obj)` | UObject\|nil | Outer object |
| `UObject_IsA` | `(obj, className)` | bool | Inheritance check |
| `UObject_HasAllFlags` | `(obj, flags)` | bool | All flags match |
| `UObject_HasAnyFlags` | `(obj, flags)` | bool | Any flag matches |
| `UObject_GetWorld` | `(obj)` | UObject\|nil | UWorld |
| `UObject_type` | `(obj)` | string | Type string |
| `UObject_GetPropertyValue` | `(obj, propName)` | RemoteUnrealParam\|nil | Typed param wrapper |

## 7. ProcessEvent Hooks
| Function | Signature | Returns | Description |
|---|---|---|---|
| `RegisterHook` | `RegisterHook(path, callback)` | (preId, postId) | Path: `"ClassName:FuncName"`. Callback: `fn(self, parms)` |
| `RegisterProcessEventHook` | `RegisterProcessEventHook(path, preFn, postFn)` | void | Separate pre/post |
| `RegisterPreHook` | `RegisterPreHook(path, callback)` | uint64 | Pre-hook only |
| `RegisterPostHook` | `RegisterPostHook(path, callback)` | uint64 | Post-hook only |
| `UnregisterHook` | `UnregisterHook(id)` or `(path, preId, postId)` | void | Remove hook |

**Pre-hook callbacks**: `fn(self, func, parms)` — return `"BLOCK"` to skip original  
**Post-hook callbacks**: `fn(self, func, parms)` — return value ignored

## 8. Lifecycle Event Hooks
| Function | Description |
|---|---|
| `RegisterBeginPlayPreHook(fn)` / `PostHook` | Before/after BeginPlay |
| `RegisterInitGameStatePreHook(fn)` / `PostHook` | Before/after InitGameState |
| `RegisterLoadMapPreHook(fn)` / `PostHook` | Before/after LoadMap |
| `RegisterProcessConsoleExecPreHook(fn)` / `PostHook` | Console command hooks |

## 9. Object Spawn Notification
| Function | Signature | Returns |
|---|---|---|
| `NotifyOnNewObject` | `NotifyOnNewObject(className, callback)` | uint64 |

## 10. Native Hooks (Dobby ARM64)
| Function | Signature | Returns | Description |
|---|---|---|---|
| `RegisterNativeHook` | `RegisterNativeHook(symbol, sig, preFn, postFn)` | bool | Hook native by symbol. Sig: `"ppf>p"` |
| `RegisterNativeHookAt` | `RegisterNativeHookAt(addr, sig, preFn, postFn)` | bool | Hook at address |

Pre can return `"BLOCK"` to skip original.

## 11. Native Calling
| Function | Signature | Returns |
|---|---|---|
| `CallNative` | `CallNative(addr, sig, args...)` | any |
| `CallNativeBySymbol` | `CallNativeBySymbol(name, sig, args...)` | any |

Sig: first char = return type (v=void, i=int32, u=uint32, b=bool, p=ptr, f=float, d=double), rest = arg types.

## 12. Memory Read/Write
**All are null-safe (msync probe).**

| Read | Write | Size |
|---|---|---|
| `ReadU8(addr)` | `WriteU8(addr, val)` | 8-bit |
| `ReadU16(addr)` | `WriteU16(addr, val)` | 16-bit |
| `ReadU32(addr)` | `WriteU32(addr, val)` | 32-bit |
| `ReadU64(addr)` | `WriteU64(addr, val)` | 64-bit |
| `ReadS32(addr)` | `WriteS32(addr, val)` | signed 32 |
| `ReadF32(addr)` / `ReadFloat` | `WriteF32(addr, val)` / `WriteFloat` | float |
| `ReadF64(addr)` | `WriteF64(addr, val)` | double |
| `ReadPtr(addr)` / `ReadPointer` | `WritePtr(addr, val)` | pointer |

## 13. Address Helpers & Symbol Resolution
| Function | Signature | Returns | Description |
|---|---|---|---|
| `GetLibBase` | `GetLibBase()` | lightuserdata | Game library base |
| `FindSymbol` | `FindSymbol(name)` | lightuserdata\|nil | dlsym resolve |
| `Resolve` | `Resolve(name, fallback?)` | lightuserdata\|nil | Full chain: dlsym→phdr→pattern→fallback |
| `FindPattern` | `FindPattern(pattern)` | lightuserdata\|nil | Byte pattern scan |
| `Offset` | `Offset(base, offset)` | lightuserdata | Pointer arithmetic: base + offset |
| `ToHex` | `ToHex(addr)` | string | Hex string |
| `IsNull` | `IsNull(addr)` | bool | NULL check |
| `IsValidPtr` | `IsValidPtr(addr)` | bool | Non-null AND mapped |
| `AllocateMemory` | `AllocateMemory(size)` | lightuserdata | calloc |

## 14. Timers & Delayed Actions
| Function | Signature | Returns | Description |
|---|---|---|---|
| `ExecuteWithDelay` | `(delayMs, callback)` | handle | One-shot after delay |
| `ExecuteAsync` | `(callback)` | handle | Next tick (0ms) |
| `LoopAsync` | `(delayMs, callback)` | handle | Repeat every delayMs |
| `ExecuteInGameThread` | `(callback)` | void | Next ProcessEvent tick |
| `ExecuteInGameThreadWithDelay` | `(delayMs, callback)` | handle | One-shot, game thread |
| `LoopInGameThread` | `(delayMs, callback)` | handle | Repeat, game thread |
| `ExecuteWithDelayFrames` | `(frames, callback)` | handle | One-shot after N frames |
| `LoopAsyncFrames` | `(frames, callback)` | handle | Repeat every N frames |
| `LoopInGameThreadFrames` | `(frames, callback)` | handle | Repeat every N frames, game thread |
| `CancelDelayedAction` | `(handle)` | void | Cancel action |
| `IsDelayedActionValid` | `(handle)` | bool | Check if active |

## 15. RebuildClass API
`RebuildClass(className)` → table with:
- `__name`, `__parent`, `__raw` fields
- `GetInstance(n?)`, `GetAllInstances()`, `InstanceCount()`
- `HasProp(name)`, `HasFunc(name)`
- `Properties()`, `Functions()`
- `HookProp(name, fn)`, `HookFunc(name, fn)`

## 16. TArray Methods (1-indexed)
| Method | Description |
|---|---|
| `arr[i]` | Read element |
| `arr[i] = val` | Write element |
| `#arr` | Length |
| `GetArrayNum()` | Element count |
| `GetArrayMax()` | Max capacity |
| `IsEmpty()` | True if Num == 0 |
| `IsValid()` | True if non-null |
| `ForEach(fn(i, elem))` | Iterate (return true to break) |
| `Empty()` / `Clear()` | Set Num to 0 |
| `Add(value)` | Append (Num < Max). Returns new index or 0 |
| `AddFName(fname)` | Append FName |

## 17. TMap Methods
| Method | Description |
|---|---|
| `#map` | Valid entry count |
| `IsValid()` | Non-null check |
| `ForEach(fn(key, value))` | Iterate (return true to break) |

## 18. FName
**Constructors**: `FName()`, `FName(string)`, `FName(string, findType)`, `FName(index)`  
**Methods**: `ToString()`, `GetComparisonIndex()`, `IsValid()`, `==`, `tostring()`

## 19. FText
**Constructors**: `FText()`, `FText(string)`  
**Methods**: `ToString()`

## 20. FString
**Constructors**: `FString()`, `FString(string)`  
**Methods**: `ToString()`, `Empty()`/`Clear()`, `Len()`, `IsEmpty()`, `Append(s)`, `Find(s)`, `StartsWith(s)`, `EndsWith(s)`, `ToUpper()`, `ToLower()`, `#fs`, `..`, `==`

## 21. RemoteUnrealParam
Wraps hook callback arguments.  
**Methods**: `get()`, `set(val)`, `type()`

## 22. Shared Variables (Cross-mod)
| Function | Signature |
|---|---|
| `SetSharedVariable` | `(name, value)` |
| `GetSharedVariable` | `(name)` → any\|nil |

## 23. ADB Bridge Commands
| Function | Signature |
|---|---|
| `RegisterCommand` | `RegisterCommand(cmdName, fn(args) → string)` |

## 24. Iteration / Reflection
| Function | Signature | Description |
|---|---|---|
| `ForEachUObject` | `(fn(obj, name))` | Iterate ALL live UObjects |
| `GetKnownClasses` | `()` → table | All class names from cache |

## 25. UClassMethods / UFunctionMethods / UEnumMethods
Static utility tables for class/function/enum introspection.

## 26. PAK & SDK
| Function | Description |
|---|---|
| `MountPak(path)` | Mount .pak file |
| `DumpSDK()` | Regenerate SDK dump |
| `DumpSymbols(path?)` | Dump ELF symbols |

## 27. File I/O
| Function | Signature | Description |
|---|---|---|
| `ReadTextFile` | `(path)` → string\|nil | Read file |
| `WriteTextFile` | `(path, content)` → bool | Write file |
| `FileExists` | `(path)` → bool | Check exists |
| `GetModDir` | `()` → string | Mod directory path |
| `GetDataDir` | `()` → string | Data directory path |

## 28. Mod Config (JSON)
| Method | Signature |
|---|---|
| `ModConfig.Load` | `(modName)` → table\|nil |
| `ModConfig.Save` | `(modName, table)` → bool |
| `ModConfig.GetPath` | `(modName)` → string |

## 29. Per-Mod Globals
| Name | Type | Description |
|---|---|---|
| `MOD_NAME` | string | Current mod name |
| `MOD_DIR` | string | Full path to mod dir |
| `SharedAPI` | table | Cross-mod API table |

## 30. Global Constants
- `MODLOADER_VERSION` = `"3.0.0-arm64"`
- `UE4SS_VERSION_MAJOR/MINOR/HOTFIX` = 3/0/0
- `EFindName.FNAME_Find` (0), `.FNAME_Add` (1)
- `PropertyTypes` — IntProperty(0), FloatProperty(2), BoolProperty(4), etc.
- `EObjectFlags` — RF_NoFlags, RF_Public, RF_Standalone, etc.
- `EInternalObjectFlags` — None, Native, PendingKill, etc.
- `Key` — All VK key codes (A-Z, 0-9, F1-F12, etc.)
- `ModifierKey`/`ModKeys` — SHIFT(1), CONTROL(2), ALT(4)

---

## ⚠️ CRITICAL GOTCHAS

### Struct Properties Are Raw Userdata
`IntPoint`, `Vector2D`, `FVector`, `FRotator` etc. returned by `:Get()` are **raw userdata** with NO metatable.  
**You CANNOT** do `struct.X` or `struct.Y`.

### Call() With Table Args for Struct Params DOES NOT WORK
`obj:Call("SetDrawSize", {X=500, Y=2000})` **ZEROES the struct to 0,0** — the modloader's `Call()` implementation cannot serialize Lua tables into UE4 struct parameters via ProcessEvent. This was proven empirically.

### TArray is 1-Indexed
All TArray access in Lua uses 1-based indexing. `arr[1]` is the first element.

### Pre-Hooks Can Block
Return `"BLOCK"` from a pre-hook callback to prevent the original UFunction from executing. Post-hooks still fire even when blocked.

### All Memory R/W Functions Are Null-Safe
msync probe on ARM64 — reads from bad pointers return 0 instead of crashing.
