// modloader/src/core/ue_memory.cpp
// Runtime resolution of UE's FMallocBinned2 allocator functions.
//
// Uses symbols::lib_base() + known offsets to find GMalloc instance and
// call Malloc/Realloc/Free directly (NOT through vtable — vtable layout
// varies between builds and the offsets are unreliable).
//
// Function offsets confirmed via bindump analysis of libUnreal.so:
//   FMallocBinned2::Malloc  = base + 0x134C410  (this, size, alignment) -> void*
//   FMallocBinned2::Realloc = base + 0x134C860  (this, ptr, size, alignment) -> void*
//   FMallocBinned2::Free    = base + 0x134CD48  (this, ptr)

#include "modloader/ue_memory.h"
#include "modloader/symbols.h"
#include "modloader/logger.h"
#include "modloader/safe_call.h"

#include <cstring>
#include <atomic>

namespace ue_memory
{
    // Function pointer types matching FMallocBinned2 signatures
    using FnMalloc = void *(*)(void *thisptr, size_t count, uint32_t alignment);
    using FnRealloc = void *(*)(void *thisptr, void *original, size_t count, uint32_t alignment);
    using FnFree = void (*)(void *thisptr, void *original);

    // Resolved GMalloc instance pointer
    static std::atomic<void *> s_gmalloc{nullptr};
    static std::atomic<bool> s_initialized{false};

    // Resolved function pointers (direct, not vtable)
    static FnMalloc s_fn_malloc = nullptr;
    static FnRealloc s_fn_realloc = nullptr;
    static FnFree s_fn_free = nullptr;

    // Known offsets for PFXVR libUnreal.so (confirmed via bindump)
    static constexpr uintptr_t GMALLOC_OFFSET_PFXVR = 0x71BDB90; // GMalloc global in .bss
    static constexpr uintptr_t MALLOC_OFFSET_PFXVR = 0x134C410;  // FMallocBinned2::Malloc
    static constexpr uintptr_t REALLOC_OFFSET_PFXVR = 0x134C860; // FMallocBinned2::Realloc
    static constexpr uintptr_t FREE_OFFSET_PFXVR = 0x134CD48;    // FMallocBinned2::Free

    static bool try_resolve()
    {
        uintptr_t base = symbols::lib_base();
        if (base == 0)
        {
            logger::log_error("UE_MEM", "Cannot resolve: lib_base is 0");
            return false;
        }

        logger::log_info("UE_MEM", "Engine library base at %p", (void *)base);

        // Resolve GMalloc instance
        uintptr_t gmalloc_ptr_addr = base + GMALLOC_OFFSET_PFXVR;

        if (!safe_call::probe_read(reinterpret_cast<void *>(gmalloc_ptr_addr), 8))
        {
            logger::log_warn("UE_MEM", "Cannot read at base+0x%lX (%p)",
                             (unsigned long)GMALLOC_OFFSET_PFXVR, (void *)gmalloc_ptr_addr);
            return false;
        }

        void *gmalloc_instance = *reinterpret_cast<void **>(gmalloc_ptr_addr);

        if (!gmalloc_instance)
        {
            logger::log_warn("UE_MEM", "GMalloc at %p is NULL — not yet initialized",
                             (void *)gmalloc_ptr_addr);
            return false;
        }

        // Resolve function pointers directly by offset
        auto fn_malloc = reinterpret_cast<FnMalloc>(base + MALLOC_OFFSET_PFXVR);
        auto fn_realloc = reinterpret_cast<FnRealloc>(base + REALLOC_OFFSET_PFXVR);
        auto fn_free = reinterpret_cast<FnFree>(base + FREE_OFFSET_PFXVR);

        // Validate all three are readable code
        if (!safe_call::probe_read(reinterpret_cast<void *>(fn_malloc), 4) ||
            !safe_call::probe_read(reinterpret_cast<void *>(fn_realloc), 4) ||
            !safe_call::probe_read(reinterpret_cast<void *>(fn_free), 4))
        {
            logger::log_error("UE_MEM", "Function pointers not readable — offsets may be wrong");
            return false;
        }

        logger::log_info("UE_MEM", "GMalloc instance: %p", gmalloc_instance);
        logger::log_info("UE_MEM", "  Malloc  = %p (base+0x%lX)", (void *)fn_malloc, (unsigned long)MALLOC_OFFSET_PFXVR);
        logger::log_info("UE_MEM", "  Realloc = %p (base+0x%lX)", (void *)fn_realloc, (unsigned long)REALLOC_OFFSET_PFXVR);
        logger::log_info("UE_MEM", "  Free    = %p (base+0x%lX)", (void *)fn_free, (unsigned long)FREE_OFFSET_PFXVR);

        s_gmalloc.store(gmalloc_instance);
        s_fn_malloc = fn_malloc;
        s_fn_realloc = fn_realloc;
        s_fn_free = fn_free;

        return true;
    }

    // ═══ PUBLIC API ═════════════════════════════════════════════════════════

    bool init()
    {
        if (s_initialized.load())
            return s_gmalloc.load() != nullptr;

        s_initialized.store(true);

        if (try_resolve())
        {
            logger::log_info("UE_MEM", "UE memory allocator initialized successfully (direct dispatch)");
            return true;
        }

        logger::log_warn("UE_MEM", "UE memory allocator NOT available — TMap growth will fall back to libc malloc");
        return false;
    }

    bool available()
    {
        return s_gmalloc.load() != nullptr;
    }

    void *malloc(size_t size, uint32_t alignment)
    {
        void *gm = s_gmalloc.load();
        if (!gm)
            return ::malloc(size);

        return s_fn_malloc(gm, size, alignment);
    }

    void *realloc(void *original, size_t size, uint32_t alignment)
    {
        void *gm = s_gmalloc.load();
        if (!gm)
            return ::realloc(original, size);

        return s_fn_realloc(gm, original, size, alignment);
    }

    void free(void *original)
    {
        if (!original)
            return;

        void *gm = s_gmalloc.load();
        if (!gm)
        {
            ::free(original);
            return;
        }

        s_fn_free(gm, original);
    }

} // namespace ue_memory
