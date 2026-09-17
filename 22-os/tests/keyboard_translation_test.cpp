// Real assertion-based unit test for the keyboard driver's pure
// scancode-to-ASCII translation table. Compiled and run with a normal
// hosted compiler (see keyboard_translation_test.sh) — no freestanding
// toolchain, emulator, or boot cycle needed, since this function has no
// hardware I/O at all.

#include "../drivers/keyboard_translation.hpp"

#include <iostream>
#include <string>

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

}  // namespace

int main() {
    check(scancode_to_ascii(0x1E) == 'a', "scancode 0x1E translates to 'a'");
    check(scancode_to_ascii(0x23) == 'h', "scancode 0x23 translates to 'h'");
    check(scancode_to_ascii(0x17) == 'i', "scancode 0x17 translates to 'i'");
    check(scancode_to_ascii(0x02) == '1', "scancode 0x02 translates to '1'");
    check(scancode_to_ascii(0x39) == ' ', "scancode 0x39 (space bar) translates to ' '");
    check(scancode_to_ascii(0x1C) == '\n', "scancode 0x1C (Enter) translates to '\\n'");
    check(scancode_to_ascii(0x0E) == '\b', "scancode 0x0E (Backspace) translates to '\\b'");

    check(scancode_to_ascii(0x9E) == 0,
          "a break code (high bit set) translates to 0, not the make "
          "code's character (0x9E is 0x1E | 0x80, 'a' released)");

    check(scancode_to_ascii(0x2A) == 0,
          "Left Shift has no ASCII translation (returns 0)");
    check(scancode_to_ascii(0x1D) == 0,
          "Left Ctrl has no ASCII translation (returns 0)");

    check(scancode_to_ascii(0xFF) == 0,
          "an out-of-range scancode translates to 0, not undefined "
          "behavior from an out-of-bounds table read");
    check(scancode_to_ascii(0x00) == 0,
          "scancode 0 (never sent by real hardware) translates to 0");

    std::cout << "\n" << (failures == 0 ? "ALL TESTS PASSED" :
                           std::to_string(failures) + " TEST(S) FAILED")
              << "\n";

    return failures == 0 ? 0 : 1;
}
