#include <stdint.h>

enum SyscallNumber : uint32_t {
    SYS_EXIT = 0,
    SYS_WRITE = 1,
    SYS_YIELD = 2,
    SYS_OPEN = 3,
    SYS_READ = 4,
    SYS_CLOSE = 5
};

extern "C" void syscall_init() {
    // Syscall ABI foundation.
    // Hardware entry/return will be added with the
    // architecture-specific syscall gate.
}
