#pragma once

// PS/2 keyboard driver. Scancode-to-ASCII translation itself lives in
// keyboard_translation.hpp, which has no hardware I/O and is unit-
// tested directly; this header covers only the hardware-facing half
// (unmasking IRQ1, reading the data port, and what happens with each
// translated character), which needs real or emulated PS/2 hardware to
// observe — see tests/keyboard_test.sh, which boots the kernel under
// QEMU and injects real scancodes through the monitor.

void keyboard_init();

// Called from the IRQ1 path (see interrupts.cpp) after acknowledging
// the interrupt. Reads exactly one scancode from the PS/2 data port and
// writes its ASCII translation to the serial console if it has one.
extern "C" void keyboard_on_irq();
