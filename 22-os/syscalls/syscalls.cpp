#include "syscalls.hpp"

#include "../kernel/serial.hpp"
#include "../scheduler/scheduler.hpp"

extern "C" void syscall_init() {
    serial_write("[SYS ] syscall dispatch online (int $0x80)\n");
}

extern "C" uint32_t syscall_dispatch(InterruptFrame* frame) {
    uint32_t currentEsp = reinterpret_cast<uint32_t>(frame);

    switch (frame->eax) {
        case SYS_EXIT:
            return scheduler_terminate_current(currentEsp);

        case SYS_WRITE:
            // ebx: a single character to write. This is deliberately
            // the only I/O ring-3 code can do right now — there is no
            // real file-descriptor-backed write yet — but it's enough
            // to prove the syscall path is both *required* (a ring-3
            // program cannot reach the serial port's I/O ports
            // directly: attempting outb at CPL 3 with no I/O
            // permission bitmap set in the TSS faults with #GP) and
            // *sufficient* (the kernel performs the write on the
            // program's behalf and returns normally).
            serial_write_char(static_cast<char>(frame->ebx & 0xFF));
            frame->eax = 0;
            return currentEsp;

        case SYS_YIELD:
            frame->eax = 0;
            return scheduler_on_yield(currentEsp);

        case SYS_OPEN:
        case SYS_READ:
        case SYS_CLOSE:
            // Syscall numbers are reserved and recognized, but there is
            // no VFS-backed file descriptor table wired up to them yet
            // — honestly reported as "not implemented", not silently
            // faked as success.
            frame->eax = static_cast<uint32_t>(SYSCALL_ENOSYS);
            return currentEsp;

        default:
            frame->eax = static_cast<uint32_t>(SYSCALL_ENOSYS);
            return currentEsp;
    }
}
