#pragma once
// modloader/include/modloader/ue_memory.h
// UE allocator wrapper — resolves GMalloc at runtime and provides
// FMemory::Malloc/Realloc/Free equivalents for use in TArray/TMap growth.
//
// On ARM64 Itanium ABI, the FMalloc vtable layout (from binary analysis):
//   +48 = Malloc(SIZE_T Count, uint32 Alignment)
//   +64 = Realloc(void* Original, SIZE_T Count, uint32 Alignment)
//   +80 = Free(void* Original)

#include <cstddef>
#include <cstdint>

namespace ue_memory
{
    // Initialize — find GMalloc pointer via pattern scanning.
    // Must be called AFTER pattern::init().
    // Returns true if GMalloc was resolved successfully.
    bool init();

    // Returns true if UE allocator is available
    bool available();

    // UE-compatible allocation functions (go through GMalloc vtable)
    void *malloc(size_t size, uint32_t alignment = 16);
    void *realloc(void *original, size_t size, uint32_t alignment = 16);
    void free(void *original);

} // namespace ue_memory
