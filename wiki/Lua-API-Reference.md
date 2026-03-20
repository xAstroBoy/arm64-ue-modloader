# Lua API Reference

This is the complete API reference for Quest UE4 Modloader's Lua scripting environment.

> For the full detailed reference, see [docs/LUA_API.md](../docs/LUA_API.md)

## Table of Contents

1. [Logging](#1-logging)
2. [Object Finding](#2-object-finding)
3. [UObject Methods](#3-uobject-methods)
4. [ProcessEvent Hooks](#4-processevent-hooks)
5. [Native Hooks](#5-native-hooks)
6. [Memory Read/Write](#6-memory-readwrite)
7. [Timers & Delayed Actions](#7-timers--delayed-actions)
8. [Structs (LuaUStruct)](#8-structs)
9. [TArray](#9-tarray)
10. [Enums](#10-enums)
11. [Widget Creation](#11-widget-creation)
12. [Config & File I/O](#12-config--file-io)
13. [Cross-Mod Communication](#13-cross-mod-communication)
14. [Bridge Commands](#14-bridge-commands)

---

## 1. Logging

```lua
Log("Info message")
LogWarn("Warning message")
LogError("Error message")
print("Debug", 42, true, obj)  -- Auto-formats all types
Notify("Title", "Body")       -- In-game notification
```

## 2. Object Finding

```lua
-- Most commonly used:
local obj = FindFirstOf("ClassName")       -- First live instance
local all = FindAllOf("ClassName")          -- All live instances (table)

-- Advanced:
local cls = FindClass("ClassName")          -- UClass object
local obj = FindObject("ShortName")         -- By short name
local obj = StaticFindObject("FullPath")    -- By full path
local obj = LoadAsset("/Game/Path/Asset")   -- Load from PAK
local world = GetWorldContext()             -- Current UWorld
```

## 3. UObject Methods

Every UObject returned by the API has these methods:

```lua
obj:IsValid()          -- Is this object still alive?
obj:GetName()          -- Short name (e.g., "PlayerController_0")
obj:GetFullName()      -- Full path
obj:GetClass()         -- UClass object
obj:GetClassName()     -- Class name string

-- Property access (reflection)
obj:Get("PropName")    -- Read property
obj:Set("PropName", v) -- Write property
obj.PropName           -- Shorthand read
obj.PropName = v       -- Shorthand write

-- Function calls
obj:Call("FuncName", arg1, arg2, ...)
obj:FuncName(arg1, arg2, ...)  -- Shorthand

-- Type checking
UObject_IsA(obj, "ClassName")  -- Inheritance check
```

## 4. ProcessEvent Hooks

```lua
-- Pre-hook: fires BEFORE original. Return "BLOCK" to prevent it.
RegisterPreHook("/Script/Game.Class:Function", function(self, func, parms)
    local obj = self:get()
    return "BLOCK"  -- Optional: skip original
end)

-- Post-hook: fires AFTER original.
RegisterPostHook("/Script/Game.Class:Function", function(self, func, parms)
    local obj = self:get()
    WriteU8(parms, 0)  -- Modify return value
end)

-- Combined (returns IDs for removal)
local preId, postId = RegisterHook("Class:Func", function(self, parms) end)
UnregisterHook("Class:Func", preId, postId)
```

### Hook Paths

Function paths follow UE4's naming:
- Blueprint: `/Game/Blueprints/Path/BP.BP_C:FunctionName`
- Native: `/Script/ModuleName.ClassName:FunctionName`

## 5. Native Hooks

```lua
-- Hook by C++ symbol name
RegisterNativeHook("_ZN9SomeClass4FuncEv", "fp",
    function(thisPtr) end,           -- Pre-hook
    function(thisPtr, retval) end    -- Post-hook
)

-- Hook by address
local base = GetLibBase()
RegisterNativeHookAt(Offset(base, 0x1234), "vpp",
    function(a1, a2) return "BLOCK" end,
    nil
)

-- Call native function
local result = CallNativeBySymbol("_ZSymbol", "ip", ptr)
local result = CallNative(addr, "fp", ptr)
```

**Signature format:** First char = return type, rest = arg types.
`v`=void, `i`=int32, `u`=uint32, `b`=bool, `p`=pointer, `f`=float, `d`=double, `l`=int64

## 6. Memory Read/Write

> ⚠️ Only use for byte/instruction patching. For UObject properties, use `Get`/`Set`.

```lua
ReadU8(addr)  / WriteU8(addr, val)    -- 8-bit
ReadU16(addr) / WriteU16(addr, val)   -- 16-bit
ReadU32(addr) / WriteU32(addr, val)   -- 32-bit
ReadU64(addr) / WriteU64(addr, val)   -- 64-bit
ReadS32(addr) / WriteS32(addr, val)   -- Signed 32-bit
ReadFloat(addr) / WriteFloat(addr, v) -- Float

-- Address helpers
local base = GetLibBase()              -- Game library base
local sym = FindSymbol("name")         -- dlsym resolve
local sym = Resolve("name")            -- Full resolution chain
local pat = FindPattern("FF 43 ?? ??") -- Byte pattern scan
local ptr = Offset(base, 0x1000)       -- Pointer arithmetic
IsNull(ptr)                            -- NULL check
IsValidPtr(ptr)                        -- Mapped check
```

## 7. Timers & Delayed Actions

```lua
-- One-shot timer
local h = ExecuteWithDelay(5000, function() end)

-- Next tick
ExecuteAsync(function() end)

-- Repeating timer
local h = LoopAsync(2000, function() end)

-- Game thread variants (safe for UObject modification)
ExecuteInGameThread(function() end)
ExecuteInGameThreadWithDelay(1000, function() end)
LoopInGameThread(2000, function() end)

-- Frame-based timers
ExecuteWithDelayFrames(60, function() end)
LoopAsyncFrames(1, function() end)  -- Every frame

-- Cancel
CancelDelayedAction(h)
IsDelayedActionValid(h)
```

## 8. Structs

Struct properties (`FVector`, `FRotator`, `IntPoint`, etc.) are returned as LuaUStruct userdata:

```lua
local pos = actor:Get("Position")  -- LuaUStruct

-- Field access
pos.X                              -- Read field
pos.X = 100                        -- Write field (writes to live memory)

-- Set from table
actor:Set("Position", {X=100, Y=200, Z=300})

-- Pass struct to Call()
actor:Call("SetActorLocation", {X=100, Y=200, Z=300})

-- Methods
pos:GetTypeName()     -- "FVector"
pos:GetSize()         -- Struct size in bytes
pos:Clone()           -- Independent copy
pos:CopyFrom({X=1})   -- Copy from table
pos:GetFields()       -- {X="float", Y="float", Z="float"}
tostring(pos)         -- "UStruct(FVector: X=100, Y=200, Z=300)"
```

## 9. TArray

TArray properties are returned as userdata with **1-based indexing**:

```lua
local arr = obj:Get("SomeArray")

arr[1]                -- First element (1-indexed!)
arr[3] = newValue     -- Write element
#arr                  -- Length
arr:GetArrayNum()     -- Element count
arr:IsEmpty()         -- True if empty
arr:ForEach(function(i, elem)
    Log(i .. ": " .. tostring(elem))
    return false  -- Return true to break
end)
arr:Add(value)        -- Append element
arr:Clear()           -- Remove all
```

## 10. Enums

```lua
-- Global enum table (auto-populated from reflection)
Enums.ECollisionChannel          -- {ECC_WorldStatic=0, ...}
Enums.DebugMenuType              -- {NewEnumerator5=0, ...}

-- Lookup
local ue = FindEnum("EnumName")
local t = GetEnumTable("EnumName")    -- {Name=Value, ...}
local names = GetEnumNames()           -- All enum names

-- Extend (runtime, adds to UEnum metadata)
AppendEnumValue("EnumName", "NewValue", 99)
```

## 11. Widget Creation

```lua
local widget = CreateWidget("TextBlock")
local vbox = CreateWidget("VerticalBox")
local custom = CreateWidget("MyWidget_C")

-- With owning player
local pc = FindFirstOf("PlayerController")
local w = CreateWidget("TextBlock", pc)
```

## 12. Config & File I/O

```lua
-- ModConfig (JSON, persistent)
ModConfig.Save("ModName", {key = "value", num = 42})
local cfg = ModConfig.Load("ModName")
local path = ModConfig.GetPath("ModName")

-- Raw file I/O
local text = ReadTextFile("/path/to/file")
WriteTextFile("/path/to/file", "content")
FileExists("/path/to/file")

-- Mod paths
GetModDir()   -- This mod's directory
GetDataDir()  -- Shared data directory
```

## 13. Cross-Mod Communication

```lua
-- SharedAPI table (shared across all mods)
SharedAPI.MyMod = { DoSomething = function() end }

-- Shared variables
SetSharedVariable("key", value)
local v = GetSharedVariable("key")
```

## 14. Bridge Commands

```lua
RegisterBridgeCommand("my_command", function(args)
    return { status = "ok", data = 42 }
end)
```

Test: `python tools/deploy.py console` → `my_command`
