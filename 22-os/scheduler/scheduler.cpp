#include "scheduler.hpp"

#include <stdint.h>

namespace {

constexpr int MAX_TASKS = 4;
constexpr uint32_t STACK_WORDS = 1024;  // 4 KiB per task stack.

enum class ProcessState : uint8_t {
    Unused,
    Ready,
    Running,
    Blocked,
    Dead
};

// Mirrors the trap frame isr_common pushes (see InterruptFrame in
// interrupts.hpp) so a freshly created task's synthesized initial frame
// can be resumed by the exact same "pop everything, iret" tail that
// resumes a genuinely-interrupted task.
struct InitialFrame {
    uint32_t gs, fs, es, ds;
    uint32_t edi, esi, ebp, espUnused, ebx, edx, ecx, eax;
    uint32_t interruptNumber, errorCode;
    uint32_t eip, cs, eflags;
};

struct Task {
    uint32_t pid;
    ProcessState state;
    uint32_t savedEsp;
    uint32_t stack[STACK_WORDS];
};

Task tasks[MAX_TASKS];
int currentTaskIndex = -1;
uint32_t nextPid = 1;
uint16_t kernelCs = 0;

void idleEntry() {
    while (true) {
        asm volatile("sti; hlt");
    }
}

void prepareInitialFrame(Task& task, TaskEntry entry) {
    uint8_t* top = reinterpret_cast<uint8_t*>(task.stack + STACK_WORDS);
    uintptr_t frameAddr =
        reinterpret_cast<uintptr_t>(top) - sizeof(InitialFrame);
    frameAddr &= ~static_cast<uintptr_t>(0xF);

    auto* frame = reinterpret_cast<InitialFrame*>(frameAddr);

    frame->gs = 0x18;
    frame->fs = 0x18;
    frame->es = 0x18;
    frame->ds = 0x18;
    frame->edi = 0;
    frame->esi = 0;
    frame->ebp = 0;
    frame->espUnused = 0;
    frame->ebx = 0;
    frame->edx = 0;
    frame->ecx = 0;
    frame->eax = 0;
    frame->interruptNumber = 32;
    frame->errorCode = 0;
    frame->eip = reinterpret_cast<uint32_t>(entry);
    frame->cs = kernelCs;
    frame->eflags = 0x202;  // IF set, reserved bit 1 set.

    task.savedEsp = static_cast<uint32_t>(frameAddr);
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
    }

    asm volatile("movw %%cs, %0" : "=r"(kernelCs));

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
