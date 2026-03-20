// modloader/src/hook/native_hooks.cpp
// Dobby-based native function hooks for non-UFunction C++ functions
// Each hook gets a trampoline that fires pre/post callbacks with
// full ARM64 register state access — both X0-X7 AND D0-D7.
//
// ARM64 AAPCS64 calling convention:
//   X0-X7:  integer/pointer arguments
//   D0-D7:  float/double arguments (independent register file)
//   X0:     integer/pointer return value
//   D0:     float/double return value
//
// The thunks use __attribute__((naked)) inline assembly to capture
// both register banks into a NativeCallContext before dispatching
// to C++ callbacks. This is essential for hooking functions like
// UpdateRecoil(self*, float dt) where dt lives in D0, not X1.

#include "modloader/native_hooks.h"
#include "modloader/symbols.h"
#include "modloader/logger.h"
#include "modloader/lua_ue4ss_globals.h"
#include <dobby.h>
#include <unordered_map>
#include <mutex>
#include <atomic>
#include <cstring>
#include <setjmp.h>

// ═══════════════════════════════════════════════════════════════════════
// SAFE-CALL CRASH RECOVERY
// ═══════════════════════════════════════════════════════════════════════
// When a hooked original function crashes (SIGSEGV/SIGBUS), the crash
// handler detects this thread-local flag and uses siglongjmp to recover
// back to dispatch_full() instead of killing the process.
// dispatch_full() then returns safe defaults (0 / 0.0) to the caller.
//
// This protects the game from crashes in native functions called through
// modloader hooks — e.g. UpdateViewTargetInternal dereferencing a
// dangling FTViewTarget::Target pointer at boot (tombstone_04/05/08/11).
thread_local volatile int g_in_hook_original_call = 0;
thread_local sigjmp_buf g_hook_recovery_jmp;

// ═══════════════════════════════════════════════════════════════════════
// HOOK INSTALLATION CRASH RECOVERY
// ═══════════════════════════════════════════════════════════════════════
// When DobbyHook() tries to install a trampoline at a bad address
// (unmapped memory, stripped function too small, etc.), it can SIGSEGV.
// This flag tells the crash handler to siglongjmp back to install_at()
// instead of killing the process. install_at() then returns 0 (failure)
// and the mod continues loading — no crash, no mods killed.
thread_local volatile int g_in_hook_install = 0;
thread_local sigjmp_buf g_hook_install_jmp;

namespace native_hooks {

static std::atomic<HookId> s_next_id{1};

struct HookRecord {
    HookId          id;
    std::string     name;
    void*           address;
    void*           original;      // trampoline to original function
    NativePreCallback  pre;
    NativePostCallback post;
    std::atomic<uint64_t> call_count;
};

static std::unordered_map<HookId, HookRecord*> s_hooks;
static std::unordered_map<void*, HookRecord*> s_addr_to_hook;
static std::mutex s_mutex;

// ═══════════════════════════════════════════════════════════════════════
// ARM64 FULL-REGISTER CAPTURE THUNK SYSTEM
// ═══════════════════════════════════════════════════════════════════════
//
// Problem: C function pointers can only capture X0-X7 (integer regs).
//          Float args in D0-D7 are invisible to C-typed thunks.
//
// Solution: Each thunk is a naked assembly function that:
//   1. Saves D0-D7 to a per-slot save area (thread-local)
//   2. Passes the slot index in X9 (caller-saved scratch reg)
//   3. Falls through to a single shared assembly dispatcher that
//      calls the C++ dispatch_full() with a pointer to the save area
//   4. On return, restores D0 from the save area (float return value)
//
// The save area layout per slot:
//   [0..63]   = d[0..7]   (8 doubles = 64 bytes)
//
// We use thread-local storage so hooks are safe across threads.

constexpr int MAX_HOOKS = 256;
constexpr int MAX_THUNK_SLOTS = 64;
static HookRecord* s_thunk_table[MAX_HOOKS] = {};
static int s_thunk_count = 0;

// Per-thread float register save area — one set of 8 doubles per slot
// We only need one active at a time per thread since hooks don't recurse
// into the same slot. We use a single flat area and the slot index.
struct FloatSaveArea {
    double d[8]; // D0-D7 at entry
};
static thread_local FloatSaveArea s_float_save[MAX_THUNK_SLOTS];

// ── C++ dispatch function called from assembly thunks ───────────────────
// This receives the slot index and ALL 8 X-registers.
// D0-D7 have already been saved to s_float_save[slot] by the asm thunk.
// Returns: X0 return value in the uint64_t return.
//          D0 return value is written to s_float_save[slot].d[0].
extern "C" uint64_t dispatch_full(int slot,
                                   uint64_t x0, uint64_t x1, uint64_t x2, uint64_t x3,
                                   uint64_t x4, uint64_t x5, uint64_t x6, uint64_t x7) {
    HookRecord* rec = s_thunk_table[slot];
    if (!rec) return 0;

    rec->call_count.fetch_add(1, std::memory_order_relaxed);

    // ── THREAD SAFETY GUARD ─────────────────────────────────────────
    // Lua VM is single-threaded. If this hook is called from a non-game
    // thread (rendering thread, task graph worker, etc.), we MUST NOT
    // invoke Lua pre/post callbacks — doing so corrupts the Lua state
    // and causes SIGSEGV in lua_rawseti/luaL_unref.
    // Crash guards (nullptr callbacks) are unaffected — they only use
    // the sigsetjmp safe-call guard on the original function.
    bool on_game_thread = lua_ue4ss_globals::is_game_thread();
    bool has_lua_callbacks = (rec->pre || rec->post);

    if (!on_game_thread && has_lua_callbacks) {
        // Off-thread call with Lua callbacks — skip callbacks, call original
        // with sigsetjmp guard only (still protects against crashes).
        FloatSaveArea& fsa = s_float_save[slot];
        for (int i = 0; i < 8; i++) fsa.d[i] = fsa.d[i]; // preserve floats

        g_in_hook_original_call = 1;
        int crash_sig = sigsetjmp(g_hook_recovery_jmp, 1);

        if (crash_sig != 0) {
            g_in_hook_original_call = 0;
            // Silent recovery — don't log every frame (rendering thread is high-frequency)
            return 0;
        }

        void* orig = rec->original;
        uint64_t ret_x0 = 0;
        double ret_d0 = 0.0;
        uint64_t x_args[8] = {x0, x1, x2, x3, x4, x5, x6, x7};

        __asm__ volatile (
            "ldp d0, d1, [%[fsa], #0]   \n"
            "ldp d2, d3, [%[fsa], #16]  \n"
            "ldp d4, d5, [%[fsa], #32]  \n"
            "ldp d6, d7, [%[fsa], #48]  \n"
            "ldp x0, x1, [%[xa], #0]    \n"
            "ldp x2, x3, [%[xa], #16]   \n"
            "ldp x4, x5, [%[xa], #32]   \n"
            "ldp x6, x7, [%[xa], #48]   \n"
            "blr %[fn]                  \n"
            "mov %[rx], x0              \n"
            "str d0, [%[rd_addr]]        \n"
            : [rx] "=r" (ret_x0)
            : [fsa]     "r" (&fsa),
              [xa]      "r" (x_args),
              [fn]      "r" (orig),
              [rd_addr] "r" (&ret_d0)
            : "x0", "x1", "x2", "x3", "x4", "x5", "x6", "x7",
              "x8", "x9", "x10", "x11", "x12", "x13", "x14", "x15",
              "x16", "x17", "x30",
              "d0", "d1", "d2", "d3", "d4", "d5", "d6", "d7",
              "d16", "d17", "d18", "d19", "d20", "d21", "d22", "d23",
              "d24", "d25", "d26", "d27", "d28", "d29", "d30", "d31",
              "cc", "memory"
        );

        g_in_hook_original_call = 0;
        fsa.d[0] = ret_d0;
        return ret_x0;
    }

    NativeCallContext ctx;
    memset(&ctx, 0, sizeof(ctx));
    ctx.x[0] = x0; ctx.x[1] = x1; ctx.x[2] = x2; ctx.x[3] = x3;
    ctx.x[4] = x4; ctx.x[5] = x5; ctx.x[6] = x6; ctx.x[7] = x7;

    // Read float registers from the save area (written by asm thunk)
    FloatSaveArea& fsa = s_float_save[slot];
    for (int i = 0; i < 8; i++) ctx.d[i] = fsa.d[i];

    ctx.original = rec->original;
    ctx.blocked = false;
    ctx.ret_override = false;
    ctx.ret_x0 = 0;
    ctx.ret_d0 = 0.0;

    // Pre callback — may modify ctx.x[], ctx.d[], or set ctx.blocked
    if (rec->pre) {
        rec->pre(ctx);
    }

    // Call original (unless blocked or return overridden)
    // Wrapped in safe-call guard: if original crashes, recover with defaults.
    if (!ctx.blocked && !ctx.ret_override) {
        // Write potentially-modified float args back to save area
        for (int i = 0; i < 8; i++) fsa.d[i] = ctx.d[i];

        void* orig = rec->original;

        // ── SAFE-CALL GUARD ──────────────────────────────────────────
        // sigsetjmp saves full register state. If the original function
        // crashes (SIGSEGV/SIGBUS on dangling pointer, null vtable, etc.),
        // the crash_handler detects g_in_hook_original_call and calls
        // siglongjmp to return here with the signal number.
        // This prevents ANY hooked native function from crashing the game.
        g_in_hook_original_call = 1;
        int crash_sig = sigsetjmp(g_hook_recovery_jmp, 1);

        if (crash_sig != 0) {
            // ── CRASH RECOVERED ──────────────────────────────────────
            g_in_hook_original_call = 0;
            logger::log_error("NHOOK",
                "SAFE-CALL RECOVERY: Hook '%s' original function crashed "
                "(signal %d) — returning safe defaults (0/0.0). "
                "Game continues running.",
                rec->name.c_str(), crash_sig);
            ctx.ret_x0 = 0;
            ctx.ret_d0 = 0.0;
        } else {
            // ── NORMAL PATH — call original with full register restore ──
            uint64_t ret_x0 = 0;
            double ret_d0 = 0.0;
            uint64_t* x_args = ctx.x;

            __asm__ volatile (
                // Load D0-D7 from float save area
                "ldp d0, d1, [%[fsa], #0]   \n"
                "ldp d2, d3, [%[fsa], #16]  \n"
                "ldp d4, d5, [%[fsa], #32]  \n"
                "ldp d6, d7, [%[fsa], #48]  \n"
                // Load X0-X7 from integer args array
                "ldp x0, x1, [%[xa], #0]    \n"
                "ldp x2, x3, [%[xa], #16]   \n"
                "ldp x4, x5, [%[xa], #32]   \n"
                "ldp x6, x7, [%[xa], #48]   \n"
                // Call original through the function pointer
                "blr %[fn]                  \n"
                // Capture return values — X0 and D0
                "mov %[rx], x0              \n"
                "str d0, [%[rd_addr]]        \n"
                : [rx] "=r" (ret_x0)
                : [fsa]     "r" (&fsa),
                  [xa]      "r" (x_args),
                  [fn]      "r" (orig),
                  [rd_addr] "r" (&ret_d0)
                : "x0", "x1", "x2", "x3", "x4", "x5", "x6", "x7",
                  "x8", "x9", "x10", "x11", "x12", "x13", "x14", "x15",
                  "x16", "x17", "x30",
                  "d0", "d1", "d2", "d3", "d4", "d5", "d6", "d7",
                  "d16", "d17", "d18", "d19", "d20", "d21", "d22", "d23",
                  "d24", "d25", "d26", "d27", "d28", "d29", "d30", "d31",
                  "cc", "memory"
            );

            g_in_hook_original_call = 0;
            ctx.ret_x0 = ret_x0;
            ctx.ret_d0 = ret_d0;
        }
    }

    // Post callback — may modify ctx.ret_x0 / ctx.ret_d0
    if (rec->post) {
        rec->post(ctx);
    }

    // Write D0 return value back to save area — asm thunk will restore D0 from here
    fsa.d[0] = ctx.ret_d0;

    // Return the X0 value — asm thunk passes this through in X0
    return ctx.ret_x0;
}

// ── Assembly thunks ─────────────────────────────────────────────────────
// Each thunk is a naked function that:
//   1. Saves D0-D7 to the thread-local save area for its slot
//   2. Saves X0-X7 to stack (preserved across the setup code)
//   3. Calls dispatch_full(slot, x0, x1, ..., x7)
//   4. Restores D0 from save area (float return value)
//   5. Returns with both X0 and D0 set correctly
//
// We use a macro to generate 64 thunks. Each thunk loads its slot index
// into a register and branches to a shared body.
//
// However, __attribute__((naked)) on AArch64 with Clang/GCC is limited.
// Instead we use a simpler approach: save D0-D7 to thread-local BEFORE
// the C++ dispatch, using a small wrapper that's NOT naked.
//
// Strategy: Each thunk is a regular C++ function that:
//   - Uses inline asm to read D0-D7 into local variables
//   - Stores them into the thread-local save area
//   - Calls dispatch_full
//   - Uses inline asm to write D0 return from save area
//   - Returns X0 value
//
// The key insight: at thunk entry, the compiler hasn't clobbered D0-D7 yet
// because they're callee-saved if we read them FIRST thing. Actually D0-D7
// are caller-saved (volatile) in AAPCS64, but at function entry they hold
// the caller's arguments. If we read them before any function call or
// complex expression, they're still valid.
//
// We use __attribute__((noinline)) to prevent the compiler from
// reordering the inline asm that reads D registers.

#define DEFINE_THUNK(N) \
    __attribute__((noinline)) \
    static uint64_t thunk_##N(uint64_t a0, uint64_t a1, uint64_t a2, uint64_t a3, \
                              uint64_t a4, uint64_t a5, uint64_t a6, uint64_t a7) { \
        /* Capture D0-D7 IMMEDIATELY — before any other code executes.       */ \
        /* At function entry, D0-D7 still hold the caller's float arguments. */ \
        /* The compiler won't have clobbered them yet because this is the    */ \
        /* very first statement and we use noinline + volatile asm.          */ \
        FloatSaveArea& fsa = s_float_save[N]; \
        __asm__ volatile ( \
            "stp d0, d1, [%0, #0]  \n" \
            "stp d2, d3, [%0, #16] \n" \
            "stp d4, d5, [%0, #32] \n" \
            "stp d6, d7, [%0, #48] \n" \
            : : "r" (&fsa) \
            : "memory" \
        ); \
        /* Dispatch — handles pre/post callbacks and original call */ \
        uint64_t ret = dispatch_full(N, a0, a1, a2, a3, a4, a5, a6, a7); \
        /* Restore D0 from save area (float return value) */ \
        __asm__ volatile ( \
            "ldr d0, [%0, #0] \n" \
            : : "r" (&fsa) \
            : "d0" \
        ); \
        return ret; \
    }

// Generate 64 thunks
DEFINE_THUNK(0)   DEFINE_THUNK(1)   DEFINE_THUNK(2)   DEFINE_THUNK(3)
DEFINE_THUNK(4)   DEFINE_THUNK(5)   DEFINE_THUNK(6)   DEFINE_THUNK(7)
DEFINE_THUNK(8)   DEFINE_THUNK(9)   DEFINE_THUNK(10)  DEFINE_THUNK(11)
DEFINE_THUNK(12)  DEFINE_THUNK(13)  DEFINE_THUNK(14)  DEFINE_THUNK(15)
DEFINE_THUNK(16)  DEFINE_THUNK(17)  DEFINE_THUNK(18)  DEFINE_THUNK(19)
DEFINE_THUNK(20)  DEFINE_THUNK(21)  DEFINE_THUNK(22)  DEFINE_THUNK(23)
DEFINE_THUNK(24)  DEFINE_THUNK(25)  DEFINE_THUNK(26)  DEFINE_THUNK(27)
DEFINE_THUNK(28)  DEFINE_THUNK(29)  DEFINE_THUNK(30)  DEFINE_THUNK(31)
DEFINE_THUNK(32)  DEFINE_THUNK(33)  DEFINE_THUNK(34)  DEFINE_THUNK(35)
DEFINE_THUNK(36)  DEFINE_THUNK(37)  DEFINE_THUNK(38)  DEFINE_THUNK(39)
DEFINE_THUNK(40)  DEFINE_THUNK(41)  DEFINE_THUNK(42)  DEFINE_THUNK(43)
DEFINE_THUNK(44)  DEFINE_THUNK(45)  DEFINE_THUNK(46)  DEFINE_THUNK(47)
DEFINE_THUNK(48)  DEFINE_THUNK(49)  DEFINE_THUNK(50)  DEFINE_THUNK(51)
DEFINE_THUNK(52)  DEFINE_THUNK(53)  DEFINE_THUNK(54)  DEFINE_THUNK(55)
DEFINE_THUNK(56)  DEFINE_THUNK(57)  DEFINE_THUNK(58)  DEFINE_THUNK(59)
DEFINE_THUNK(60)  DEFINE_THUNK(61)  DEFINE_THUNK(62)  DEFINE_THUNK(63)

using ThunkFn = uint64_t(*)(uint64_t, uint64_t, uint64_t, uint64_t,
                           uint64_t, uint64_t, uint64_t, uint64_t);

static ThunkFn s_thunk_fns[64] = {
    thunk_0,  thunk_1,  thunk_2,  thunk_3,  thunk_4,  thunk_5,  thunk_6,  thunk_7,
    thunk_8,  thunk_9,  thunk_10, thunk_11, thunk_12, thunk_13, thunk_14, thunk_15,
    thunk_16, thunk_17, thunk_18, thunk_19, thunk_20, thunk_21, thunk_22, thunk_23,
    thunk_24, thunk_25, thunk_26, thunk_27, thunk_28, thunk_29, thunk_30, thunk_31,
    thunk_32, thunk_33, thunk_34, thunk_35, thunk_36, thunk_37, thunk_38, thunk_39,
    thunk_40, thunk_41, thunk_42, thunk_43, thunk_44, thunk_45, thunk_46, thunk_47,
    thunk_48, thunk_49, thunk_50, thunk_51, thunk_52, thunk_53, thunk_54, thunk_55,
    thunk_56, thunk_57, thunk_58, thunk_59, thunk_60, thunk_61, thunk_62, thunk_63,
};

void init() {
    logger::log_info("NHOOK", "Native hook subsystem initialized (max %d hooks)", MAX_HOOKS);
}

HookId install(const std::string& symbol, NativePreCallback pre, NativePostCallback post) {
    void* addr = symbols::resolve(symbol);
    if (!addr) {
        logger::log_error("NHOOK", "Cannot hook '%s' — symbol not found", symbol.c_str());
        return 0;
    }
    return install_at(addr, symbol, pre, post);
}

HookId install_at(void* addr, const std::string& name,
                  NativePreCallback pre, NativePostCallback post) {
    std::lock_guard<std::mutex> lock(s_mutex);

    // Check if this address is already hooked — Dobby cannot hook the same address twice.
    // This happens when two different search names resolve to the same overload
    // (e.g. cItemMgr_bulletNum and cItemMgr_bulletNum_cItem both resolve to cItemMgr::bulletNum()).
    auto existing = s_addr_to_hook.find(addr);
    if (existing != s_addr_to_hook.end()) {
        logger::log_warn("NHOOK", "Address 0x%lX already hooked as '%s' — skipping duplicate hook '%s'",
                         reinterpret_cast<uintptr_t>(addr),
                         existing->second->name.c_str(), name.c_str());
        return existing->second->id;
    }

    if (s_thunk_count >= 64) {
        logger::log_error("NHOOK", "Hook limit reached (64) — cannot hook '%s'", name.c_str());
        return 0;
    }

    int slot = s_thunk_count++;
    auto* rec = new HookRecord();
    rec->id = s_next_id.fetch_add(1);
    rec->name = name;
    rec->address = addr;
    rec->original = nullptr;
    rec->pre = pre;
    rec->post = post;
    rec->call_count.store(0);

    s_thunk_table[slot] = rec;

    // ── HOOK INSTALL CRASH GUARD ─────────────────────────────────────
    // DobbyHook() reads instructions at 'addr' to build a trampoline,
    // then writes a branch to our thunk. If 'addr' is unmapped, the
    // function is too small for a trampoline, or the memory page can't
    // be made writable, Dobby will SIGSEGV/SIGBUS.
    // We use sigsetjmp to catch those crashes and recover gracefully —
    // install_at() returns 0 (failure) and the mod keeps loading.
    g_in_hook_install = 1;
    int install_crash_sig = sigsetjmp(g_hook_install_jmp, 1);

    if (install_crash_sig != 0) {
        // ── CRASH RECOVERED DURING HOOK INSTALLATION ─────────────────
        g_in_hook_install = 0;
        logger::log_error("NHOOK",
            "CRASH DURING HOOK INSTALL: '%s' at 0x%lX — "
            "DobbyHook() crashed with signal %d. "
            "Address is likely unmapped, function too small for trampoline, "
            "or memory protection fault. Hook skipped — mod continues loading.",
            name.c_str(), reinterpret_cast<uintptr_t>(addr), install_crash_sig);
        delete rec;
        s_thunk_table[slot] = nullptr;
        s_thunk_count--;
        return 0;
    }

    int status = DobbyHook(addr,
                           reinterpret_cast<dobby_dummy_func_t>(s_thunk_fns[slot]),
                           reinterpret_cast<dobby_dummy_func_t*>(&rec->original));

    g_in_hook_install = 0;

    if (status == 0) {
        logger::log_info("NHOOK", "Hook '%s' installed at 0x%lX (slot %d, id %lu)",
                         name.c_str(), reinterpret_cast<uintptr_t>(addr),
                         slot, (unsigned long)rec->id);
        s_hooks[rec->id] = rec;
        s_addr_to_hook[addr] = rec;
        return rec->id;
    } else {
        logger::log_error("NHOOK", "Dobby failed to hook '%s' at 0x%lX (status=%d)",
                          name.c_str(), reinterpret_cast<uintptr_t>(addr), status);
        delete rec;
        s_thunk_table[slot] = nullptr;
        s_thunk_count--;
        return 0;
    }
}

void remove(HookId id) {
    std::lock_guard<std::mutex> lock(s_mutex);
    auto it = s_hooks.find(id);
    if (it == s_hooks.end()) return;

    HookRecord* rec = it->second;
    DobbyDestroy(rec->address);
    s_addr_to_hook.erase(rec->address);
    s_hooks.erase(it);

    // Find and clear thunk slot
    for (int i = 0; i < MAX_HOOKS; i++) {
        if (s_thunk_table[i] == rec) {
            s_thunk_table[i] = nullptr;
            break;
        }
    }

    logger::log_info("NHOOK", "Hook '%s' removed (id %lu)", rec->name.c_str(), (unsigned long)id);
    delete rec;
}

bool is_hooked(const std::string& symbol) {
    std::lock_guard<std::mutex> lock(s_mutex);
    for (const auto& pair : s_hooks) {
        if (pair.second->name == symbol) return true;
    }
    return false;
}

std::vector<HookInfo> get_all_hooks() {
    std::lock_guard<std::mutex> lock(s_mutex);
    std::vector<HookInfo> result;
    for (const auto& pair : s_hooks) {
        HookInfo hi;
        hi.id = pair.second->id;
        hi.name = pair.second->name;
        hi.address = pair.second->address;
        hi.call_count = pair.second->call_count.load(std::memory_order_relaxed);
        hi.has_pre = (bool)pair.second->pre;
        hi.has_post = (bool)pair.second->post;
        result.push_back(hi);
    }
    return result;
}

// ═══════════════════════════════════════════════════════════════════════
// BUILT-IN CRASH GUARDS
// ═══════════════════════════════════════════════════════════════════════
// These hooks have NO Lua callbacks. They exist solely to route
// known-crashy game functions through dispatch_full()'s sigsetjmp
// safe-call guard. If the original function crashes (SIGSEGV on
// dangling pointer, null vtable deref, etc.), the guard recovers
// and returns safe defaults (0/0.0) instead of killing the game.
//
// This is a C++ modloader built-in — no Lua patches required.
// ═══════════════════════════════════════════════════════════════════════

// Table of known-crashy functions to guard.
// Each entry: { mangled_symbol, fallback_offset, friendly_name }
struct CrashGuardEntry {
    const char* symbol;
    uintptr_t   fallback_offset;  // offset from libUE4 base
    const char* name;
};

static const CrashGuardEntry s_crash_guards[] = {
    // UGameEngine::RedrawViewports(bool)
    // Top-level render entry point called once per tick.
    // Hooking HERE catches ALL downstream viewport crashes:
    //   RedrawViewports → FViewport::Draw → UGameViewportClient::Draw
    //     → CalcSceneView → CalcSceneViewInitOptions → GetProjectionData
    //     → UpdateViewTargetInternal → CalcCamera
    // All of these crash at boot (9s) with dangling pointers.
    // If it crashes, safe-call guard skips this frame. Next tick retries.
    // Tombstones: 04, 05, 08, 11, 12, 13, 14
    {
        "_ZN12UGameEngine16RedrawViewportsEb",
        0x088DA0AC,
        "UGameEngine::RedrawViewports"
    },

    // APlayerCameraManager::UpdateViewTargetInternal(FTViewTarget&, float)
    // Crashes at boot (9s) when FTViewTarget::Target is null/dangling.
    // The engine calls CalcCamera on Target before it's valid.
    // Tombstones: 04, 05, 08, 11, 12, 13
    {
        "_ZN20APlayerCameraManager24UpdateViewTargetInternalER12FTViewTargetf",
        0x08A98A9C,
        "UpdateViewTargetInternal"
    },

    // ULocalPlayer::GetProjectionData(FViewport*, EStereoscopicPass,
    //                                  FSceneViewProjectionData&) const
    // Crashes at boot (9s) — vtable call on dangling view target pointer.
    // Tombstone: 14 — fault addr 0x00c1b68800000074 (corrupted vtable)
    {
        "_ZNK12ULocalPlayer17GetProjectionDataEP9FViewport17EStereoscopicPassR24FSceneViewProjectionData",
        0x08A1195C,
        "ULocalPlayer::GetProjectionData"
    },

    // APlayerController::UpdateRotation(float)
    // Crashes at boot (9s) via tick system on OVRA Update thread:
    //   UWorld::Tick → TickTaskManager → APlayerController::TickActor
    //     → UpdateRotation → vtable call on dangling pointer
    // This is a SEPARATE call chain from RedrawViewports — tick system
    // dispatches actor ticks in parallel via task graph workers.
    // Tombstone: 16 — fault addr 0x0044b68800000074 (corrupted vtable)
    {
        "_ZN18APlayerController14UpdateRotationEf",
        0x08A9F8B8,
        "APlayerController::UpdateRotation"
    },

    // APlayerController::TickActor(float, ELevelTick, FActorTickFunction&)
    // Parent of UpdateRotation in the tick pipeline.
    // Guards ALL APlayerController tick operations at once.
    // Tombstone: 16 — APlayerController::TickActor at offset +824
    {
        "_ZN18APlayerController9TickActorEf10ELevelTickR18FActorTickFunction",
        0x08AAA6C0,
        "APlayerController::TickActor"
    },
};

void install_builtin_crash_guards() {
    logger::log_info("NHOOK",
        "Installing %zu built-in crash guard(s)...",
        sizeof(s_crash_guards) / sizeof(s_crash_guards[0]));

    uintptr_t lib_base = symbols::lib_base();

    for (const auto& entry : s_crash_guards) {
        // Try dlsym first
        void* addr = symbols::resolve(entry.symbol);

        // Fallback to base + offset
        if (!addr && lib_base != 0 && entry.fallback_offset != 0) {
            addr = reinterpret_cast<void*>(lib_base + entry.fallback_offset);
            logger::log_warn("NHOOK",
                "Crash guard '%s': dlsym failed, using fallback offset 0x%lX → 0x%lX",
                entry.name, (unsigned long)entry.fallback_offset,
                (unsigned long)reinterpret_cast<uintptr_t>(addr));
        }

        if (!addr) {
            logger::log_error("NHOOK",
                "Crash guard '%s': FAILED — could not resolve address", entry.name);
            continue;
        }

        // Install hook with NO callbacks — purely for safe-call protection.
        // dispatch_full() will call the original inside the sigsetjmp guard.
        // If it crashes, siglongjmp recovers and returns 0/0.0.
        HookId id = install_at(addr, std::string("__guard_") + entry.name,
                               nullptr, nullptr);

        if (id != 0) {
            logger::log_info("NHOOK",
                "Crash guard '%s' installed at 0x%lX (hook id %lu) "
                "— sigsetjmp safe-call active",
                entry.name, (unsigned long)reinterpret_cast<uintptr_t>(addr),
                (unsigned long)id);
        } else {
            logger::log_error("NHOOK",
                "Crash guard '%s': Dobby hook FAILED at 0x%lX",
                entry.name, (unsigned long)reinterpret_cast<uintptr_t>(addr));
        }
    }

    logger::log_info("NHOOK", "Built-in crash guards installation complete");
}

} // namespace native_hooks
