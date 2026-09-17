# Techuilaguy OS

A from-first-principles operating-system prototype inside THE OG TECHUILAGUY.

**Status: PROTOTYPE.** Boots under QEMU and passes automated tests that
assert on real serial console output, real paging-enforced memory
isolation, real ring-3 privilege enforcement, and real injected PS/2
input — not just that the kernel prints a banner. Not a production OS:
one shared identity-mapped address space (no per-process address
spaces yet), no storage or network drivers, no filesystem, no
networking.

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
  physical page (identity-mapped; no paging yet), running at CPL 3 with
  its own separate user-mode stack page. `int $0x80` (a dedicated
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
- **real paging-based memory isolation**: `paging/` builds a 32-bit
  (non-PAE) identity-mapped page directory/table set (16 MiB, matching
  the physical allocator's range) and enables paging (`CR0.PG`).
  Every page starts supervisor-only; `scheduler_create_user_task`
  grants user access to exactly the two pages (code, stack) it
  allocates for that task via `paging_set_user_accessible`, and
  revokes it (`paging_set_supervisor_only`) before a freed page returns
  to the general allocator, so a stale user-accessible mapping can
  never persist onto whatever the page is reused for next. This is
  distinct from — and a stronger guarantee than — the instruction-level
  privilege isolation above: without it, a flat 0..4 GiB segment limit
  gave ring-3 code full read/write access to *all* physical memory,
  including the kernel's own code and data, as long as it avoided
  privileged instructions. `tests/boot_test.sh` verifies this against
  real boot behavior: a third ring-3 program (`userland/kernel_peek.S`)
  directly reads the kernel's own load address (1 MiB), which it was
  never granted access to, and the test confirms that read faults with
  #PF (14), the program is killed exactly like the privileged-
  instruction case, and the kernel and every other task keep running.

## Not yet implemented

- per-process address spaces (paging is currently one identity-mapped
  page directory shared by everything; every task sees the same
  address layout, just with different per-page permissions — not
  separate virtual address spaces)
- a kernel heap (`malloc`-style allocation on top of the physical page
  allocator)
- more than one userland program's worth of syscalls (no exec, no
  fork, no IPC)
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
```
