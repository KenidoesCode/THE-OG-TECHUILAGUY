#include "scheduler.hpp"

#include "../interrupts/interrupts.hpp"
#include "../gdt/gdt.hpp"
#include "../memory/memory.hpp"

#include <stdint.h>

namespace {

constexpr int MAX_TASKS = 6;
constexpr uint32_t STACK_WORDS = 1024;  // 4 KiB per task stack.
constexpr uint32_t PAGE_SIZE = 4096;

enum class ProcessState : uint8_t {
    Unused,
    Ready,
    Running,
    Blocked,
    Dead
};

struct Task {
    uint32_t pid;
    ProcessState state;
    uint32_t savedEsp;
    bool isUser;
    uintptr_t userCodePage;   // 0 for kernel-mode tasks.
    uintptr_t userStackPage;  // 0 for kernel-mode tasks.

    // This task's own kernel-mode stack. For a kernel task, it runs on
    // this stack directly. For a user task, it never runs on this
    // stack at all — it only exists so the CPU has somewhere to land
    // (via the TSS's esp0, kept in sync by switchTo below) the moment
    // that task takes any interrupt, exception, or syscall from ring 3.
    uint32_t stack[STACK_WORDS];
};

Task tasks[MAX_TASKS];
int currentTaskIndex = -1;
uint32_t nextPid = 1;

uint32_t kernelStackTop(const Task& task) {
    return reinterpret_cast<uint32_t>(task.stack + STACK_WORDS);
}

void idleEntry() {
    while (true) {
        asm volatile("sti; hlt");
    }
}

// Places a synthesized InterruptFrame (see interrupts.hpp — this is
// exactly the trap frame isr_common pushes for a genuinely-interrupted
// task) at the top of `task`'s own kernel stack, so it can be resumed
// by the exact same "pop everything, iret" tail that resumes a real
// one. Shared by kernel- and user-mode task creation; the two differ
// only in which selectors/eip/esp end up in the frame.
InterruptFrame* placeInitialFrame(Task& task) {
    uintptr_t frameAddr = kernelStackTop(task) - sizeof(InterruptFrame);
    frameAddr &= ~static_cast<uintptr_t>(0xF);

    auto* frame = reinterpret_cast<InterruptFrame*>(frameAddr);

    frame->edi = 0;
    frame->esi = 0;
    frame->ebp = 0;
    frame->esp = 0;  // Unused: popa skips this slot on restore.
    frame->ebx = 0;
    frame->edx = 0;
    frame->ecx = 0;
    frame->eax = 0;
    frame->interrupt_number = 32;
    frame->error_code = 0;
    frame->eflags = 0x202;  // IF set, reserved bit 1 set.

    task.savedEsp = static_cast<uint32_t>(frameAddr);
    return frame;
}

void prepareInitialFrame(Task& task, TaskEntry entry) {
    InterruptFrame* frame = placeInitialFrame(task);

    frame->gs = KERNEL_DATA_SELECTOR;
    frame->fs = KERNEL_DATA_SELECTOR;
    frame->es = KERNEL_DATA_SELECTOR;
    frame->ds = KERNEL_DATA_SELECTOR;
    frame->eip = reinterpret_cast<uint32_t>(entry);
    frame->cs = KERNEL_CODE_SELECTOR;

    // useresp/ss are left unset: iret only consults them when the CS
    // it's restoring has a different RPL than the CPU's current CPL,
    // which never happens for a same-ring kernel task resume.
}

void prepareUserInitialFrame(
    Task& task,
    uintptr_t entryPoint,
    uintptr_t userStackTop
) {
    InterruptFrame* frame = placeInitialFrame(task);

    frame->gs = USER_DATA_SELECTOR_RPL3;
    frame->fs = USER_DATA_SELECTOR_RPL3;
    frame->es = USER_DATA_SELECTOR_RPL3;
    frame->ds = USER_DATA_SELECTOR_RPL3;
    frame->eip = static_cast<uint32_t>(entryPoint);
    frame->cs = USER_CODE_SELECTOR_RPL3;
    frame->useresp = static_cast<uint32_t>(userStackTop);
    frame->ss = USER_DATA_SELECTOR_RPL3;
}

int findSlot(ProcessState wanted) {
    for (int i = 0; i < MAX_TASKS; ++i) {
        if (tasks[i].state == wanted) return i;
    }
    return -1;
}

// Round-robin: the next Ready or Running slot after `from`, wrapping
// around. Unused, Blocked and — critically — Dead slots are always
// skipped, which is what makes resurrecting a dead task structurally
// impossible rather than merely avoided by convention.
int pickNext(int from) {
    if (from < 0) from = 0;

    for (int offset = 1; offset <= MAX_TASKS; ++offset) {
        int idx = (from + offset) % MAX_TASKS;
        if (tasks[idx].state == ProcessState::Ready ||
            tasks[idx].state == ProcessState::Running) {
            return idx;
        }
    }
    return -1;
}

uint32_t switchTo(int nextIndex) {
    if (currentTaskIndex >= 0 &&
        tasks[currentTaskIndex].state == ProcessState::Running) {
        tasks[currentTaskIndex].state = ProcessState::Ready;
    }

    tasks[nextIndex].state = ProcessState::Running;
    currentTaskIndex = nextIndex;

    // The TSS's esp0 must be correct *before* this task can possibly
    // take a trap — which for a user-mode task can happen the instant
    // it resumes (a timer tick, its own syscall, a fault). Kernel-mode
    // tasks never actually use ss0:esp0 (they never leave ring 0), but
    // setting it unconditionally is simpler and harmless.
    tss_set_kernel_stack(kernelStackTop(tasks[nextIndex]));

    return tasks[nextIndex].savedEsp;
}

int indexForPid(uint32_t pid) {
    for (int i = 0; i < MAX_TASKS; ++i) {
        if (tasks[i].pid == pid &&
            (tasks[i].state == ProcessState::Ready ||
             tasks[i].state == ProcessState::Running ||
             tasks[i].state == ProcessState::Blocked)) {
            return i;
        }
    }
    return -1;
}

}  // namespace

extern "C" void scheduler_init() {
    for (auto& t : tasks) {
        t.pid = 0;
        t.state = ProcessState::Unused;
        t.savedEsp = 0;
        t.isUser = false;
        t.userCodePage = 0;
        t.userStackPage = 0;
    }

    tasks[0].pid = nextPid++;
    tasks[0].state = ProcessState::Ready;
    prepareInitialFrame(tasks[0], idleEntry);

    currentTaskIndex = -1;
}

int scheduler_create_task(TaskEntry entry) {
    int slot = findSlot(ProcessState::Unused);
    if (slot < 0) slot = findSlot(ProcessState::Dead);
    if (slot < 0) return -1;

    uint32_t pid = nextPid++;

    tasks[slot].pid = pid;
    tasks[slot].state = ProcessState::Ready;
    prepareInitialFrame(tasks[slot], entry);

    return static_cast<int>(pid);
}

int scheduler_create_user_task(const uint8_t* code, uint32_t codeLen) {
    if (codeLen > PAGE_SIZE) {
        return -1;
    }

    int slot = findSlot(ProcessState::Unused);
    if (slot < 0) slot = findSlot(ProcessState::Dead);
    if (slot < 0) return -1;

    uintptr_t codePage = memory_alloc_page();
    if (codePage == 0) {
        return -1;
    }

    uintptr_t stackPage = memory_alloc_page();
    if (stackPage == 0) {
        memory_free_page(codePage);
        return -1;
    }

    // Identity-mapped (no paging yet), so the physical page address
    // doubles as both where the code lives and where it executes from.
    uint8_t* dst = reinterpret_cast<uint8_t*>(codePage);
    for (uint32_t i = 0; i < codeLen; ++i) {
        dst[i] = code[i];
    }

    // If a previous occupant of this slot leaked pages, free them now
    // rather than losing the reference.
    if (tasks[slot].userCodePage != 0) {
        memory_free_page(tasks[slot].userCodePage);
    }
    if (tasks[slot].userStackPage != 0) {
        memory_free_page(tasks[slot].userStackPage);
    }

    uint32_t pid = nextPid++;

    tasks[slot].pid = pid;
    tasks[slot].state = ProcessState::Ready;
    tasks[slot].isUser = true;
    tasks[slot].userCodePage = codePage;
    tasks[slot].userStackPage = stackPage;

    prepareUserInitialFrame(tasks[slot], codePage, stackPage + PAGE_SIZE);

    return static_cast<int>(pid);
}

extern "C" uint32_t scheduler_terminate_current(uint32_t currentEsp) {
    if (currentTaskIndex >= 0) {
        tasks[currentTaskIndex].state = ProcessState::Dead;
    }

    return scheduler_on_timer_tick(currentEsp);
}

extern "C" uint32_t scheduler_on_timer_tick(uint32_t currentEsp) {
    if (currentTaskIndex < 0) {
        return switchTo(0);
    }

    tasks[currentTaskIndex].savedEsp = currentEsp;

    int next = pickNext(currentTaskIndex);
    if (next < 0 || next == currentTaskIndex) {
        return currentEsp;
    }

    return switchTo(next);
}

extern "C" uint32_t scheduler_on_yield(uint32_t currentEsp) {
    return scheduler_on_timer_tick(currentEsp);
}

void scheduler_yield() {
    asm volatile("int $0x81");
}

[[noreturn]] void scheduler_exit() {
    if (currentTaskIndex >= 0) {
        tasks[currentTaskIndex].state = ProcessState::Dead;
    }

    scheduler_yield();

    // Unreachable: a Dead task is never selected by pickNext again, so
    // control never returns here.
    while (true) {
        asm volatile("cli; hlt");
    }
}

void scheduler_block_self() {
    if (currentTaskIndex >= 0) {
        tasks[currentTaskIndex].state = ProcessState::Blocked;
    }

    scheduler_yield();
}

void scheduler_unblock(int pid) {
    int idx = indexForPid(static_cast<uint32_t>(pid));
    if (idx < 0) return;

    if (tasks[idx].state == ProcessState::Blocked) {
        tasks[idx].state = ProcessState::Ready;
    }
}
