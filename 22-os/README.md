# Techuilaguy OS

A from-first-principles operating-system prototype inside THE OG TECHUILAGUY.

**Status: PROTOTYPE.** Boots under QEMU and passes automated tests that
assert on real serial console output, real per-process address-space
isolation, real ring-3 privilege enforcement, real ELF32/i386 loading,
and real injected PS/2 input — not just that the kernel prints a
banner. Not a production OS: no storage or network drivers, no
filesystem, no `exec()`/process replacement, no dynamic linking.

## Implemented and tested

- x86 32-bit protected-mode boot via a Multiboot-compliant header, verified
  with `grub-file --is-x86-multiboot` and by actually booting in QEMU
- kernel entry, serial console
- physical frame allocator foundation
- a real IDT with CPU exception handlers (vectors 0-31)
- the PIC, remapped, with hardware IRQ0 (timer) and IRQ1 (keyboard) routed
  through the same trap-frame save/restore path as CPU exceptions
- the PIT programmed to 100 Hz
- **a real preemptive scheduler**: round-robin task selection, a
  Ready/Running/Blocked/Dead lifecycle keyed by a permanently unique pid
  (not by task-table slot — a dead task's slot can be reused by an
  unrelated new task without any risk of a stale pid handle reaching it),
  and context switching implemented by having the timer/yield interrupt
  path resume a different task's previously saved trap frame
- hardware IRQ handling (PIC EOI, raw tick counting) kept architecturally
  separate from scheduling policy (round-robin choice, lifecycle
  transitions): both the timer and a software `int $0x81` yield vector
  drive the same policy function
- syscall ABI foundation, VFS foundation, capability/security foundation
- **a real PS/2 keyboard driver**: IRQ1 reads a scancode from port 0x60
  and translates it (US QWERTY, unshifted, set-1 make codes) to ASCII on
  the serial console. The translation table itself
  (`drivers/keyboard_translation.cpp`) is pure logic with no hardware
  I/O, unit-tested directly with a hosted compiler
  (`tests/keyboard_translation_test.sh`); the driver's hardware-facing
  half is verified by actually booting the kernel and injecting real
  scancodes through QEMU's monitor (`tests/keyboard_test.sh`), not just
  by testing the table in isolation
- **a real GDT + TSS + ring-3 userspace**: this kernel previously had no
  GDT at all — `isr_common` and the scheduler both hardcoded `0x18` as
  "the kernel data selector", an unverified assumption inherited from
  whatever GRUB's own default GDT happened to leave in place. `gdt/`
  now builds and installs the kernel's own GDT (null, kernel
  code/data, user code/data, TSS) and reloads every segment register,
  including CS via a far jump. The TSS's `ss0:esp0` is kept in sync
  with whichever task is about to run (`tss_set_kernel_stack`, called
  from the scheduler's `switchTo`) so a ring-3 task's stack is always
  correct the instant it takes any interrupt, timer tick, or syscall.
  `scheduler_create_user_task` builds a task from a real flat
  machine-code image (see `userland/`) copied into an allocated
  physical page, running at CPL 3 with its own separate user-mode stack
  page (see the paging entry below for how that page's virtual address
  became genuinely private to the task once paging gained per-process
  address spaces). `int $0x80` (a dedicated
  DPL-3 IDT gate — every other gate is DPL 0, so only this one can be
  invoked directly from ring 3) reaches a real syscall dispatcher
  (`syscalls/syscalls.cpp`: SYS_WRITE, SYS_YIELD, SYS_EXIT implemented;
  SYS_OPEN/READ/CLOSE honestly report "not implemented" rather than
  faking success). A CPU exception raised by ring-3 code (checked via
  the interrupted CS's RPL) terminates only that task — the kernel and
  every other task keep running — rather than halting the system the
  way a kernel-mode exception still does.
  `tests/boot_test.sh` verifies all of this against real boot
  behavior: a genuine ring-3 program (`userland/hello.S`) reaches the
  kernel through `SYS_WRITE` and survives an unrecognized syscall
  number (99) without crashing; a second one (`userland/evil.S`)
  executes `cli` directly from CPL 3, which must fault with #GP (a
  privileged instruction requires CPL <= IOPL, which ring 3 never
  satisfies here) — the test confirms that program's code *after* the
  fault never ran, that it printed `[FAULT] ... killed by exception 13`
  rather than halting the kernel, and that the rest of the system
  (timer ticks, the other scheduler tasks) kept running afterward.
- **real paging with genuine per-process address spaces**: `paging/`
  builds a shared, 32-bit (non-PAE) identity-mapped kernel region (16
  MiB, matching the physical allocator's range, supervisor-only) and
  enables paging (`CR0.PG`). On top of that shared base, every user
  task created by `scheduler_create_user_task` gets its **own page
  directory** (`paging_create_address_space`) and its **own private
  page table** for a fixed virtual region (`USER_CODE_VADDR` /
  `USER_STACK_PAGE_VADDR`, identical addresses for every task) mapping
  that task's own physical code/stack pages there — `paging_map_user_page`
  allocates a dedicated table for this the first time it's needed, never
  the shared kernel one. The scheduler's `switchTo` loads the running
  task's own address space (`paging_switch_address_space`, i.e. `CR3`)
  on every context switch, alongside the existing TSS `esp0` update. No
  other task's directory has any translation for this task's private
  virtual region at all — this is what makes one process structurally
  unable to reach another's memory, not merely permission-denied from
  it, and is a strictly stronger guarantee than a single shared
  directory with per-page permission bits (the design this replaced):
  without separate address spaces, a flat 0..4 GiB segment limit gave
  ring-3 code full read/write access to *all* physical memory as long
  as it avoided privileged instructions, and even with a shared
  directory's permission bits, any process could in principle reach
  any other's granted pages by guessing their physical addresses.
  Terminating a slot's previous occupant (`reclaimSlot`, run before
  reusing a task-table slot) tears down its private address space and
  frees its pages before the slot is handed to a new task, and a
  recycled stack page is explicitly zeroed so no process ever observes
  another's leftover stack contents.
  `tests/boot_test.sh` verifies this against real boot behavior with
  three distinct ring-3 programs: `userland/kernel_peek.S` reads the
  kernel's own load address (1 MiB, in the *shared* region, never
  granted to user code) and must fault with #PF; `userland/neighbor_peek.S`
  reads one page past its own granted 2-page private region — a virtual
  address that is never mapped in *any* task's own address space — and
  must also fault with #PF; both are confirmed to be killed in
  isolation, with their own code after the fault never executing, and
  the kernel and every other task (including the ongoing scheduler
  lifecycle test and the timer) confirmed to keep running afterward.

- **a real kernel heap**: `heap/` implements `kmalloc`/`kfree` on top
  of the physical page allocator — a first-fit, address-ordered
  free-list allocator with real block splitting (a large free block is
  cut down to the requested size, leaving the remainder as its own
  free block, rather than handing out the whole thing) and coalescing
  (freeing a block merges it with an immediately adjacent free
  neighbor in either direction). Each heap *segment* is exactly one
  physical page obtained via `memory_alloc_page()` — a segment's blocks
  only ever coalesce with each other, never across segments, since two
  pages from the physical allocator aren't guaranteed to be physically
  contiguous. A single allocation therefore cannot exceed one page
  minus header overhead; `kmalloc` returns `null` rather than a bogus
  pointer if that limit is hit or the physical allocator itself is
  exhausted, and a double-free is a safe no-op (a best-effort guard,
  not real corruption detection — there is no canary or pointer
  validation, exactly like a hosted C `free()`).
  The allocator logic itself (splitting, coalescing, first-fit search)
  is hardware-independent and unit-tested with a hosted compiler
  (`tests/heap_test.cpp`, `tests/heap_test.sh`) against a fake page
  allocator backed by real host memory — the real
  `memory_alloc_page()` hands back raw physical addresses only
  dereferenceable inside the kernel's own identity-mapped address
  space, so a hosted test process needs its own stand-in rather than
  linking `memory/memory.cpp` directly, the same reasoning
  `tests/keyboard_translation_test.sh` already applies to the keyboard
  driver's scancode table. `tests/boot_test.sh` additionally runs a
  real boot-time self-test against the actual physical allocator and
  identity-mapped kernel address space: allocate two blocks, write
  through both, confirm neither corrupts the other, free one, and
  confirm reallocating the same size reuses the exact freed block.
- **a real ELF32/i386 loader**: `elf/` parses and validates a genuine
  ELF header and program header table — magic, class, endianness,
  version, type (`ET_EXEC` only), machine (`EM_386` only), and every
  `PT_LOAD` segment's offset/size/address/alignment, with every check
  done via wide (64-bit) intermediate arithmetic so a crafted
  offset+size pair can't integer-overflow past a bounds check — before
  `scheduler_create_elf_user_task` (`scheduler/scheduler.cpp`) touches
  any kernel resource. Once validated, each segment gets its own
  physical pages (zeroed first, both to realize BSS's zero-init
  requirement and to prevent a recycled page leaking a previous
  process's contents) and its own permissions — `paging_map_user_page`
  gained a `writable` parameter driven by each segment's `PF_W` flag,
  so a code segment is genuinely read+execute-only and a data segment
  is genuinely writable, not just labeled that way. (This is 32-bit,
  non-PAE paging: there is no NX bit at all, so executability itself
  can't be hardware-enforced — an honest architectural limit, not an
  oversight; see docs/ADR/0004-elf-loader.md.) A failure at any point —
  a malformed image, or a physical-page allocation running out
  partway through — unwinds every page and address space already
  allocated for that attempt rather than leaving a half-built process
  or leaking pages.
  The validation logic itself is hardware-independent and unit-tested
  with a hosted compiler against 22 synthetic ELF images
  (`tests/elf_test.cpp`/`elf_test.sh`) covering every rejection case:
  bad magic, wrong class/endianness/machine/type, a truncated header,
  a program-header table or segment offset outside the image (plain
  and integer-overflow-inducing), `memsz < filesz`, unaligned or
  out-of-window virtual addresses, overlapping segments, exceeding the
  v1 segment/page-count limits, and an entry point outside any
  executable segment. `tests/boot_test.sh` additionally boots two real
  ELF binaries (`userland/elf_hello.S`, `elf_write_to_code.S` — still
  hand-written assembly, linked with an explicit `PHDRS`-based script
  into genuine multi-segment ELF files, never flattened): one reads,
  writes, and reads back its own `.data` segment to prove it's real,
  distinct, writable memory; the other deliberately writes into its
  own read-execute-only code segment and is confirmed to fault with
  #PF and be killed in isolation — the concrete negative proof that
  segment permissions are enforced by the CPU, not merely recorded by
  the loader.

## Not yet implemented

- `exec()`/process replacement, fork, IPC (the ELF loader creates a
  *new* task; there is no syscall for an existing task to load and
  replace itself with an ELF image)
- dynamic linking, relocations, PIE/`ET_DYN` ELF images, or a
  general-purpose ELF loader (v1 is ET_EXEC/EM_386 only, with fixed
  small segment/page-count limits — see docs/ADR/0004-elf-loader.md)
- shifted/uppercase keyboard input, modifier keys, non-US layouts
- drivers beyond the timer, PIC, and keyboard (no storage, no
  networking)
- a filesystem
- dynamic memory allocation wired to the scheduler (task stacks are
  static, fixed-size, and fixed in number — see `MAX_TASKS` in
  `scheduler/scheduler.cpp`)
- stack overflow detection for task stacks
- more than 8 tasks at a time

## Building and testing

```sh
make            # build techuilaguy-os
make iso        # build a bootable ISO (requires grub-mkrescue, xorriso)
make run        # boot it in QEMU with -serial stdio
bash tests/boot_test.sh                  # automated boot + scheduler lifecycle test
bash tests/keyboard_test.sh              # boots + injects real scancodes via QEMU's monitor
bash tests/keyboard_translation_test.sh  # hosted unit test, no boot cycle needed
bash tests/heap_test.sh                  # hosted unit test of the kmalloc/kfree allocator logic
bash tests/elf_test.sh                   # hosted unit test of ELF header/segment validation
```
