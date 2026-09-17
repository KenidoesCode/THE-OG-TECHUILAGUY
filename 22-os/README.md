# Techuilaguy OS

A from-first-principles operating-system prototype inside THE OG TECHUILAGUY.

**Status: PROTOTYPE.** Boots under QEMU and passes an automated boot test
(`tests/boot_test.sh`) that asserts on real serial console output,
including a live preemptive-scheduling lifecycle scenario. Not a production
OS: single privilege ring, no userspace, no drivers beyond the timer/PIC,
no filesystem, no networking.

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

## Not yet implemented

- userspace / ring 3, GDT/TSS-based privilege separation
- drivers beyond the timer and PIC (no real keyboard input, no storage,
  no networking)
- a filesystem
- dynamic memory allocation wired to the scheduler (task stacks are
  static, fixed-size, and fixed in number — see `MAX_TASKS` in
  `scheduler/scheduler.cpp`)
- stack overflow detection for task stacks
- more than 4 tasks at a time

## Building and testing

```sh
make            # build techuilaguy-os
make iso        # build a bootable ISO (requires grub-mkrescue, xorriso)
make run        # boot it in QEMU with -serial stdio
bash tests/boot_test.sh   # automated boot + scheduler lifecycle test
```
