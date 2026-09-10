#include "serial.hpp"

static inline void outb(uint16_t port, uint8_t value) {
    asm volatile(
        "outb %0, %1"
        :
        : "a"(value), "Nd"(port)
    );
}

extern "C" void serial_init() {
    outb(0x3F8 + 1, 0x00);
    outb(0x3F8 + 3, 0x80);
    outb(0x3F8 + 0, 0x03);
    outb(0x3F8 + 1, 0x00);
    outb(0x3F8 + 3, 0x03);
    outb(0x3F8 + 2, 0xC7);
    outb(0x3F8 + 4, 0x0B);
}

extern "C" void serial_write_char(char c) {
    outb(0x3F8, static_cast<uint8_t>(c));
}

extern "C" void serial_write(const char* text) {
    while (*text) {
        serial_write_char(*text);
        ++text;
    }
}
