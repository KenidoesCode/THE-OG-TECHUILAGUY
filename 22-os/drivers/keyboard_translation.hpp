#pragma once

#include <stdint.h>

// PS/2 set-1 scancode -> ASCII translation, US QWERTY, unshifted only.
//
// Deliberately has zero hardware I/O and zero freestanding-only
// dependencies, so it can be compiled and unit-tested with a normal
// hosted compiler (tests/keyboard_translation_test.cpp) without
// touching the freestanding kernel toolchain, an emulator, or a boot
// cycle — unlike keyboard.cpp, which needs real (or emulated) PS/2
// hardware to observe end-to-end.
//
// Returns the ASCII character for a make-code (key press), or 0 for a
// break-code (key release, scancode with the high bit set) or any
// scancode with no ASCII representation (Shift, Ctrl, arrow keys,
// function keys, etc. — not modeled yet).
uint8_t scancode_to_ascii(uint8_t scancode);
