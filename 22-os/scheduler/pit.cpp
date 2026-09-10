#include "pit.hpp"

#include "../kernel/serial.hpp"

static volatile uint64_t ticks = 0;

static inline void outb(uint16_t port, uint8_t value) {
    asm volatile(
        "outb %0, %1"
        :
        : "a"(value), "Nd"(port)
    );
}

static void write_decimal(uint64_t value) {
    char buffer[21];
    int position = 20;

    buffer[position] = '\0';

    if (value == 0) {
        serial_write("0");
        return;
    }

    while (value > 0) {
        buffer[--position] =
            static_cast<char>('0' + (value % 10));
        value /= 10;
    }

    serial_write(&buffer[position]);
}

void pit_init(uint32_t frequency) {
    constexpr uint32_t PIT_BASE_FREQUENCY = 1193182;

    if (frequency == 0)
        frequency = 100;

    uint32_t divisor =
        PIT_BASE_FREQUENCY / frequency;

    if (divisor > 65535)
        divisor = 65535;

    if (divisor < 1)
        divisor = 1;

    outb(0x43, 0x36);

    outb(
        0x40,
        static_cast<uint8_t>(divisor & 0xFF)
    );

    outb(
        0x40,
        static_cast<uint8_t>((divisor >> 8) & 0xFF)
    );

    serial_write("[PIT ] programmable timer online\n");
}

uint64_t pit_ticks() {
    return ticks;
}

extern "C" void pit_tick() {
    ++ticks;

    if ((ticks % 100) == 0) {
        serial_write("[TICK] ");
        write_decimal(ticks);
        serial_write("\n");
    }
}
