#include "keyboard_translation.hpp"

namespace {

// Set 1 make-code -> ASCII, US QWERTY, unshifted. Index 0 is unused
// (scancode 0 never occurs); entries with no ASCII meaning (Ctrl,
// Shift, function keys, arrows — none of which this table covers yet)
// are 0.
constexpr uint8_t kScancodeTable[0x3A] = {
    /* 0x00 */ 0,
    /* 0x01 */ 0,    // Esc
    /* 0x02 */ '1',
    /* 0x03 */ '2',
    /* 0x04 */ '3',
    /* 0x05 */ '4',
    /* 0x06 */ '5',
    /* 0x07 */ '6',
    /* 0x08 */ '7',
    /* 0x09 */ '8',
    /* 0x0A */ '9',
    /* 0x0B */ '0',
    /* 0x0C */ '-',
    /* 0x0D */ '=',
    /* 0x0E */ '\b',
    /* 0x0F */ '\t',
    /* 0x10 */ 'q',
    /* 0x11 */ 'w',
    /* 0x12 */ 'e',
    /* 0x13 */ 'r',
    /* 0x14 */ 't',
    /* 0x15 */ 'y',
    /* 0x16 */ 'u',
    /* 0x17 */ 'i',
    /* 0x18 */ 'o',
    /* 0x19 */ 'p',
    /* 0x1A */ '[',
    /* 0x1B */ ']',
    /* 0x1C */ '\n',
    /* 0x1D */ 0,    // Left Ctrl
    /* 0x1E */ 'a',
    /* 0x1F */ 's',
    /* 0x20 */ 'd',
    /* 0x21 */ 'f',
    /* 0x22 */ 'g',
    /* 0x23 */ 'h',
    /* 0x24 */ 'j',
    /* 0x25 */ 'k',
    /* 0x26 */ 'l',
    /* 0x27 */ ';',
    /* 0x28 */ '\'',
    /* 0x29 */ '`',
    /* 0x2A */ 0,    // Left Shift
    /* 0x2B */ '\\',
    /* 0x2C */ 'z',
    /* 0x2D */ 'x',
    /* 0x2E */ 'c',
    /* 0x2F */ 'v',
    /* 0x30 */ 'b',
    /* 0x31 */ 'n',
    /* 0x32 */ 'm',
    /* 0x33 */ ',',
    /* 0x34 */ '.',
    /* 0x35 */ '/',
    /* 0x36 */ 0,    // Right Shift
    /* 0x37 */ 0,    // Keypad *
    /* 0x38 */ 0,    // Left Alt
    /* 0x39 */ ' ',
};

}  // namespace

uint8_t scancode_to_ascii(uint8_t scancode) {
    // High bit set means a break code (key release); we only translate
    // make codes (key presses).
    if (scancode & 0x80) {
        return 0;
    }

    if (scancode >= sizeof(kScancodeTable)) {
        return 0;
    }

    return kScancodeTable[scancode];
}
