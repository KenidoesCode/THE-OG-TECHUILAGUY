#include "keyboard.hpp"
#include "keyboard_translation.hpp"
#include "../kernel/serial.hpp"
#include "../interrupts/pic.hpp"

namespace {

inline uint8_t inb(uint16_t port) {
    uint8_t value;
    asm volatile("inb %1, %0" : "=a"(value) : "Nd"(port));
    return value;
}

}  // namespace

void keyboard_init() {
    pic_unmask_irq(1);
    serial_write("[KBD ] keyboard driver online\n");
}

extern "C" void keyboard_on_irq() {
    uint8_t scancode = inb(0x60);
    uint8_t ascii = scancode_to_ascii(scancode);

    if (ascii != 0) {
        serial_write_char(static_cast<char>(ascii));
    }
}
