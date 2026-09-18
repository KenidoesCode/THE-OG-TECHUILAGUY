#pragma once

#include <stdint.h>

// A real ELF32/i386 loader's *validation and planning* stage: pure
// arithmetic over a raw byte buffer, no allocation, no paging, no
// kernel dependency of any kind. Deliberately kept hardware/OS-
// independent so it can be exhaustively unit-tested with a hosted
// compiler (tests/elf_test.cpp/elf_test.sh), the same reasoning
// tests/heap_test.cpp and tests/keyboard_translation_test.cpp already
// apply to their own pure-logic pieces — the actual mapping/copying
// into a real address space happens separately, in
// scheduler_create_elf_user_task (scheduler/scheduler.cpp), which
// calls elf_validate_and_plan() first and only proceeds if it
// succeeds.
//
// v1 scope (see docs/ADR/0004-elf-loader.md for the full design and
// exact reasoning): ELF32, i386, ET_EXEC only. No PIE, no dynamic
// linking, no relocations, no section-header processing at all (only
// the program header table is read — a statically-linked, fixed-
// address executable needs nothing else to run). At most
// ELF_MAX_SEGMENTS PT_LOAD segments, using at most ELF_MAX_PAGES
// physical pages in total, all page-aligned, all within a fixed user
// virtual-address window reserved for ELF segments
// (ELF_SEGMENT_VIRTUAL_BASE..+ELF_MAX_PAGES pages) — this is not a
// general-purpose loader for an arbitrary Linux ELF binary; it's a
// loader for a class of binaries this project's own toolchain
// produces (see userland/README.md for the build path).

inline constexpr uint32_t ELF_MAX_SEGMENTS = 4;
inline constexpr uint32_t ELF_MAX_PAGES = 8;
inline constexpr uint32_t ELF_PAGE_SIZE = 4096;

// Fixed virtual window every ELF-loaded task's PT_LOAD segments must
// fit inside, and the fixed stack placed immediately after it — both
// per-address-space (see paging.hpp), so every ELF task uses the
// identical virtual layout; what differs between tasks is only which
// physical pages their own address space's tables translate these
// virtual addresses to, exactly like the existing flat-binary path's
// USER_CODE_VADDR/USER_STACK_*.
inline constexpr uint32_t ELF_SEGMENT_VIRTUAL_BASE = 0x01000000;
inline constexpr uint32_t ELF_STACK_PAGE_VADDR =
    ELF_SEGMENT_VIRTUAL_BASE + ELF_MAX_PAGES * ELF_PAGE_SIZE;
inline constexpr uint32_t ELF_STACK_TOP_VADDR =
    ELF_STACK_PAGE_VADDR + ELF_PAGE_SIZE;

enum class ElfError {
    None,
    ImageTooSmallForHeader,
    BadMagic,
    UnsupportedClass,
    UnsupportedEndianness,
    UnsupportedVersion,
    UnsupportedType,
    UnsupportedMachine,
    ProgramHeaderTableOutOfBounds,
    TooManySegments,
    NoLoadSegments,
    SegmentOffsetOutOfBounds,
    SegmentSizeInvalid,
    SegmentNotPageAligned,
    SegmentOutsideAllowedRegion,
    SegmentOverlap,
    TooManyPages,
    EntryPointNotInAnyExecutableSegment
};

struct ElfSegmentPlan {
    uint32_t virtualAddress;  // page-aligned
    uint32_t fileOffset;
    uint32_t fileSize;        // <= memSize
    uint32_t memSize;         // page-count is derived from this
    bool writable;
    bool executable;
};

struct ElfLoadPlan {
    uint32_t entryPoint;
    uint32_t segmentCount;
    ElfSegmentPlan segments[ELF_MAX_SEGMENTS];
    uint32_t totalPages;
};

// Validates `image` (imageSize bytes) as a supported ELF32/i386
// ET_EXEC image and, on success, fills `outPlan` with everything
// needed to actually load it (segment layout, permissions, entry
// point) without needing to re-read the raw ELF structures again.
// Every offset/size/address/alignment/range check runs before any
// value derived from the file is trusted for arithmetic that could
// overflow or read out of bounds — see elf.cpp for exactly which
// invariant each check enforces.
ElfError elf_validate_and_plan(
    const uint8_t* image,
    uint32_t imageSize,
    ElfLoadPlan& outPlan
);
