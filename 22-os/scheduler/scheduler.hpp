#pragma once

#include <stdint.h>

// Software scheduling policy: task lifecycle, round-robin selection, and
// context switching. Deliberately separate from the hardware IRQ0/IRQ1
// path (interrupts.cpp/isr.S), which only ever does hardware-facing work
// (EOI, raw tick counting) and then hands control here to decide what
// runs next.

using TaskEntry = void (*)();

extern "C" void scheduler_init();

// Called by interrupt_handler for the timer vector and the software
// yield vector respectively. Both take the kernel stack pointer of the
// interrupted context and return the kernel stack pointer to actually
// resume — the same value if no switch happens, or a different task's
// saved frame if one does.
extern "C" uint32_t scheduler_on_timer_tick(uint32_t currentEsp);
extern "C" uint32_t scheduler_on_yield(uint32_t currentEsp);

// Creates a new Ready task and returns its pid (a permanently unique
// handle — never reused, unlike the underlying task-table slot), or -1
// if the task table is full.
int scheduler_create_task(TaskEntry entry);

// Creates a new ring-3 (user-mode) task from a raw flat machine-code
// image (no relocation, no ELF — see userland/). `code` is copied into
// a freshly allocated physical page (identity-mapped, since this
// kernel does not use paging yet) that becomes the task's instruction
// stream starting at offset 0; `codeLen` must fit in one page. A
// second allocated page becomes its user-mode stack. Returns the new
// task's pid, or -1 if the task table is full or a page couldn't be
// allocated.
int scheduler_create_user_task(const uint8_t* code, uint32_t codeLen);

// Called by interrupt_handler when the currently running task must be
// terminated non-cooperatively — a ring-3 task that faulted (see
// interrupts.cpp) or a SYS_EXIT syscall — rather than via the
// cooperative scheduler_exit() a kernel task calls on itself. Marks it
// Dead and returns the kernel stack pointer to resume on (a different
// task; a Dead task is never rescheduled, exactly as for
// scheduler_exit()).
extern "C" uint32_t scheduler_terminate_current(uint32_t currentEsp);

// Callable only from within the currently running task. Marks it Dead
// and switches away; a Dead task is never scheduled again, and its pid
// is never reassigned, so no other code can ever be mistakenly directed
// at a task that has since exited (or worse, at a *different* task that
// later reuses the same table slot).
[[noreturn]] void scheduler_exit();

// Callable only from within the currently running task. Marks it
// Blocked and switches away; it will not run again until some other
// task calls scheduler_unblock with its pid.
void scheduler_block_self();

// No-ops if `pid` does not name a currently Blocked task — in
// particular if that task has already exited, or if the slot it used
// to occupy has since been reused by an unrelated task with a new pid.
void scheduler_unblock(int pid);

// Voluntarily gives up the remainder of this task's time slice.
void scheduler_yield();
