# ADR 0004: Techuilaguy OS's ELF loader (v1)

**Status:** Accepted (describes the implemented state as of this
writing). This is explicitly a first version — see "What this is not"
below for exactly which capabilities a general-purpose ELF loader has
that this one does not.

## Context

Every ring-3 userland program before this ADR was a hand-written flat
machine-code image: no ELF header, no program headers, no segment
permissions — `scheduler_create_user_task` simply copied a raw byte
blob into one physical page at offset 0 and jumped to it. That model
cannot express more than one segment, cannot express differing
permissions between code and data, and cannot represent BSS (a region
that must be zero-initialized but occupies no space in the file) at
all. This ADR records the real ELF32/i386 loader that closes those
gaps, and exactly how far it goes.

## Threat model and trust boundary

An ELF image handed to `scheduler_create_elf_user_task` is untrusted
input — the same trust level a real OS gives a file loaded from a
freely-writable filesystem (an OS security boundary, in the terms this
project's own instructions use). Every field read from the image
(offsets, sizes, addresses, counts) is validated *before* it is used
for any further arithmetic, memory access, or allocation decision —
never trusted first and checked second. The specific properties this
loader guarantees for a malformed or hostile image:

- **No out-of-bounds read of the image buffer.** Every offset+size
  pair (`e_phoff`+`phnum*phentsize`, each segment's
  `p_offset`+`p_filesz`) is checked against the buffer's actual length
  using 64-bit intermediate arithmetic specifically so a large,
  attacker-chosen offset/size pair cannot wrap a 32-bit sum back into
  a value that looks in-bounds.
- **No out-of-bounds write of kernel or another process's memory.**
  Every segment's virtual-address range is checked against a fixed
  window (`ELF_SEGMENT_VIRTUAL_BASE`..+`ELF_MAX_PAGES` pages) that sits
  entirely outside the shared, supervisor-only kernel identity map —
  the same architectural guarantee that already makes the shared
  kernel region unreachable to any user task's private mappings (see
  ADR-adjacent comments in `paging/paging.hpp`).
- **No jump to attacker-controlled garbage disguised as an entry
  point.** `e_entry` must land inside a segment that is both mapped and
  marked executable (`PF_X`) in the *validated plan* — not merely
  "some value the file claims," and not merely "inside the image
  file," which would say nothing about where it ends up mapped.
- **No partially-constructed process left behind on failure.**
  `scheduler_create_elf_user_task` allocates a fresh address space and
  physical pages incrementally while loading; any failure at any point
  (a validation error caught before anything is allocated, or a
  physical-page-allocation failure partway through mapping) unwinds
  every physical page and the address space allocated so far before
  returning failure. The task table slot being considered for reuse is
  never touched (`reclaimSlot`) until every resource the new task needs
  already exists and is fully mapped.
- **The kernel itself never crashes or corrupts its own memory in
  response to a malformed image.** Every rejection path is a normal
  function return (`ElfError` != `None`, or `-1` from
  `scheduler_create_elf_user_task`) — there is no code path where a
  bad field value is dereferenced, indexed, or used as a size before
  being checked.

What this threat model deliberately does *not* cover: the loaded
program's own behavior once it starts running. A syntactically valid
ELF image whose *code* is malicious is expected to run exactly as
written, at ring 3, under the same per-process address-space isolation
every other ring-3 task already gets (ADR "per-process address spaces"
work — see the OS's main README) — this loader's job is to reject
malformed *structure*, not to sandbox arbitrary logic.

## Decision

### What is validated (`elf/elf.hpp`/`elf.cpp`)

`elf_validate_and_plan()` is pure arithmetic over a raw byte buffer —
no allocation, no paging, no kernel dependency — deliberately kept
hardware/OS-independent so it can be exhaustively unit-tested with a
hosted compiler (`tests/elf_test.cpp`/`elf_test.sh`), the same
reasoning `tests/heap_test.cpp` and
`tests/keyboard_translation_test.cpp` already apply to their own
pure-logic pieces. It checks, in order: the buffer is large enough to
even hold an ELF header; the magic bytes; `EI_CLASS` (must be
`ELFCLASS32`); `EI_DATA` (must be `ELFDATA2LSB`); `EI_VERSION`;
`e_type` (must be `ET_EXEC`); `e_machine` (must be `EM_386`); the
program header table's offset+size fits the buffer; the segment count
doesn't exceed `ELF_MAX_SEGMENTS`; and then, per `PT_LOAD` segment
(any other segment type is silently skipped — it carries no
instructions or data this loader needs to act on): `p_memsz >=
p_filesz`; `p_offset+p_filesz` fits the buffer; `p_vaddr` is
page-aligned; `p_vaddr`..`p_vaddr+p_memsz` fits entirely inside the
fixed ELF segment window; the segment's pages don't overlap any
previously-accepted segment's pages; and the running total of pages
needed across all segments doesn't exceed `ELF_MAX_PAGES`. Finally,
`e_entry` must fall inside some accepted segment that has `PF_X` set.

### What is loaded (`scheduler_create_elf_user_task`,
`scheduler/scheduler.cpp`)

Only once validation succeeds does any resource get allocated: a fresh
per-process address space (reusing the existing
`paging_create_address_space`/`paging_destroy_address_space`
machinery — an ELF-loaded task is isolated from every other task and
from the kernel exactly the same way a flat-binary task already is),
then, per segment, one physical page per page of `memSize` — each page
is zeroed first (satisfying both BSS's zero-initialization requirement
and this project's existing anti-leak discipline against a recycled
physical page's previous contents), then whatever file bytes belong to
that page are copied in before the page is ever mapped user-accessible,
then mapped with `paging_map_user_page`'s new `writable` parameter set
from the segment's `PF_W` flag. A dedicated stack page (zeroed, like
every other user task's stack) is mapped at a fixed address just past
the ELF segment window (`ELF_STACK_PAGE_VADDR`/`_TOP`). The initial
trap frame is built with the *real* ELF entry point and stack top,
reusing the identical `prepareUserInitialFrame` the flat-binary path
already uses.

### The NX limit

This is 32-bit, non-PAE paging. There is no execute-disable (NX) bit
available at all — NX requires PAE or long mode. `paging_map_user_page`
can therefore only distinguish a segment's writability (the `W` page
table bit), never its executability at the hardware level. A `PF_X`
flag with no `PF_W` still produces a genuinely read+execute-only page
(demonstrated by `userland/elf_write_to_code.S`'s negative test — a
write into such a segment faults with #PF), but a data segment marked
`PF_W` without `PF_X` is still technically executable by the CPU if
control ever jumped there; nothing in this architecture can prevent
that. This is an honest architectural limit, not an oversight, and is
recorded explicitly rather than implied away by only testing the case
that happens to work.

### Toolchain

Real ELF userland programs (`userland/elf_hello.S`,
`elf_write_to_code.S`) are still hand-written x86 assembly, assembled
and linked with the exact same freestanding GNU binutils (`as`, `ld`)
every other part of this OS already uses — no new toolchain dependency
was introduced. What's different from the existing flat-binary
programs is the *linker script*: `userland/elf_user.ld` and
`elf_write_to_code.ld` use explicit `PHDRS` blocks to produce real,
distinct `PT_LOAD` segments with real permission flags
(`FLAGS(5)` = R+X, `FLAGS(6)` = R+W), and the Makefile's
`ELF_USERLAND_EMBED_OBJECTS` rule runs `objcopy -I binary` directly on
the *linked ELF file* — never flattening it first, unlike
`USERLAND_EMBED_OBJECTS` — so the bytes the kernel parses at boot are
a genuine ELF image with a real header and program header table, not
the flat-binary path's raw instruction stream.

## What this is not

- **Not a general-purpose ELF loader.** ELF32/i386, `ET_EXEC` only —
  no `ET_DYN` (shared objects, PIE), no dynamic linking, no
  relocations processed at all, no interpreter (`PT_INTERP`), no
  section-header table read (only program headers). A real Linux
  binary produced by an unmodified toolchain (typically PIE by
  default on modern distributions) will be rejected by the `ET_EXEC`
  check, not silently misloaded.
- **Fixed, small v1 limits.** At most `ELF_MAX_SEGMENTS` (4) `PT_LOAD`
  segments, at most `ELF_MAX_PAGES` (8) total physical pages across all
  of them, and every segment must fit inside one fixed 32-page virtual
  window. These are explicit, checked limits — an image exceeding them
  is rejected outright, not truncated or partially loaded.
- **No NX enforcement**, for the architectural reason above.
- **No `PT_GNU_STACK`/`PT_GNU_RELRO`/`PT_NOTE` handling** beyond
  "safely ignored" — these segment types carry no content this loader
  needs, so they're skipped, not rejected and not acted on.
- **No exec() / process replacement.** This loader creates a *new*
  task; there is no syscall yet for an existing running task to load
  and replace itself with an ELF image (that would be `execve`-style
  behavior — a real Layer-3 OS requirement still `PLANNED`, not this
  ADR's scope).

## Tested invariants

Positive (`tests/elf_test.cpp`, hosted): a minimal valid image, an
image with multiple non-overlapping `PT_LOAD` segments with distinct
permissions, and a BSS-bearing segment (`memsz > filesz`) are all
accepted with the correct plan recorded.

Negative (`tests/elf_test.cpp`, hosted — one test per case, each
mutating exactly one field of an otherwise-valid image): bad magic,
wrong class, wrong endianness, wrong machine, wrong type (`ET_DYN`),
a header truncated below `EHDR_SIZE`, a program-header table pointing
outside the image (both a plain out-of-bounds offset and one chosen to
integer-overflow the check), a segment offset outside the image (both
plain and overflow-inducing), `memsz < filesz`, a non-page-aligned
virtual address, a segment outside the allowed virtual window, two
overlapping segments, more segments than `ELF_MAX_SEGMENTS`, more pages
than `ELF_MAX_PAGES`, an entry point outside any segment, an entry
point inside a non-executable segment, and confirmation that a
non-`PT_LOAD` segment type is silently skipped rather than rejected or
mapped.

Integration (`tests/boot_test.sh`, real QEMU boot): the kernel's own
loader successfully validates and loads a real, two-segment ELF image
(`elf_hello`) at boot; that task reads its own `.data` segment's
initial value, writes to it, and reads the write back — proving the
data segment is real, distinct, independently-mapped memory, not an
extension of `.text`. A second real ELF image
(`elf_write_to_code`) reaches the kernel via a syscall from its own
loaded code segment, then attempts to write into that segment; this
faults with #PF and the task is killed in isolation (verified via the
same fault-isolation mechanism already proven for `evil.S`/
`kernel_peek.S`/`neighbor_peek.S`), and the task's own code after that
write never runs — the concrete, negative proof that a `PT_LOAD`
segment's `PF_W` flag genuinely controls hardware write permission,
not just loader bookkeeping.

Regression: every pre-existing boot, keyboard, heap, and compiler test
was rerun from a clean build and remains green — nothing about this
change altered the flat-binary task path, the scheduler's existing
lifecycle tests, or paging's shared kernel region.

## Consequences

Every claim about OGLang/Techuilaguy OS elsewhere in this repository
must describe ELF support using the scope recorded here: a real,
tested ELF32/i386 `ET_EXEC` loader with fixed v1 size limits, no
dynamic linking, no NX enforcement, no `exec()` — not a general-purpose
ELF loader capable of running an arbitrary Linux binary. This ADR is
the single source of truth for that distinction until a future ADR
supersedes it.
