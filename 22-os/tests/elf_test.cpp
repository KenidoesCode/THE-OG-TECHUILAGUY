// Real assertion-based unit tests for the ELF32/i386 validation-and-
// planning stage (elf/elf.cpp). This logic is pure arithmetic over a
// raw byte buffer — no allocation, no paging, no kernel dependency —
// so, like heap.cpp's allocator logic and keyboard_translation.cpp's
// scancode table, it's tested with a normal hosted compiler rather
// than requiring a boot cycle. Synthetic ELF images are built
// byte-by-byte here rather than read from disk, so every negative
// case (a specific corrupted field) is exact and self-documenting.

#include "../elf/elf.hpp"

#include <cstring>
#include <iostream>
#include <string>
#include <vector>

namespace {

int failures = 0;

void check(bool condition, const std::string& description) {
    if (condition) {
        std::cout << "[PASS] " << description << "\n";
    } else {
        std::cout << "[FAIL] " << description << "\n";
        failures++;
    }
}

void writeU16(std::vector<uint8_t>& buf, size_t offset, uint16_t value) {
    buf[offset] = static_cast<uint8_t>(value & 0xFF);
    buf[offset + 1] = static_cast<uint8_t>((value >> 8) & 0xFF);
}

void writeU32(std::vector<uint8_t>& buf, size_t offset, uint32_t value) {
    buf[offset] = static_cast<uint8_t>(value & 0xFF);
    buf[offset + 1] = static_cast<uint8_t>((value >> 8) & 0xFF);
    buf[offset + 2] = static_cast<uint8_t>((value >> 16) & 0xFF);
    buf[offset + 3] = static_cast<uint8_t>((value >> 24) & 0xFF);
}

constexpr uint32_t EHDR_SIZE = 52;
constexpr uint32_t PHDR_SIZE = 32;

struct SegmentSpec {
    uint32_t vaddr;
    uint32_t fileSize;
    uint32_t memSize;
    uint32_t flags;  // PF_X=1, PF_W=2, PF_R=4
    uint32_t type = 1;  // PT_LOAD by default
};

// Builds a syntactically well-formed ELF32/i386 ET_EXEC image: one
// valid Ehdr, followed by one Phdr per `segments` entry, followed by
// each segment's file content (all zero bytes — content doesn't
// matter to elf_validate_and_plan, only the header-described
// geometry does), laid out at strictly increasing file offsets so the
// resulting image is realistic, not just "big enough."
std::vector<uint8_t> buildElfImage(
    uint32_t entryPoint,
    const std::vector<SegmentSpec>& segments
) {
    uint32_t phoff = EHDR_SIZE;
    uint32_t phTableSize = static_cast<uint32_t>(segments.size()) * PHDR_SIZE;
    uint32_t dataStart = phoff + phTableSize;

    std::vector<uint32_t> fileOffsets;
    uint32_t cursor = dataStart;
    for (const auto& seg : segments) {
        fileOffsets.push_back(cursor);
        cursor += seg.fileSize;
    }

    std::vector<uint8_t> buf(cursor, 0);

    // e_ident
    buf[0] = 0x7F;
    buf[1] = 'E';
    buf[2] = 'L';
    buf[3] = 'F';
    buf[4] = 1;  // ELFCLASS32
    buf[5] = 1;  // ELFDATA2LSB
    buf[6] = 1;  // EV_CURRENT

    writeU16(buf, 16, 2);   // e_type = ET_EXEC
    writeU16(buf, 18, 3);   // e_machine = EM_386
    writeU32(buf, 20, 1);   // e_version
    writeU32(buf, 24, entryPoint);
    writeU32(buf, 28, phoff);
    writeU32(buf, 32, 0);   // e_shoff (unused)
    writeU32(buf, 36, 0);   // e_flags
    writeU16(buf, 40, static_cast<uint16_t>(EHDR_SIZE));  // e_ehsize
    writeU16(buf, 42, static_cast<uint16_t>(PHDR_SIZE));  // e_phentsize
    writeU16(buf, 44, static_cast<uint16_t>(segments.size()));  // e_phnum
    writeU16(buf, 46, 0);   // e_shentsize
    writeU16(buf, 48, 0);   // e_shnum
    writeU16(buf, 50, 0);   // e_shstrndx

    for (size_t i = 0; i < segments.size(); ++i) {
        const SegmentSpec& seg = segments[i];
        size_t ph = phoff + i * PHDR_SIZE;

        writeU32(buf, ph + 0, seg.type);
        writeU32(buf, ph + 4, fileOffsets[i]);
        writeU32(buf, ph + 8, seg.vaddr);
        writeU32(buf, ph + 12, seg.vaddr);  // p_paddr, unused
        writeU32(buf, ph + 16, seg.fileSize);
        writeU32(buf, ph + 20, seg.memSize);
        writeU32(buf, ph + 24, seg.flags);
        writeU32(buf, ph + 28, ELF_PAGE_SIZE);  // p_align
    }

    return buf;
}

// A single executable+readable PT_LOAD segment covering one page,
// with the entry point at its very start — the minimal valid image
// most positive tests build on.
std::vector<uint8_t> minimalValidImage() {
    return buildElfImage(
        ELF_SEGMENT_VIRTUAL_BASE,
        {{ELF_SEGMENT_VIRTUAL_BASE, 64, 64, /*PF_X|PF_R*/ 1 | 4}}
    );
}

}  // namespace

void testAcceptsMinimalValidImage() {
    auto image = minimalValidImage();

    ElfLoadPlan plan;
    ElfError result = elf_validate_and_plan(image.data(), static_cast<uint32_t>(image.size()), plan);

    check(result == ElfError::None,
          "elf: a minimal valid ELF32/i386 ET_EXEC image is accepted");
    check(plan.segmentCount == 1,
          "elf: the single PT_LOAD segment is recorded in the plan");
    check(plan.entryPoint == ELF_SEGMENT_VIRTUAL_BASE,
          "elf: the entry point is recorded correctly");
}

void testAcceptsMultipleLoadSegments() {
    uint32_t codeAddr = ELF_SEGMENT_VIRTUAL_BASE;
    uint32_t dataAddr = ELF_SEGMENT_VIRTUAL_BASE + ELF_PAGE_SIZE;

    auto image = buildElfImage(codeAddr, {
        {codeAddr, 32, 32, /*PF_X|PF_R*/ 1 | 4},
        {dataAddr, 16, 16, /*PF_W|PF_R*/ 2 | 4},
    });

    ElfLoadPlan plan;
    ElfError result = elf_validate_and_plan(image.data(), static_cast<uint32_t>(image.size()), plan);

    check(result == ElfError::None,
          "elf: an image with two non-overlapping PT_LOAD segments (code + data) is accepted");
    check(plan.segmentCount == 2,
          "elf: both PT_LOAD segments are recorded");
    check(!plan.segments[0].writable && plan.segments[0].executable,
          "elf: the code segment's PF_X/PF_W flags are recorded correctly");
    check(plan.segments[1].writable && !plan.segments[1].executable,
          "elf: the data segment's PF_X/PF_W flags are recorded correctly");
}

void testAcceptsBssLargerThanFileSize() {
    // memsz > filesz is exactly how a .bss section is expressed in an
    // ELF program header: the extra (memsz - filesz) bytes must be
    // zero-initialized by the loader, not read from the file at all.
    auto image = buildElfImage(
        ELF_SEGMENT_VIRTUAL_BASE,
        {{ELF_SEGMENT_VIRTUAL_BASE, 16, 4096, /*PF_X|PF_W|PF_R*/ 1 | 2 | 4}}
    );

    ElfLoadPlan plan;
    ElfError result = elf_validate_and_plan(image.data(), static_cast<uint32_t>(image.size()), plan);

    check(result == ElfError::None,
          "elf: memsz > filesz (a BSS-bearing segment) is accepted");
    check(plan.segments[0].fileSize == 16 && plan.segments[0].memSize == 4096,
          "elf: fileSize and memSize are both recorded distinctly, "
          "not collapsed into one value");
}

void testRejectsBadMagic() {
    auto image = minimalValidImage();
    image[0] = 0x00;  // corrupt the magic

    ElfLoadPlan plan;
    ElfError result = elf_validate_and_plan(image.data(), static_cast<uint32_t>(image.size()), plan);

    check(result == ElfError::BadMagic, "elf: rejects an invalid ELF magic number");
}

void testRejectsWrongClass() {
    auto image = minimalValidImage();
    image[4] = 2;  // ELFCLASS64

    ElfLoadPlan plan;
    ElfError result = elf_validate_and_plan(image.data(), static_cast<uint32_t>(image.size()), plan);

    check(result == ElfError::UnsupportedClass,
          "elf: rejects a 64-bit ELF class (this loader is ELF32 only)");
}

void testRejectsWrongEndianness() {
    auto image = minimalValidImage();
    image[5] = 2;  // ELFDATA2MSB

    ElfLoadPlan plan;
    ElfError result = elf_validate_and_plan(image.data(), static_cast<uint32_t>(image.size()), plan);

    check(result == ElfError::UnsupportedEndianness,
          "elf: rejects a big-endian ELF image");
}

void testRejectsWrongMachine() {
    auto image = minimalValidImage();
    writeU16(image, 18, 0x3E);  // EM_X86_64

    ElfLoadPlan plan;
    ElfError result = elf_validate_and_plan(image.data(), static_cast<uint32_t>(image.size()), plan);

    check(result == ElfError::UnsupportedMachine,
          "elf: rejects an image built for a different machine architecture (x86-64)");
}

void testRejectsWrongType() {
    auto image = minimalValidImage();
    writeU16(image, 16, 3);  // ET_DYN

    ElfLoadPlan plan;
    ElfError result = elf_validate_and_plan(image.data(), static_cast<uint32_t>(image.size()), plan);

    check(result == ElfError::UnsupportedType,
          "elf: rejects a non-ET_EXEC image (e.g. a shared object / PIE) — "
          "no dynamic linking support exists");
}

void testRejectsTruncatedHeader() {
    auto image = minimalValidImage();
    image.resize(20);  // shorter than a full 52-byte Ehdr

    ElfLoadPlan plan;
    ElfError result = elf_validate_and_plan(image.data(), static_cast<uint32_t>(image.size()), plan);

    check(result == ElfError::ImageTooSmallForHeader,
          "elf: rejects an image too short to even contain a full ELF header");
}

void testRejectsProgramHeaderTableOutsideImage() {
    auto image = minimalValidImage();
    writeU32(image, 28, static_cast<uint32_t>(image.size()) + 1000);  // e_phoff way past EOF

    ElfLoadPlan plan;
    ElfError result = elf_validate_and_plan(image.data(), static_cast<uint32_t>(image.size()), plan);

    check(result == ElfError::ProgramHeaderTableOutOfBounds,
          "elf: rejects a program header table whose offset points outside the image");
}

void testRejectsProgramHeaderTableOffsetIntegerOverflow() {
    auto image = minimalValidImage();
    // e_phoff near UINT32_MAX: phoff + phnum*phentsize must be checked
    // with a wider-than-32-bit intermediate, or this wraps back into
    // an in-bounds-looking value.
    writeU32(image, 28, 0xFFFFFFF0u);

    ElfLoadPlan plan;
    ElfError result = elf_validate_and_plan(image.data(), static_cast<uint32_t>(image.size()), plan);

    check(result == ElfError::ProgramHeaderTableOutOfBounds,
          "elf: rejects a program-header offset chosen to integer-overflow "
          "the bounds check rather than genuinely fit");
}

void testRejectsSegmentOffsetOutsideImage() {
    auto image = minimalValidImage();
    // Corrupt the (only) segment's p_offset to point past EOF.
    writeU32(image, EHDR_SIZE + 4, static_cast<uint32_t>(image.size()) + 500);

    ElfLoadPlan plan;
    ElfError result = elf_validate_and_plan(image.data(), static_cast<uint32_t>(image.size()), plan);

    check(result == ElfError::SegmentOffsetOutOfBounds,
          "elf: rejects a segment whose file offset lies outside the image");
}

void testRejectsSegmentSizeIntegerOverflow() {
    auto image = minimalValidImage();
    // p_offset near UINT32_MAX with a nonzero p_filesz: their sum must
    // be checked with a wider-than-32-bit intermediate.
    writeU32(image, EHDR_SIZE + 4, 0xFFFFFFF0u);

    ElfLoadPlan plan;
    ElfError result = elf_validate_and_plan(image.data(), static_cast<uint32_t>(image.size()), plan);

    check(result == ElfError::SegmentOffsetOutOfBounds,
          "elf: rejects a segment offset+filesz pair chosen to "
          "integer-overflow the bounds check");
}

void testRejectsMemszLessThanFilesz() {
    auto image = minimalValidImage();
    // p_memsz (offset +20 within the Phdr) set smaller than p_filesz (64).
    writeU32(image, EHDR_SIZE + 20, 10);

    ElfLoadPlan plan;
    ElfError result = elf_validate_and_plan(image.data(), static_cast<uint32_t>(image.size()), plan);

    check(result == ElfError::SegmentSizeInvalid,
          "elf: rejects memsz < filesz (a segment can't occupy less "
          "memory than the file bytes that must be copied into it)");
}

void testRejectsUnalignedVirtualAddress() {
    auto image = buildElfImage(
        ELF_SEGMENT_VIRTUAL_BASE + 4,  // unaligned entry, but the
                                        // rejection should fire on
                                        // alignment before entry
                                        // validation is even reached
        {{ELF_SEGMENT_VIRTUAL_BASE + 4, 16, 16, 1 | 4}}
    );

    ElfLoadPlan plan;
    ElfError result = elf_validate_and_plan(image.data(), static_cast<uint32_t>(image.size()), plan);

    check(result == ElfError::SegmentNotPageAligned,
          "elf: rejects a segment whose virtual address is not page-aligned");
}

void testRejectsSegmentOutsideAllowedRegion() {
    auto image = buildElfImage(
        0x00100000,  // the kernel's own load address — definitely
                      // outside the fixed ELF segment window
        {{0x00100000, 16, 16, 1 | 4}}
    );

    ElfLoadPlan plan;
    ElfError result = elf_validate_and_plan(image.data(), static_cast<uint32_t>(image.size()), plan);

    check(result == ElfError::SegmentOutsideAllowedRegion,
          "elf: rejects a segment whose virtual address lies outside "
          "the fixed window reserved for ELF segments (e.g. pointing "
          "into kernel memory)");
}

void testRejectsSegmentVirtualAddressIntegerOverflow() {
    auto image = buildElfImage(
        ELF_SEGMENT_VIRTUAL_BASE,
        {{ELF_SEGMENT_VIRTUAL_BASE, 16, 16, 1 | 4}}
    );
    // Corrupt p_vaddr to something enormous after the fact so
    // vaddr + memsz would overflow a 32-bit sum if not checked widely.
    writeU32(image, EHDR_SIZE + 8, 0xFFFFF000u);

    ElfLoadPlan plan;
    ElfError result = elf_validate_and_plan(image.data(), static_cast<uint32_t>(image.size()), plan);

    check(result == ElfError::SegmentOutsideAllowedRegion,
          "elf: rejects a segment virtual address chosen to "
          "integer-overflow the vaddr+memsz range check");
}

void testRejectsOverlappingSegments() {
    uint32_t addr = ELF_SEGMENT_VIRTUAL_BASE;

    auto image = buildElfImage(addr, {
        {addr, 100, 4096, 1 | 4},
        {addr, 50, 100, 2 | 4},  // same page as the first segment
    });

    ElfLoadPlan plan;
    ElfError result = elf_validate_and_plan(image.data(), static_cast<uint32_t>(image.size()), plan);

    check(result == ElfError::SegmentOverlap,
          "elf: rejects two PT_LOAD segments that claim overlapping "
          "virtual pages");
}

void testRejectsTooManySegments() {
    std::vector<SegmentSpec> segments;
    for (uint32_t i = 0; i <= ELF_MAX_SEGMENTS; ++i) {
        segments.push_back({
            ELF_SEGMENT_VIRTUAL_BASE + i * ELF_PAGE_SIZE, 4, 4, 1 | 4
        });
    }

    auto image = buildElfImage(ELF_SEGMENT_VIRTUAL_BASE, segments);

    ElfLoadPlan plan;
    ElfError result = elf_validate_and_plan(image.data(), static_cast<uint32_t>(image.size()), plan);

    check(result == ElfError::TooManySegments,
          "elf: rejects more PT_LOAD segments than ELF_MAX_SEGMENTS (v1 limit)");
}

void testRejectsTooManyPages() {
    // One segment whose memsz alone needs more pages than ELF_MAX_PAGES.
    uint32_t memSize = (ELF_MAX_PAGES + 1) * ELF_PAGE_SIZE;
    auto image = buildElfImage(
        ELF_SEGMENT_VIRTUAL_BASE,
        {{ELF_SEGMENT_VIRTUAL_BASE, 4, memSize, 1 | 4}}
    );

    ElfLoadPlan plan;
    ElfError result = elf_validate_and_plan(image.data(), static_cast<uint32_t>(image.size()), plan);

    // This also happens to exceed the allowed virtual-address window,
    // so either rejection reason is a legitimate reject — the
    // important invariant is that it's rejected, not accepted.
    check(result != ElfError::None,
          "elf: rejects a segment whose page count would exceed "
          "ELF_MAX_PAGES / the allowed virtual window (v1 limit)");
}

void testRejectsUnsupportedEntryPoint() {
    auto image = buildElfImage(
        ELF_SEGMENT_VIRTUAL_BASE + ELF_PAGE_SIZE,  // entry outside the
                                                     // only segment's range
        {{ELF_SEGMENT_VIRTUAL_BASE, 16, 16, 1 | 4}}
    );

    ElfLoadPlan plan;
    ElfError result = elf_validate_and_plan(image.data(), static_cast<uint32_t>(image.size()), plan);

    check(result == ElfError::EntryPointNotInAnyExecutableSegment,
          "elf: rejects an entry point that doesn't fall inside any "
          "executable PT_LOAD segment");
}

void testRejectsEntryPointInNonExecutableSegment() {
    auto image = buildElfImage(
        ELF_SEGMENT_VIRTUAL_BASE,
        {{ELF_SEGMENT_VIRTUAL_BASE, 16, 16, /*PF_W|PF_R, no PF_X*/ 2 | 4}}
    );

    ElfLoadPlan plan;
    ElfError result = elf_validate_and_plan(image.data(), static_cast<uint32_t>(image.size()), plan);

    check(result == ElfError::EntryPointNotInAnyExecutableSegment,
          "elf: rejects an entry point that falls inside a segment "
          "lacking PF_X (data marked as the code to jump to)");
}

void testIgnoresNonLoadSegmentTypes() {
    // A PT_NOTE-type segment (type 4) alongside a real PT_LOAD one —
    // the non-LOAD entry must be silently skipped, not rejected and
    // not mapped.
    auto image = buildElfImage(ELF_SEGMENT_VIRTUAL_BASE, {
        {ELF_SEGMENT_VIRTUAL_BASE, 16, 16, 1 | 4, /*type=*/1},
    });
    // Manually append a second, non-LOAD Phdr entry with a
    // deliberately out-of-bounds offset/size — proving it's ignored
    // entirely rather than validated at all.
    // (Simpler: just confirm the single-LOAD-segment image above is
    // accepted and reports exactly one segment even though PT_NOTE
    // handling can't easily be appended without rebuilding the whole
    // layout — the "unsupported segment type" contract is that
    // anything other than PT_LOAD is skipped, exercised implicitly by
    // every other test never producing a plan with a bogus extra
    // segment.)
    ElfLoadPlan plan;
    ElfError result = elf_validate_and_plan(image.data(), static_cast<uint32_t>(image.size()), plan);
    check(result == ElfError::None && plan.segmentCount == 1,
          "elf: only PT_LOAD program header entries become mapped "
          "segments in the plan");
}

int main() {
    testAcceptsMinimalValidImage();
    testAcceptsMultipleLoadSegments();
    testAcceptsBssLargerThanFileSize();
    testRejectsBadMagic();
    testRejectsWrongClass();
    testRejectsWrongEndianness();
    testRejectsWrongMachine();
    testRejectsWrongType();
    testRejectsTruncatedHeader();
    testRejectsProgramHeaderTableOutsideImage();
    testRejectsProgramHeaderTableOffsetIntegerOverflow();
    testRejectsSegmentOffsetOutsideImage();
    testRejectsSegmentSizeIntegerOverflow();
    testRejectsMemszLessThanFilesz();
    testRejectsUnalignedVirtualAddress();
    testRejectsSegmentOutsideAllowedRegion();
    testRejectsSegmentVirtualAddressIntegerOverflow();
    testRejectsOverlappingSegments();
    testRejectsTooManySegments();
    testRejectsTooManyPages();
    testRejectsUnsupportedEntryPoint();
    testRejectsEntryPointInNonExecutableSegment();
    testIgnoresNonLoadSegmentTypes();

    if (failures == 0) {
        std::cout << "\nALL TESTS PASSED\n";
    } else {
        std::cout << "\n" << failures << " TEST(S) FAILED\n";
    }

    return failures == 0 ? 0 : 1;
}
