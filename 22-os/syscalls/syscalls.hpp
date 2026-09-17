#pragma once

#include <stdint.h>

#include "../interrupts/interrupts.hpp"

enum SyscallNumber : uint32_t {
    SYS_EXIT = 0,
    SYS_WRITE = 1,
    SYS_YIELD = 2,
    SYS_OPEN = 3,
    SYS_READ = 4,
    SYS_CLOSE = 5
};

// Negative-on-error return convention (eax on return from `int $0x80`),
// mirroring the syscall numbers' own style rather than inventing a
// separate errno mechanism this kernel doesn't have yet.
inline constexpr int32_t SYSCALL_ENOSYS = -1;  // No such syscall.

extern "C" void syscall_init();

// Dispatches a syscall trapped via int $0x80 (see isr_syscall in
// isr.S). `frame->eax` is the syscall number, `frame->ebx` its first
// argument; the result is written back into `frame->eax` so it's what
// the calling ring-3 code sees in %eax after `iret`. Returns the
// kernel stack pointer to resume on — ordinarily the same frame, but a
// different one for SYS_EXIT, which terminates the calling task.
//
// An unrecognized syscall number is not fatal: it returns
// SYSCALL_ENOSYS in eax and the calling task continues running,
// exactly like a real syscall ABI's ENOSYS rather than crashing the
// task over a simple bad argument.
extern "C" uint32_t syscall_dispatch(InterruptFrame* frame);
