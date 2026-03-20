# Troubleshooting

Common issues and solutions for Quest UE4 Modloader.

## Getting Logs

Always start by collecting logs:

```bash
# Modloader log
python tools/deploy.py log

# Crash dumps (tombstones)
python tools/deploy.py tombstones

# Android logcat (verbose)
adb logcat | grep -i "modloader\|UEMod\|lua"
```

## Common Issues

### Modloader doesn't load / no log file

**Symptoms:** No `UEModLoader.log` on device, game runs normally without mods.

**Causes & Solutions:**
1. **Library not injected** — Verify `libmodloader.so` is in the correct path
2. **Wrong architecture** — Must be ARM64 (`file libmodloader.so` should show `aarch64`)
3. **Permissions** — `chmod 755 /path/to/libmodloader.so`
4. **Root required** — Modloader injection requires root access on Quest

### Game crashes on launch

**Symptoms:** Game immediately closes or shows a black screen.

**Solutions:**
1. Pull tombstones: `python tools/deploy.py tombstones`
2. Check for symbol resolution failures in the log
3. Try removing all mods and testing with just the modloader
4. Verify NDK version matches (r23c / 23.1.7779620)

### Mod not loading

**Symptoms:** Log shows modloader init but mod isn't listed.

**Check:**
1. Folder structure: `mods/ModName/main.lua` (case-sensitive!)
2. Lua syntax errors: check the log for `[LuaError]` messages
3. File encoding: must be UTF-8 (no BOM)
4. Mod name conflicts: each folder must have a unique name

### Hook not firing

**Symptoms:** `RegisterPreHook`/`RegisterPostHook` succeeds but callback never runs.

**Check:**
1. **Path is correct** — Use full UFunction path: `/Script/Module.Class:Function`
   - Blueprint: `/Game/Blueprints/Path/BP.BP_C:FunctionName`
   - Native: `/Script/ModuleName.ClassName:FunctionName`
2. **Function actually called** — Not all functions run during gameplay
3. **Typos** — Path is case-sensitive
4. **PE trace** — Enable ProcessEvent tracing to see what functions fire:
   ```bash
   python tools/deploy.py console
   > exec_lua SetPETraceEnabled(true)
   ```

### pcall returns error but no useful message

**Symptoms:** `pcall(function() obj:Get("Prop") end)` returns `false` with a generic error.

**Solutions:**
1. Check `obj:IsValid()` before the pcall
2. Verify property name exists in the SDK dump
3. Try `obj:GetClassName()` to confirm the object type
4. Use the bridge to test interactively:
   ```
   exec_lua local o = FindFirstOf("ClassName"); return o:Get("PropName")
   ```

### "Object not valid" errors

**Symptoms:** Objects that were valid become invalid.

**Causes:**
- UObject was garbage collected
- Level was unloaded
- Object was destroyed by game logic

**Solution:** Always re-find objects or check `IsValid()`:
```lua
-- Don't cache forever
local function get_player()
    local p = FindFirstOf("PlayerController")
    return (p and p:IsValid()) and p or nil
end
```

### Bridge connection refused

**Symptoms:** `python tools/deploy.py console` fails to connect.

**Solutions:**
1. Set up port forwarding: `python tools/deploy.py forward`
2. Or manually: `adb forward tcp:19420 tcp:19420`
3. Verify game is running with modloader loaded
4. Check if bridge is enabled in modloader config

### Build fails

**Symptoms:** `build.bat` or `build.sh` fails.

**Check:**
1. NDK path is correct and NDK r23c is installed
2. CMake 3.22+ is available
3. Ninja is installed
4. Submodules are cloned: `git submodule update --init --recursive`
5. On Windows, ensure NDK path has no spaces

### Native hook crash

**Symptoms:** Game crashes after `RegisterNativeHook`.

**Causes:**
- Wrong signature string (arg count mismatch)
- Symbol doesn't exist (stripped)
- Function is too short for Dobby to hook (< 16 bytes)

**Solutions:**
1. Verify symbol exists: `exec_lua return ToHex(FindSymbol("_ZSymbol"))`
2. Double-check signature matches the actual C++ signature
3. Use pattern scan as fallback: `FindPattern("FF 43 00 D1 ...")`
4. The modloader has crash guards — check the log for `[CrashHandler]`

## Performance Issues

### Game stutters with many mods

- Reduce `LoopAsync` frequency (use longer intervals)
- Avoid heavy operations in hooks that fire every frame
- Use `LoopInGameThread` instead of `LoopAsync` for game modifications
- Profile with PE trace to find hotspots

### Large number of FindFirstOf calls

- Cache results and re-find only when needed
- Use `NotifyOnNewObject` instead of polling

## Getting Help

1. **Search existing issues** on [GitHub Issues](https://github.com/xAstroBoy/quest-ue4-modloader/issues)
2. **Open a bug report** using the [template](https://github.com/xAstroBoy/quest-ue4-modloader/issues/new?template=bug_report.yml)
3. **Include logs** — always attach `UEModLoader.log` and any tombstones
4. **Minimal reproduction** — try to isolate which mod/hook causes the issue
