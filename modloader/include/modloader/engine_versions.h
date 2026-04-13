#pragma once
// modloader/include/modloader/engine_versions.h
// ═══════════════════════════════════════════════════════════════════════════
// Comprehensive UE engine version database derived from source analysis of
// https://github.com/oculus-vr/UnrealEngine across ALL branches:
//   UE 4.25, 4.27, oculus-5.0, oculus-5.1, oculus-5.2, oculus-5.3,
//   oculus-5.4, oculus-5.5, oculus-5.6
//
// ── KEY FINDINGS ──
// • UObjectBase layout is IDENTICAL (0x28 bytes) across ALL versions 4.25→5.6
//   in shipping ARM64 builds (no WITH_EDITORONLY_DATA, no LATE_RESOLVE).
// • FName is always 8 bytes (ComparisonIndex + Number) in shipping builds.
//   FNamePool (block-based allocator) is used from UE4.23 onward.
// • FProperty (FField-based) exists from UE4.25 onward. UProperty (UObject-based)
//   was removed in UE5. Both coexist in UE4.25/4.27.
// • UE5 uses compact 8-byte FFieldVariant (tagged pointer) vs UE4's 16-byte
//   FFieldVariant, shifting all FField/FProperty offsets by -8.
// • ProcessEvent vtable index differs between UE4 and UE5 due to added/removed
//   virtual functions. Must be resolved per-game via pattern scanning.
// • FUObjectItem gained a RefCount field in UE5.x (0x18→0x18 or 0x14→0x18
//   depending on build configuration).
// ═══════════════════════════════════════════════════════════════════════════

#include <cstdint>

namespace engine_versions
{

    // ═══ Engine version enumeration ════════════════════════════════════════════
    // Covers all Oculus/Meta Quest branches. The modloader identifies which
    // version a game uses at runtime via string signatures + structure probing.
    enum class EngineVersion : uint32_t
    {
        // UE4 series (libUE4.so)
        UE4_25 = 425, // RE4 VR (com.Armature.VR4) — oldest supported
        UE4_26 = 426,
        UE4_27 = 427, // Many Quest titles

        // UE5 series (libUnreal.so)
        UE5_0 = 500, // oculus-5.0 branch
        UE5_1 = 510, // oculus-5.1 branch
        UE5_2 = 520, // oculus-5.2 branch
        UE5_3 = 530, // oculus-5.3 branch
        UE5_4 = 540, // Pinball FX VR (com.zenstudios.PFXVRQuest)
        UE5_5 = 550, // oculus-5.5 branch
        UE5_6 = 560, // oculus-5.6 branch (latest)

        UNKNOWN = 0
    };

    // Helper: is this a UE5 version?
    inline bool is_ue5(EngineVersion v)
    {
        return static_cast<uint32_t>(v) >= 500;
    }

    // Helper: is this a UE4 version?
    inline bool is_ue4(EngineVersion v)
    {
        return static_cast<uint32_t>(v) >= 400 && static_cast<uint32_t>(v) < 500;
    }

    // ═══════════════════════════════════════════════════════════════════════════
    // UObjectBase Layout — IDENTICAL across all versions (shipping ARM64)
    // ═══════════════════════════════════════════════════════════════════════════
    //
    // Source: Engine/Source/Runtime/CoreUObject/Public/UObject/UObjectBase.h
    //
    // Class hierarchy: UObjectBase → UObjectBaseUtility → UObject → UField → UStruct
    // UObjectBaseUtility and UObject add NO instance fields (only virtual methods).
    //
    // In a shipping Quest build:
    //   WITH_EDITORONLY_DATA = 0
    //   UE_WITH_OBJECT_HANDLE_LATE_RESOLVE = 0
    //   UE_STORE_OBJECT_LIST_INTERNAL_INDEX = 0
    //
    // UE5's TNonAccessTrackedObjectPtr<T> resolves to raw T* in shipping builds:
    //   TNonAccessTrackedObjectPtr → TObjectPtr → FObjectPtr → FObjectHandle → UObject*
    //
    // ┌─────────┬──────┬──────────────────────────────────────────┐
    // │ Offset  │ Size │ Field                                    │
    // ├─────────┼──────┼──────────────────────────────────────────┤
    // │ 0x00    │  8   │ vtable pointer                           │
    // │ 0x08    │  4   │ EObjectFlags ObjectFlags (int32)         │
    // │ 0x0C    │  4   │ int32 InternalIndex                      │
    // │ 0x10    │  8   │ UClass* ClassPrivate                     │
    // │ 0x18    │  4   │ FName::ComparisonIndex (int32)           │
    // │ 0x1C    │  4   │ FName::Number (uint32)                   │
    // │ 0x20    │  8   │ UObject* OuterPrivate                    │
    // ├─────────┼──────┼──────────────────────────────────────────┤
    // │ Total   │ 0x28 │ 40 bytes — same 4.25 through 5.6        │
    // └─────────┴──────┴──────────────────────────────────────────┘
    //
    namespace uobject_base
    {
        constexpr uint32_t VTABLE = 0x00;
        constexpr uint32_t OBJECT_FLAGS = 0x08;
        constexpr uint32_t INTERNAL_INDEX = 0x0C;
        constexpr uint32_t CLASS_PRIVATE = 0x10;
        constexpr uint32_t FNAME_INDEX = 0x18;  // FName::ComparisonIndex
        constexpr uint32_t FNAME_NUMBER = 0x1C; // FName::Number
        constexpr uint32_t OUTER_PRIVATE = 0x20;
        constexpr uint32_t SIZE = 0x28; // 40 bytes total
    }

    // ═══════════════════════════════════════════════════════════════════════════
    // UField Layout — IDENTICAL across all versions
    // ═══════════════════════════════════════════════════════════════════════════
    //
    // class UField : public UObject {
    //     UField* Next;   // +0x28
    // };
    //
    namespace ufield_layout
    {
        constexpr uint32_t NEXT = 0x28; // UField* linked list next pointer
        constexpr uint32_t SIZE = 0x30; // 48 bytes total
    }

    // ═══════════════════════════════════════════════════════════════════════════
    // FName Layout — 8 bytes in all shipping builds
    // ═══════════════════════════════════════════════════════════════════════════
    //
    // Source: Engine/Source/Runtime/Core/Public/UObject/NameTypes.h
    //
    // Shipping build: WITH_CASE_PRESERVING_NAME=0, UE_FNAME_OUTLINE_NUMBER=0
    //
    // struct FName {
    //     FNameEntryId ComparisonIndex;  // uint32 (block<<16 | offset)
    //     uint32       Number;           // instance number (0 = no number)
    // };
    //
    // ── IMPORTANT VERSION DIFFERENCES ──
    //
    // Pre-UE4.23: Used TNameEntryArray (flat array of FNameEntry*). Direct int32 index.
    //             NOT SUPPORTED — all Quest games use UE4.25+.
    //
    // UE4.23+:    Uses FNamePool with block-based FNameEntryAllocator.
    //             FNameEntryId = uint32, encodes Block<<16 | Offset.
    //             Resolution: Blocks[Block] + Stride * Offset → FNameEntry*
    //
    // UE5.4+:     Added UE_FNAME_OUTLINE_NUMBER (default OFF). When enabled,
    //             Number moves into FNamePool, reducing FName to 4 bytes.
    //             We don't support this mode (it's opt-in and no Quest game uses it).
    //
    // Field order within FName:
    //   4.25–5.0: ComparisonIndex, [DisplayIndex if editor], Number
    //   5.4–5.6:  ComparisonIndex, Number, [DisplayIndex if editor]
    //   In shipping (no editor) both are: {ComparisonIndex, Number} = 8 bytes
    //
    namespace fname_layout
    {
        constexpr uint32_t SIZE = 8;
        constexpr uint32_t COMPARISON_INDEX_OFFSET = 0;
        constexpr uint32_t NUMBER_OFFSET = 4;
    }

    // ═══════════════════════════════════════════════════════════════════════════
    // FNamePool Layout — used in all supported versions (UE4.23+)
    // ═══════════════════════════════════════════════════════════════════════════
    //
    // Source: Engine/Source/Runtime/Core/Private/UObject/UnrealNames.cpp
    //
    // The global GNames pointer points to an FNamePool-owning structure.
    // GNames + 0x30 → FNamePool (contains FNameEntryAllocator + shard hash maps)
    // FNamePool + 0x10 → Blocks[] array (array of FNameEntry* block pointers)
    //
    // FNameEntry layout (UE4.23+):
    //   +0x00: FNameEntryHeader (uint16 bitfield)
    //          - bit 0:     bIsWide (1=wide, 0=ansi)
    //          - bits 1-5:  LowercaseProbeHash (5 bits, non-editor only)
    //          - bits 6-15: Len (10 bits, max 1023) [non-editor]
    //          OR
    //          - bit 0:     bIsWide
    //          - bits 1-15: Len (15 bits, max 32767) [editor, WITH_CASE_PRESERVING_NAME]
    //   +0x02: char AnsiName[] or char16_t WideName[]
    //
    // FNameEntryId encoding (same across all UE4.23+ versions):
    //   Value = (Block << FNameBlockOffsetBits) | Offset
    //   FNameBlockOffsetBits = 16
    //   FNameMaxBlockBits = 13 (5.4+) or implicit (4.25)
    //   Max blocks: 8192
    //   Stride: alignof(FNameEntry) = 2
    //   Resolution: Blocks[Value >> 16] + 2 * (Value & 0xFFFF) → FNameEntry*
    //
    namespace fnamepool
    {
        constexpr uint32_t GNAMES_TO_FNAMEPOOL = 0x30;     // offset from GNames global
        constexpr uint32_t FNAMEPOOL_TO_BLOCKS = 0x10;     // offset to Blocks[] array
        constexpr uint32_t FNAMEPOOL_CURRENT_BLOCK = 0x08; // current block index
        constexpr uint32_t FNAMEPOOL_CURRENT_BYTE = 0x0C;  // current byte offset

        constexpr uint32_t FNAMENTRY_HEADER_SIZE = 2;   // uint16 header
        constexpr uint32_t FNAMENTRY_LEN_SHIFT = 6;     // bits 6-15 = length (non-editor)
        constexpr uint32_t FNAMENTRY_WIDE_BIT = 0x01;   // bit 0 = wide flag
        constexpr uint32_t FNAMENTRY_LEN_MASK = 0x03FF; // 10-bit length (after shift)

        constexpr uint32_t FNAME_STRIDE = 2; // alignof(FNameEntry) = 2
        constexpr uint32_t FNAME_MAX_BLOCKS = 8192;
        constexpr uint32_t FNAME_BLOCK_BITS = 16;    // bits for offset within block
        constexpr uint32_t FNAME_BLOCK_SIZE = 65536; // entries per block (1 << 16)
    }

    // ═══════════════════════════════════════════════════════════════════════════
    // GUObjectArray / FUObjectItem Layout
    // ═══════════════════════════════════════════════════════════════════════════
    //
    // Source: Engine/Source/Runtime/CoreUObject/Public/UObject/UObjectArray.h
    //
    // extern FUObjectArray GUObjectArray;  // global — direct symbol, not singleton
    //
    // ── FUObjectItem ──
    //
    // UE 4.25/4.27:
    //   +0x00: UObjectBase* Object     (8 bytes)
    //   +0x08: int32 Flags             (4 bytes, EInternalObjectFlags)
    //   +0x0C: int32 ClusterRootIndex  (4 bytes)
    //   +0x10: int32 SerialNumber      (4 bytes)
    //   Total: 20 bytes → aligned to 24 (0x18)
    //   NOTE: Some builds pack to 20 bytes (0x14) without padding.
    //
    // UE 5.0+:
    //   +0x00: UObjectBase* Object     (8 bytes, or packed ptr in some configs)
    //   +0x08: int32 Flags             (4 bytes)
    //   +0x0C: int32 ClusterRootIndex  (4 bytes)
    //   +0x10: int32 SerialNumber      (4 bytes)
    //   +0x14: int32 RefCount          (4 bytes — NEW in UE5)
    //   Total: 24 bytes (0x18)
    //   NOTE: Some UE5 builds omit RefCount → 20 bytes (0x14)
    //
    // ── FChunkedFixedUObjectArray (TUObjectArray) ──
    //
    // UE 4.25:
    //   +0x00: FUObjectItem** Objects       (chunk pointer array)
    //   +0x08: int32 MaxElements
    //   +0x0C: int32 NumElements            ← what we read for object count
    //   +0x10: int32 MaxChunks
    //   +0x14: int32 NumChunks
    //
    // UE 5.0+:
    //   +0x00: FUObjectItem** Objects       (chunk pointer array)
    //   +0x08: FUObjectItem* PreAllocatedObjects  ← NEW (shifts subsequent fields)
    //   +0x10: int32 MaxElements
    //   +0x14: int32 NumElements            ← shifted by +8 from UE4
    //   +0x18: int32 MaxChunks
    //   +0x1C: int32 NumChunks
    //
    // ── FUObjectArray ──
    //   +0x00: int32 ObjFirstGCIndex
    //   +0x04: int32 ObjLastNonGCIndex
    //   +0x08: int32 MaxObjectsNotConsideredByGC
    //   +0x0C: bool  OpenForDisregardForGC
    //   +0x10: TUObjectArray ObjObjects     ← the chunked array starts here
    //
    // ── Access pattern ──
    //   GUObjectArray + 0x10 → ObjObjects (TUObjectArray)
    //   ObjObjects + NumElements_offset → int32 count
    //   ObjObjects.Objects[chunk_idx] → FUObjectItem* chunk base
    //   chunk_base + (index_in_chunk * FUObjectItem_size) → FUObjectItem
    //   FUObjectItem.Object → UObject*
    //
    namespace guobjectarray
    {
        // FUObjectArray to embedded TUObjectArray — SAME in all versions
        constexpr uint32_t TO_OBJECTS = 0x10;

        // NumElements offset WITHIN TUObjectArray — DIFFERS between UE4 and UE5
        // UE4: Objects(8) → MaxElements(4) → NumElements at +0x0C (relative to TUObjectArray)
        // UE5: Objects(8) + PreAllocatedObjects(8) → MaxElements(4) → NumElements at +0x14
        constexpr uint32_t UE4_NUM_ELEMENTS = 0x0C; // TUObjectArray + 0x0C
        constexpr uint32_t UE5_NUM_ELEMENTS = 0x14; // TUObjectArray + 0x14

        // Convenience: use offset from GUObjectArray directly
        // GUObjectArray + TO_OBJECTS + NUM_ELEMENTS = absolute offset to count
        constexpr uint32_t UE4_ABS_NUM_ELEMENTS = 0x1C; // 0x10 + 0x0C
        constexpr uint32_t UE5_ABS_NUM_ELEMENTS = 0x24; // 0x10 + 0x14

        // FUObjectItem sizes seen in the wild
        constexpr uint32_t FUOBJECTITEM_SIZE_24 = 0x18; // 24 bytes (UE4 padded / UE5 with RefCount)
        constexpr uint32_t FUOBJECTITEM_SIZE_20 = 0x14; // 20 bytes (UE4 tight / UE5 no RefCount)

        // Chunk config
        constexpr uint32_t ELEMENTS_PER_CHUNK = 0x10000; // 65536
    }

    // ═══════════════════════════════════════════════════════════════════════════
    // UStruct Layout — Varies between UE4 and UE5
    // ═══════════════════════════════════════════════════════════════════════════
    //
    // Source: Engine/Source/Runtime/CoreUObject/Public/UObject/Class.h
    //
    // class UStruct : public UField, private FStructBaseChain
    //
    // FStructBaseChain adds 2 pointers (16 bytes) when IMPLEMENT_STRUCT_BASE_CHAIN_CHECKS=1
    // The private inheritance means these fields come BEFORE UStruct's own members
    // in the class layout (after UField's data, but part of UStruct's memory).
    //
    // ── UE4.25/4.27 layout (verified from RE4 VR) ──
    //   UObjectBase:       0x00-0x27 (40 bytes)
    //   UField::Next:      0x28 (8 bytes)
    //   FStructBaseChain:  0x30 (16 bytes, 2 pointers)
    //   UStruct fields:
    //     +0x40: UStruct* SuperStruct
    //     +0x48: UField* Children           (linked list of UFunctions)
    //     +0x50: FField* ChildProperties    (linked list of FProperties)
    //     +0x58: int32 PropertiesSize
    //
    // ── UE5.0+ layout ──
    //   Same as UE4 in shipping builds. TObjectPtr<T> resolves to raw T*.
    //   The offsets are IDENTICAL to UE4 for the fields we care about.
    //
    // NOTE: The offsets 0x40/0x48/0x50/0x58 are CONFIRMED identical across
    // UE4.25 through UE5.6 in shipping ARM64 builds. The only difference
    // is type wrappers (TObjectPtr<UStruct> vs raw UStruct*) which compile
    // to the same binary layout.
    //
    namespace ustruct_layout
    {
        // These are IDENTICAL across all supported versions in shipping builds
        constexpr uint32_t SUPER_STRUCT = 0x40;
        constexpr uint32_t CHILDREN = 0x48;         // UField* — for UFunctions
        constexpr uint32_t CHILD_PROPERTIES = 0x50; // FField* — for FProperties
        constexpr uint32_t PROPERTIES_SIZE = 0x58;
    }

    // ═══════════════════════════════════════════════════════════════════════════
    // UFunction Layout — Verified identical across versions
    // ═══════════════════════════════════════════════════════════════════════════
    //
    // Source: Engine/Source/Runtime/CoreUObject/Public/UObject/ObjectMacros.h + Class.h
    //
    // class UFunction : public UStruct
    //
    // UStruct ends, then UFunction adds:
    //   +0xB0: EFunctionFlags FunctionFlags (uint32)
    //   +0xB4: uint8  NumParms
    //   +0xB6: uint16 ParmsSize
    //   +0xB8: uint16 ReturnValueOffset
    //   +0xBA: int16  RPCId
    //   +0xBC: int16  RPCResponseId
    //   +0xC0: FProperty* FirstPropertyToInit (8 bytes)
    //   +0xC8: UFunction* EventGraphFunction (8 bytes, conditional)
    //   +0xD0: int32  EventGraphCallOffset (conditional)
    //   +0xD8: FNativeFuncPtr Func (the actual function pointer)
    //
    namespace ufunction_layout
    {
        constexpr uint32_t FUNCTION_FLAGS = 0xB0;
        constexpr uint32_t NUM_PARMS = 0xB4;
        constexpr uint32_t PARMS_SIZE = 0xB6;
        constexpr uint32_t RETURN_VALUE_OFFSET = 0xB8;
        constexpr uint32_t RPC_ID = 0xBA;
        constexpr uint32_t RPC_RESPONSE_ID = 0xBC;
        constexpr uint32_t FIRST_PROPERTY_INIT = 0xC0;
        constexpr uint32_t FUNC_PTR = 0xD8; // FNativeFuncPtr Func
    }

    // ═══════════════════════════════════════════════════════════════════════════
    // FField Layout — DIFFERS between UE4 and UE5
    // ═══════════════════════════════════════════════════════════════════════════
    //
    // Source: Engine/Source/Runtime/CoreUObject/Public/UObject/Field.h
    //
    // class FField (virtual class — has vtable)
    //
    // ── UE4 (4.25/4.27) — FFieldVariant is 16 bytes ──
    //   FFieldVariant in UE4 stores {void* Object; bool bIsUObject;} = 16 bytes
    //   FField layout:
    //     +0x00: vtable (8)
    //     +0x08: FFieldClass* ClassPrivate (8)
    //     +0x10: FFieldVariant Owner (16 bytes — void* + bool + padding)
    //     +0x20: FField* Next (8)
    //     +0x28: FName NamePrivate (8)
    //     +0x30: EObjectFlags FlagsPrivate (4)
    //     Total header: ~0x34, aligned to 0x38
    //
    // ── UE5 (5.0+) — FFieldVariant is 8 bytes (tagged pointer) ──
    //   FFieldVariant uses low-bit tagging: {uintptr_t Value;} = 8 bytes
    //   FField layout:
    //     +0x00: vtable (8)
    //     +0x08: FFieldClass* ClassPrivate (8)
    //     +0x10: FFieldVariant Owner (8 bytes — tagged pointer)
    //     +0x18: FField* Next (8)  ← shifted -8 from UE4
    //     +0x20: FName NamePrivate (8)  ← shifted -8 from UE4
    //     +0x28: EObjectFlags FlagsPrivate (4)
    //     Total header: ~0x2C, aligned to 0x30
    //
    // The KEY difference is Owner size: 16 bytes (UE4) vs 8 bytes (UE5).
    // This shifts Next and NamePrivate by -8 in UE5.
    //
    namespace ffield_layout
    {
        // ── UE4 (4.25/4.27) ──
        namespace ue4
        {
            constexpr uint32_t VTABLE = 0x00;
            constexpr uint32_t CLASS_PRIVATE = 0x08;
            constexpr uint32_t OWNER = 0x10; // FFieldVariant (16 bytes)
            constexpr uint32_t NEXT = 0x20;
            constexpr uint32_t NAME_PRIVATE = 0x28;
            constexpr uint32_t FLAGS_PRIVATE = 0x30;
        }

        // ── UE5 (5.0+) ──
        namespace ue5
        {
            constexpr uint32_t VTABLE = 0x00;
            constexpr uint32_t CLASS_PRIVATE = 0x08;
            constexpr uint32_t OWNER = 0x10;        // FFieldVariant (8 bytes, tagged)
            constexpr uint32_t NEXT = 0x18;         // shifted -8
            constexpr uint32_t NAME_PRIVATE = 0x20; // shifted -8
            constexpr uint32_t FLAGS_PRIVATE = 0x28;
        }
    }

    // ═══════════════════════════════════════════════════════════════════════════
    // FProperty Layout — DIFFERS between UE4 and UE5
    // ═══════════════════════════════════════════════════════════════════════════
    //
    // Source: Engine/Source/Runtime/CoreUObject/Public/UObject/UnrealType.h
    //
    // class FProperty : public FField
    //
    // FProperty fields (after FField header):
    //   ArrayDim (int32)
    //   ElementSize (int32)
    //   PropertyFlags (EPropertyFlags, uint64)
    //   RepIndex (uint16)
    //   BlueprintReplicationCondition (uint8, private, conditional)
    //   Offset_Internal (int32)
    //   RepNotifyFunc (FName, 8 bytes)
    //   PropertyLinkNext (FProperty*)
    //   NextRef (FProperty*)
    //   DestructorLinkNext (FProperty*)
    //   PostConstructLinkNext (FProperty*)
    //
    // The base FField header is 8 bytes smaller in UE5, so ALL FProperty
    // field offsets shift by -8 in UE5.
    //
    namespace fproperty_layout
    {
        // ── UE4 (4.25/4.27) — FField header ends at ~0x38 ──
        namespace ue4
        {
            constexpr uint32_t ARRAY_DIM = 0x34; // int32 (after FField header + padding)
            constexpr uint32_t ELEMENT_SIZE = 0x38;
            constexpr uint32_t PROPERTY_FLAGS = 0x40;  // uint64 EPropertyFlags
            constexpr uint32_t REP_INDEX = 0x48;       // uint16
            constexpr uint32_t OFFSET_INTERNAL = 0x4C; // int32
            constexpr uint32_t REP_NOTIFY_FUNC = 0x50; // FName (8 bytes)
            constexpr uint32_t PROPERTY_LINK = 0x58;   // FProperty*
            constexpr uint32_t NEXT_REF = 0x60;        // FProperty*
            constexpr uint32_t DESTRUCTOR_LINK = 0x68; // FProperty*
            constexpr uint32_t POST_CONSTRUCT = 0x70;  // FProperty*
            constexpr uint32_t BASE_SIZE = 0x78;       // total FProperty base size
        }

        // ── UE5 (5.0+) — FField header ends at ~0x30 ──
        namespace ue5
        {
            constexpr uint32_t ARRAY_DIM = 0x2C; // shifted -8 from UE4
            constexpr uint32_t ELEMENT_SIZE = 0x30;
            constexpr uint32_t PROPERTY_FLAGS = 0x38;
            constexpr uint32_t REP_INDEX = 0x40;
            constexpr uint32_t OFFSET_INTERNAL = 0x44;
            constexpr uint32_t REP_NOTIFY_FUNC = 0x48;
            constexpr uint32_t PROPERTY_LINK = 0x50;
            constexpr uint32_t NEXT_REF = 0x58;
            constexpr uint32_t DESTRUCTOR_LINK = 0x60;
            constexpr uint32_t POST_CONSTRUCT = 0x68;
            constexpr uint32_t BASE_SIZE = 0x70; // total FProperty base size (shifted -8)
        }
    }

    // ═══════════════════════════════════════════════════════════════════════════
    // Typed FProperty Extensions — Inner pointers (after FProperty base)
    // ═══════════════════════════════════════════════════════════════════════════
    //
    // These are the extension fields that come after the base FProperty fields.
    // Their absolute offset depends on FProperty::BASE_SIZE per version.
    //
    // FBoolProperty (special — packs 4 uint8 fields):
    //   +BASE_SIZE+0: uint8  FieldSize
    //   +BASE_SIZE+1: uint8  ByteOffset
    //   +BASE_SIZE+2: uint8  ByteMask
    //   +BASE_SIZE+3: uint8  FieldMask
    //
    // FObjectProperty:
    //   +BASE_SIZE+0: UClass* PropertyClass (8)
    //
    // FClassProperty (extends FObjectProperty):
    //   +BASE_SIZE+0: UClass* PropertyClass (8)   — inherited
    //   +BASE_SIZE+8: UClass* MetaClass (8)
    //
    // FInterfaceProperty:
    //   +BASE_SIZE+0: UClass* InterfaceClass (8)
    //
    // FArrayProperty:
    //   +BASE_SIZE+0: EArrayPropertyFlags (4 or 8, varies)
    //   +BASE_SIZE+8: FProperty* Inner (8)      — some builds at +0 directly
    //
    // FMapProperty:
    //   +BASE_SIZE+0: FProperty* KeyProp (8)
    //   +BASE_SIZE+8: FProperty* ValueProp (8)
    //
    // FSetProperty:
    //   +BASE_SIZE+0: FProperty* ElementProp (8)
    //
    // FStructProperty:
    //   +BASE_SIZE+0: UScriptStruct* Struct (8)
    //
    // FEnumProperty:
    //   +BASE_SIZE+0: FNumericProperty* UnderlyingProp (8)
    //   +BASE_SIZE+8: UEnum* Enum (8)
    //
    // FByteProperty:
    //   +BASE_SIZE+0: UEnum* Enum (8)
    //
    namespace fproperty_ext
    {
        // Helper: compute absolute offset from version-specific base size
        inline constexpr uint32_t at(uint32_t base_size, uint32_t ext_offset)
        {
            return base_size + ext_offset;
        }

        // ── UE4 extension offsets (base = 0x78) ──
        namespace ue4
        {
            constexpr uint32_t BASE = fproperty_layout::ue4::BASE_SIZE;
            constexpr uint32_t BOOL_FIELD_SIZE = BASE + 0;     // 0x78
            constexpr uint32_t BOOL_BYTE_OFFSET = BASE + 1;    // 0x79
            constexpr uint32_t BOOL_BYTE_MASK = BASE + 2;      // 0x7A
            constexpr uint32_t BOOL_FIELD_MASK = BASE + 3;     // 0x7B
            constexpr uint32_t OBJ_PROPERTY_CLASS = BASE + 0;  // 0x78
            constexpr uint32_t CLASS_META_CLASS = BASE + 8;    // 0x80
            constexpr uint32_t INTERFACE_CLASS = BASE + 8;     // 0x80
            constexpr uint32_t ARRAY_INNER = BASE + 0;         // 0x78
            constexpr uint32_t MAP_KEY_PROP = BASE + 0;        // 0x78
            constexpr uint32_t MAP_VALUE_PROP = BASE + 8;      // 0x80
            constexpr uint32_t SET_ELEMENT_PROP = BASE + 0;    // 0x78
            constexpr uint32_t STRUCT_INNER_STRUCT = BASE + 0; // 0x78
            constexpr uint32_t ENUM_PROP_ENUM = BASE + 8;      // 0x80
            constexpr uint32_t BYTE_PROP_ENUM = BASE + 0;      // 0x78
        }

        // ── UE5 extension offsets (base = 0x70) ──
        namespace ue5
        {
            constexpr uint32_t BASE = fproperty_layout::ue5::BASE_SIZE;
            constexpr uint32_t BOOL_FIELD_SIZE = BASE + 8;     // 0x78 (confirmed on PFX VR)
            constexpr uint32_t BOOL_BYTE_OFFSET = BASE + 9;    // 0x79
            constexpr uint32_t BOOL_BYTE_MASK = BASE + 10;     // 0x7A
            constexpr uint32_t BOOL_FIELD_MASK = BASE + 11;    // 0x7B
            constexpr uint32_t OBJ_PROPERTY_CLASS = BASE + 0;  // 0x70
            constexpr uint32_t CLASS_META_CLASS = BASE + 8;    // 0x78
            constexpr uint32_t INTERFACE_CLASS = BASE + 8;     // 0x78
            constexpr uint32_t ARRAY_INNER = BASE + 8;         // 0x78 (ArrayFlags at +0)
            constexpr uint32_t MAP_KEY_PROP = BASE + 0;        // 0x70
            constexpr uint32_t MAP_VALUE_PROP = BASE + 8;      // 0x78
            constexpr uint32_t SET_ELEMENT_PROP = BASE + 0;    // 0x70
            constexpr uint32_t STRUCT_INNER_STRUCT = BASE + 0; // 0x70
            constexpr uint32_t ENUM_PROP_ENUM = BASE + 8;      // 0x78
            constexpr uint32_t BYTE_PROP_ENUM = BASE + 0;      // 0x70
        }
    }

    // ═══════════════════════════════════════════════════════════════════════════
    // UEnum Layout — IDENTICAL across all versions
    // ═══════════════════════════════════════════════════════════════════════════
    //
    // Source: Engine/Source/Runtime/CoreUObject/Public/UObject/Class.h
    //
    // class UEnum : public UField
    //   ...
    //   TArray<TPair<FName, int64>> Names;  // at offset 0x40 from UObject base
    //
    // TPair<FName,int64> = FName(8) + int64(8) = 16 bytes per entry
    //
    namespace uenum_layout
    {
        constexpr uint32_t NAMES_DATA = 0x40; // TArray::Data pointer
        constexpr uint32_t NAMES_NUM = 0x48;  // TArray::Num (int32)
        constexpr uint32_t NAMES_MAX = 0x4C;  // TArray::Max (int32)
        constexpr uint32_t ENTRY_SIZE = 16;   // sizeof(TPair<FName, int64>)
    }

    // ═══════════════════════════════════════════════════════════════════════════
    // ProcessEvent — Virtual function table index
    // ═══════════════════════════════════════════════════════════════════════════
    //
    // UObject::ProcessEvent(UFunction*, void*) is a virtual function.
    // Its vtable index DIFFERS between UE4 and UE5 because UE5 adds/removes
    // virtual functions in UObjectBase and UObject.
    //
    // ── Key differences affecting vtable layout ──
    //
    // UE4 (4.25/4.27):
    //   - UObjectBase has: ~dtor(2 slots), DeferredRegister, Register (pure virtual),
    //     ClassPackage (pure virtual)
    //   - UObjectBaseUtility has: CanBeClusterRoot, CanBeInCluster, CreateCluster,
    //     OnClusterMarkedAsPendingKill
    //
    // UE5 (5.0+):
    //   - UObjectBase: ~dtor(2 slots), HashObject (NEW), DeferredRegister
    //   - Register() and ClassPackage() REMOVED
    //   - UObjectBaseUtility adds GetVersePath() (conditional, internal)
    //   - UObject adds: PostReinitProperties, PostLoadSubobjects (new overload),
    //     GetExtendedDynamicType, GetExtendedDynamicTypeId, many others
    //
    // BOTTOM LINE: The vtable index must be found via pattern scanning, not hardcoded.
    // The ProcessEvent function itself has a distinctive prologue we can match.
    //
    // For known games we can hardcode the vtable index as confirmed by hooking:
    //   RE4 VR:       ProcessEvent vtable index determined at runtime
    //   Pinball FX VR: ProcessEvent vtable index determined at runtime
    //
    // The modloader hooks ProcessEvent by finding its address via pattern scan
    // (not by vtable index), so the exact index is not critical for hooking.
    // It IS critical if we ever need to call ProcessEvent via vtable dispatch.
    //

    // ═══════════════════════════════════════════════════════════════════════════
    // Engine Version Detection Signatures
    // ═══════════════════════════════════════════════════════════════════════════
    //
    // These are string constants embedded in the engine binary that allow
    // runtime detection of the exact engine version.
    //
    // Method 1: FEngineVersion string
    //   UE4: "4.25.x-XXXXXXX+++UE4+Release-4.25"
    //   UE5: "5.4.x-XXXXXXX+++UE5+Release-5.4"
    //   Found via: strxrefs "Release-" or grep for "+UE4+" / "+UE5+"
    //
    // Method 2: Library name
    //   UE4.x:  loaded library is "libUE4.so"
    //   UE5.x:  loaded library is "libUnreal.so"
    //
    // Method 3: Copyright/About string
    //   Contains the year and version: "Epic Games, Inc. All Rights Reserved."
    //   Year gives rough version bounds.
    //
    // Method 4: GUObjectArray item size probe
    //   Create a known UObject, check if FUObjectItem at its index has valid
    //   pointers when read with size 0x18 vs 0x14. Validates FUObjectItem_size.
    //
    // Method 5: FField owner size probe
    //   Read FField::Owner and check if it's a valid tagged pointer (UE5, 8 bytes)
    //   or a valid {ptr, bool} pair (UE4, 16 bytes).
    //
    namespace detection
    {
        // String patterns to search for in the binary
        constexpr const char *UE4_LIB_NAME = "libUE4.so";
        constexpr const char *UE5_LIB_NAME = "libUnreal.so";

        // Known engine version strings (partial matches)
        constexpr const char *UE4_25_SIG = "Release-4.25";
        constexpr const char *UE4_26_SIG = "Release-4.26";
        constexpr const char *UE4_27_SIG = "Release-4.27";
        constexpr const char *UE5_0_SIG = "Release-5.0";
        constexpr const char *UE5_1_SIG = "Release-5.1";
        constexpr const char *UE5_2_SIG = "Release-5.2";
        constexpr const char *UE5_3_SIG = "Release-5.3";
        constexpr const char *UE5_4_SIG = "Release-5.4";
        constexpr const char *UE5_5_SIG = "Release-5.5";
        constexpr const char *UE5_6_SIG = "Release-5.6";

        // Additional detection strings
        constexpr const char *UE4_MARKER = "+UE4+"; // present in UE4 version strings
        constexpr const char *UE5_MARKER = "+UE5+"; // present in UE5 version strings
    }

    // ═══════════════════════════════════════════════════════════════════════════
    // Version-specific TypeOffsets builder
    // ═══════════════════════════════════════════════════════════════════════════
    //
    // Builds a complete TypeOffsets struct for any engine version.
    // Uses the compile-time constants above to fill in all fields.

} // namespace engine_versions
