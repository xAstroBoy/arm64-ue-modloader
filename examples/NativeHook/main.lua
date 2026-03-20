-- examples/NativeHook/main.lua
-- ═══════════════════════════════════════════════════════════════════════
-- Native Hook — ARM64 Dobby hooking example
--
-- This demonstrates:
--   - RegisterNativeHook for hooking stripped C++ functions
--   - Signature strings for argument/return types
--   - CallNative for calling native functions directly
--   - Symbol resolution with FindSymbol / Resolve
--
-- IMPORTANT: Native hooks require knowing the function's symbol name
-- or address. These are game-specific. The patterns below are templates.
-- ═══════════════════════════════════════════════════════════════════════

local TAG = "NativeHook"
local state = { enabled = true }

-- Load config & register menu
local saved = ModConfig.Load(TAG)
if saved and saved.enabled ~= nil then state.enabled = saved.enabled end

if SharedAPI and SharedAPI.DebugMenu then
    SharedAPI.DebugMenu.RegisterToggle(TAG, "Native Hook Demo", state.enabled,
        function(s) state.enabled = s; ModConfig.Save(TAG, state) end)
end

-- ═══════════════════════════════════════════════════════════════════════
-- Signature Strings
-- ═══════════════════════════════════════════════════════════════════════
-- Native hook signatures describe return type + argument types:
--
--   First char  = return type
--   Rest        = argument types (left to right)
--
--   v = void      i = int32       u = uint32
--   b = bool      p = pointer     f = float
--   d = double    l = int64
--
-- Examples:
--   "vp"    → void func(void*)
--   "fp"    → float func(void*)       — returns float
--   "vpf"   → void func(void*, float) — void, takes ptr + float
--   "bpp"   → bool func(void*, void*) — returns bool
--   "pf>p"  → (alternate notation)

-- ═══════════════════════════════════════════════════════════════════════
-- Pattern 1: Hook by symbol name
-- ═══════════════════════════════════════════════════════════════════════
-- Use RegisterNativeHook with the mangled C++ symbol name.
-- Pre-callback can return "BLOCK" to skip the original.

--[[
local ok = RegisterNativeHook(
    "_ZN12SomeClass14SomeFunctionEv",  -- Mangled C++ symbol
    "fp",                                -- float func(void* this)
    -- Pre-hook: called before original
    function(thisPtr)
        if not state.enabled then return end
        Log(TAG .. ": SomeFunction called on " .. ToHex(thisPtr))
        -- return "BLOCK"  -- Uncomment to block original
    end,
    -- Post-hook: called after original, receives return value
    function(thisPtr, retval)
        if not state.enabled then return end
        Log(TAG .. ": SomeFunction returned " .. tostring(retval))
    end
)
if ok then
    Log(TAG .. ": Hooked SomeFunction")
else
    Log(TAG .. ": Failed to hook SomeFunction (symbol not found)")
end
]]

-- ═══════════════════════════════════════════════════════════════════════
-- Pattern 2: Hook by address
-- ═══════════════════════════════════════════════════════════════════════
-- Use RegisterNativeHookAt when you know the function's address.

--[[
local base = GetLibBase()
local addr = Offset(base, 0x123456)  -- Known offset from analysis

local ok = RegisterNativeHookAt(
    addr,
    "vpp",  -- void func(void*, void*)
    function(arg1, arg2)
        if not state.enabled then return end
        return "BLOCK"
    end,
    nil  -- No post-hook
)
]]

-- ═══════════════════════════════════════════════════════════════════════
-- Pattern 3: Call a native function directly
-- ═══════════════════════════════════════════════════════════════════════
-- Use CallNative or CallNativeBySymbol to invoke C++ functions from Lua.

--[[
-- By symbol
local result = CallNativeBySymbol("_ZN12SomeClass7GetNameEv", "pp", somePtr)

-- By address
local base = GetLibBase()
local func_addr = Offset(base, 0x789ABC)
local result = CallNative(func_addr, "ip", somePtr)  -- int func(void*)
Log(TAG .. ": Native call returned " .. tostring(result))
]]

-- ═══════════════════════════════════════════════════════════════════════
-- Pattern 4: Symbol resolution chain
-- ═══════════════════════════════════════════════════════════════════════
-- Resolve() tries multiple methods: dlsym → phdr scan → pattern match

--[[
local sym = Resolve("_ZN12SomeClass14SomeFunctionEv")
if sym and not IsNull(sym) then
    Log(TAG .. ": Found symbol at " .. ToHex(sym))
else
    Log(TAG .. ": Symbol not found, trying pattern scan...")
    local pat = FindPattern("FF 43 00 D1 ?? ?? ?? ?? F3 53 01 A9")
    if pat and not IsNull(pat) then
        Log(TAG .. ": Pattern found at " .. ToHex(pat))
    end
end
]]

-- ═══════════════════════════════════════════════════════════════════════
-- Pattern 5: Memory patching (NOP instructions)
-- ═══════════════════════════════════════════════════════════════════════
-- Direct memory writes for instruction patching (ARM64 NOP = 0x1F2003D5)

--[[
local base = GetLibBase()
local patch_addr = Offset(base, 0xDEAD00)

-- ARM64 NOP instruction (4 bytes, little-endian)
WriteU32(patch_addr, 0xD503201F)
Log(TAG .. ": Patched instruction at " .. ToHex(patch_addr))
]]

Log(TAG .. ": Loaded — all patterns are templates, uncomment to use")
