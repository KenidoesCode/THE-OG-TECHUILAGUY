#include "elf.hpp"

namespace {

// --- Raw ELF32 structures (see the System V ABI / ELF specification).
// Read only via byte-offset + memcpy-style field extraction below,
// never by casting `image` directly to an Elf32_Ehdr* — the buffer's
// alignment isn't guaranteed, and more importantly every field must be
// bounds-checked against imageSize before it's trusted for any further
// arithmetic, which a direct struct overlay would make easy to forget.

constexpr uint32_t EHDR_SIZE = 52;
constexpr uint32_t PHDR_SIZE = 32;

constexpr uint8_t ELFCLASS32 = 1;
constexpr uint8_t ELFDATA2LSB = 1;
constexpr uint8_t EV_CURRENT = 1;

constexpr uint16_t ET_EXEC = 2;
constexpr uint16_t EM_386 = 3;

constexpr uint32_t PT_LOAD = 1;

constexpr uint32_t PF_X = 0x1;
constexpr uint32_t PF_W = 0x2;

uint16_t readU16(const uint8_t* p) {
    return static_cast<uint16_t>(
        static_cast<uint32_t>(p[0]) |
        (static_cast<uint32_t>(p[1]) << 8)
    );
}

uint32_t readU32(const uint8_t* p) {
    return static_cast<uint32_t>(p[0]) |
           (static_cast<uint32_t>(p[1]) << 8) |
           (static_cast<uint32_t>(p[2]) << 16) |
           (static_cast<uint32_t>(p[3]) << 24);
}

uint32_t pageAlignDown(uint32_t address) {
    return address & ~(ELF_PAGE_SIZE - 1);
}

bool isPageAligned(uint32_t address) {
    return (address & (ELF_PAGE_SIZE - 1)) == 0;
}

// True if [aOffset, aOffset+aLen) and [bOffset, bOffset+bLen) (both in
// page units) overlap. Used both for the file-offset bounds check
// (against imageSize) and for the virtual-page overlap check between
// two segments.
bool rangesOverlap(
    uint32_t aStart, uint32_t aEnd,
    uint32_t bStart, uint32_t bEnd
) {
    return aStart < bEnd && bStart < aEnd;
}

}  // namespace

ElfError elf_validate_and_plan(
    const uint8_t* image,
    uint32_t imageSize,
    ElfLoadPlan& outPlan
) {
    if (imageSize < EHDR_SIZE) {
        return ElfError::ImageTooSmallForHeader;
    }

    if (image[0] != 0x7F || image[1] != 'E' ||
        image[2] != 'L' || image[3] != 'F') {
        return ElfError::BadMagic;
    }

    if (image[4] != ELFCLASS32) {
        return ElfError::UnsupportedClass;
    }

    if (image[5] != ELFDATA2LSB) {
        return ElfError::UnsupportedEndianness;
    }

    if (image[6] != EV_CURRENT) {
        return ElfError::UnsupportedVersion;
    }

    uint16_t type = readU16(image + 16);
    if (type != ET_EXEC) {
        return ElfError::UnsupportedType;
    }

    uint16_t machine = readU16(image + 18);
    if (machine != EM_386) {
        return ElfError::UnsupportedMachine;
    }

    uint32_t entry = readU32(image + 24);
    uint32_t phoff = readU32(image + 28);
    uint16_t phentsize = readU16(image + 42);
    uint16_t phnum = readU16(image + 44);

    if (phnum == 0) {
        return ElfError::NoLoadSegments;
    }

    if (phnum > ELF_MAX_SEGMENTS) {
        return ElfError::TooManySegments;
    }

    if (phentsize != PHDR_SIZE) {
        // A nonstandard program-header entry size is outside this
        // loader's supported subset — rather than trusting it and
        // computing a stride that doesn't match the structures below,
        // reject it outright.
        return ElfError::ProgramHeaderTableOutOfBounds;
    }

    // phoff + phnum*phentsize must fit within imageSize, checked with
    // 64-bit intermediates so a large phoff/phnum pair can never wrap
    // a 32-bit sum back into an in-bounds-looking value.
    uint64_t phTableEnd =
        static_cast<uint64_t>(phoff) +
        static_cast<uint64_t>(phnum) * static_cast<uint64_t>(phentsize);

    if (phTableEnd > static_cast<uint64_t>(imageSize)) {
        return ElfError::ProgramHeaderTableOutOfBounds;
    }

    outPlan.entryPoint = entry;
    outPlan.segmentCount = 0;
    outPlan.totalPages = 0;

    for (uint16_t i = 0; i < phnum; ++i) {
        const uint8_t* ph = image + phoff + i * phentsize;

        uint32_t p_type = readU32(ph + 0);

        if (p_type != PT_LOAD) {
            // Any other segment type (PT_NOTE, PT_GNU_STACK, etc.) is
            // simply not something this loader acts on — safe to
            // ignore, not a rejection, since it carries no
            // instructions or data that need to end up mapped.
            continue;
        }

        uint32_t p_offset = readU32(ph + 4);
        uint32_t p_vaddr = readU32(ph + 8);
        uint32_t p_filesz = readU32(ph + 16);
        uint32_t p_memsz = readU32(ph + 20);
        uint32_t p_flags = readU32(ph + 24);

        if (p_memsz < p_filesz) {
            return ElfError::SegmentSizeInvalid;
        }

        // p_offset + p_filesz must fit within imageSize — again via a
        // 64-bit intermediate, since p_offset and p_filesz are both
        // attacker/file-controlled 32-bit values whose sum can
        // otherwise overflow and wrap past the bounds check.
        uint64_t fileEnd =
            static_cast<uint64_t>(p_offset) +
            static_cast<uint64_t>(p_filesz);

        if (fileEnd > static_cast<uint64_t>(imageSize)) {
            return ElfError::SegmentOffsetOutOfBounds;
        }

        if (!isPageAligned(p_vaddr)) {
            return ElfError::SegmentNotPageAligned;
        }

        // p_vaddr + p_memsz must fit within the fixed ELF segment
        // window without overflowing a 32-bit address — checked via a
        // 64-bit intermediate for the same reason as above.
        uint64_t memEnd =
            static_cast<uint64_t>(p_vaddr) +
            static_cast<uint64_t>(p_memsz);

        uint64_t windowStart = ELF_SEGMENT_VIRTUAL_BASE;
        uint64_t windowEnd =
            static_cast<uint64_t>(ELF_SEGMENT_VIRTUAL_BASE) +
            static_cast<uint64_t>(ELF_MAX_PAGES) * ELF_PAGE_SIZE;

        if (static_cast<uint64_t>(p_vaddr) < windowStart ||
            memEnd > windowEnd) {
            return ElfError::SegmentOutsideAllowedRegion;
        }

        if (outPlan.segmentCount >= ELF_MAX_SEGMENTS) {
            return ElfError::TooManySegments;
        }

        uint32_t segStartPage = pageAlignDown(p_vaddr);
        uint32_t segEndPage =
            pageAlignDown(
                static_cast<uint32_t>(memEnd) + ELF_PAGE_SIZE - 1
            );

        for (uint32_t j = 0; j < outPlan.segmentCount; ++j) {
            uint32_t otherStart =
                pageAlignDown(outPlan.segments[j].virtualAddress);
            uint32_t otherEnd = pageAlignDown(
                outPlan.segments[j].virtualAddress +
                outPlan.segments[j].memSize + ELF_PAGE_SIZE - 1
            );

            if (rangesOverlap(
                    segStartPage, segEndPage, otherStart, otherEnd)) {
                return ElfError::SegmentOverlap;
            }
        }

        ElfSegmentPlan& segment = outPlan.segments[outPlan.segmentCount];
        segment.virtualAddress = p_vaddr;
        segment.fileOffset = p_offset;
        segment.fileSize = p_filesz;
        segment.memSize = p_memsz;
        segment.writable = (p_flags & PF_W) != 0;
        segment.executable = (p_flags & PF_X) != 0;

        outPlan.segmentCount++;

        uint32_t pagesForSegment =
            (p_memsz + ELF_PAGE_SIZE - 1) / ELF_PAGE_SIZE;

        if (pagesForSegment == 0) {
            pagesForSegment = 1;  // a zero-memsz LOAD segment is unusual
                                   // but still occupies its own page.
        }

        outPlan.totalPages += pagesForSegment;

        if (outPlan.totalPages > ELF_MAX_PAGES) {
            return ElfError::TooManyPages;
        }
    }

    if (outPlan.segmentCount == 0) {
        return ElfError::NoLoadSegments;
    }

    bool entryIsValid = false;
    for (uint32_t i = 0; i < outPlan.segmentCount; ++i) {
        const ElfSegmentPlan& segment = outPlan.segments[i];

        if (!segment.executable) {
            continue;
        }

        uint64_t segEnd =
            static_cast<uint64_t>(segment.virtualAddress) +
            static_cast<uint64_t>(segment.memSize);

        if (static_cast<uint64_t>(entry) >= segment.virtualAddress &&
            static_cast<uint64_t>(entry) < segEnd) {
            entryIsValid = true;
            break;
        }
    }

    if (!entryIsValid) {
        return ElfError::EntryPointNotInAnyExecutableSegment;
    }

    return ElfError::None;
}
