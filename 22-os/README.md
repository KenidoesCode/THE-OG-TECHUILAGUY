# Techuilaguy OS

A from-first-principles operating-system prototype inside THE OG TECHUILAGUY.

**Status: PROTOTYPE.** Boots under QEMU and passes automated tests that
assert on real serial console output and, for the keyboard driver, on
real injected PS/2 input — not just that the kernel prints a banner. Not
a production OS: single privilege ring, no userspace, no storage or
network drivers, no filesystem, no networking.

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

## Not yet implemented

- userspace / ring 3, GDT/TSS-based privilege separation
- shifted/uppercase keyboard input, modifier keys, non-US layouts
- drivers beyond the timer, PIC, and keyboard (no storage, no
  networking)
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
bash tests/boot_test.sh                  # automated boot + scheduler lifecycle test
bash tests/keyboard_test.sh              # boots + injects real scancodes via QEMU's monitor
bash tests/keyboard_translation_test.sh  # hosted unit test, no boot cycle needed
```
