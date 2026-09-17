#include "../kernel/serial.hpp"

#include "../memory/memory.hpp"
#include "../gdt/gdt.hpp"
#include "../interrupts/interrupts.hpp"
#include "../interrupts/pic.hpp"
#include "../scheduler/scheduler.hpp"
#include "../scheduler/pit.hpp"
#include "../syscalls/syscalls.hpp"
#include "../vfs/vfs.hpp"
#include "../security/security.hpp"
#include "../drivers/keyboard.hpp"

// Embedded userland flat binaries — see the Makefile's
// USERLAND_EMBED_OBJECTS rule (objcopy -I binary synthesizes these
// _binary_<path>_start/_end symbols from userland/hello.bin and
// userland/evil.bin).
extern "C" {
    extern const uint8_t _binary_userland_hello_bin_start[];
    extern const uint8_t _binary_userland_hello_bin_end[];
    extern const uint8_t _binary_userland_evil_bin_start[];
    extern const uint8_t _binary_userland_evil_bin_end[];
}

namespace {

void writeDecimal(uint32_t value) {
    char buffer[11];
    int position = 10;
    buffer[position] = '\0';

    if (value == 0) {
        serial_write("0");
        return;
    }

    while (value > 0) {
        buffer[--position] = static_cast<char>('0' + (value % 10));
        value /= 10;
    }

    serial_write(&buffer[position]);
}

// --------------------------------------------------------------
// Scheduler lifecycle test.
//
// task_a runs a fixed number of times and exits. task_b (the
// supervisor) watches for that exit, then keeps running long enough
// to prove task_a is never scheduled again afterward — the concrete,
// checkable form of "a dead task cannot resurrect". It then creates a
// brand-new task in the slot task_a vacated and confirms that
// unblocking task_a's *old* pid does not reach the new occupant,
// which is what actually makes resurrection structurally impossible
// rather than merely untested.
// --------------------------------------------------------------

constexpr uint32_t TASK_A_LIFETIME_RUNS = 5;
constexpr uint32_t OBSERVATION_WINDOW_RUNS = 30;
constexpr uint32_t TASK_C_LIFETIME_RUNS = 10;

volatile uint32_t taskARuns = 0;
volatile bool taskAExited = false;

volatile uint32_t taskCRuns = 0;

int taskAPid = -1;

void taskAEntry() {
    while (true) {
        ++taskARuns;

        if (taskARuns >= TASK_A_LIFETIME_RUNS) {
            taskAExited = true;
            scheduler_exit();
        }

        scheduler_yield();
    }
}

void taskCEntry() {
    while (true) {
        ++taskCRuns;

        if (taskCRuns >= TASK_C_LIFETIME_RUNS) {
            scheduler_exit();
        }

        scheduler_yield();
    }
}

void taskBEntry() {
    uint32_t runsAtExit = 0;
    bool captured = false;
    uint32_t windowStart = 0;
    bool spawnedTaskC = false;

    while (true) {
        if (!taskAExited) {
            scheduler_yield();
            continue;
        }

        if (!captured) {
            runsAtExit = taskARuns;
            captured = true;
            windowStart = 0;

            serial_write("[TEST] task A exited after ");
            writeDecimal(runsAtExit);
            serial_write(" run(s); observing for resurrection\n");
        }

        ++windowStart;

        if (!spawnedTaskC && windowStart == OBSERVATION_WINDOW_RUNS / 2) {
            // Reuse task A's now-Dead slot for an unrelated task, and
            // try to unblock task A's *old* pid — a real resurrection
            // bug would let this corrupt or wake the new occupant.
            scheduler_create_task(taskCEntry);

            serial_write("[TEST] reused task A's slot for a new task\n");

            // task A's pid is stale the moment it exits; this must be
            // a no-op rather than reaching whatever task now occupies
            // its old table slot.
            scheduler_unblock(taskAPid);
            spawnedTaskC = true;
        }

        if (windowStart >= OBSERVATION_WINDOW_RUNS) {
            bool noResurrection = (taskARuns == runsAtExit);

            serial_write(noResurrection
                ? "[PASS] dead task never ran again after exit\n"
                : "[FAIL] dead task ran again after exit\n");

            // If the stale unblock call had corrupted the new task's
            // scheduling state, it would not have made progress.
            bool taskCMadeProgress = (taskCRuns > 0);

            serial_write(taskCMadeProgress
                ? "[PASS] task reusing the dead slot ran normally "
                  "despite an unblock call against the old pid\n"
                : "[FAIL] the new task never ran; a stale pid may have "
                  "corrupted its scheduling state\n");

            serial_write("[TEST] task C observed ");
            writeDecimal(taskCRuns);
            serial_write(" run(s) in its own slot\n");

            scheduler_exit();
        }

        scheduler_yield();
    }
}

}  // namespace

extern "C" void kernel_main(uint32_t multiboot_magic, uint32_t multiboot_info) {
    (void)multiboot_magic;
    (void)multiboot_info;

    serial_init();

    serial_write("\n");
    serial_write("====================================\n");
    serial_write("      TECHUILAGUY OS KERNEL\n");
    serial_write("====================================\n");

    serial_write("[BOOT] kernel entered\n");

    memory_init();
    serial_write("[MEM ] physical frame allocator online\n");

    gdt_init();

    interrupts_init();
    pic_init();

    serial_write("[INT ] interrupt subsystem online\n");

    scheduler_init();
    serial_write("[SCHED] scheduler initialized\n");

    syscall_init();
    serial_write("[SYS ] syscall subsystem initialized\n");

    vfs_init();
    serial_write("[VFS ] virtual filesystem initialized\n");

    security_init();
    serial_write("[SEC ] capability security initialized\n");

    serial_write("------------------------------------\n");
    serial_write("TECHUILAGUY OS SCHEDULER TEST\n");
    serial_write("------------------------------------\n");

    pit_init(100);

    taskAPid = scheduler_create_task(taskAEntry);
    int taskBPid = scheduler_create_task(taskBEntry);

    serial_write("[TEST] created task A (pid ");
    writeDecimal(static_cast<uint32_t>(taskAPid));
    serial_write(") and task B (pid ");
    writeDecimal(static_cast<uint32_t>(taskBPid));
    serial_write(")\n");

    serial_write("------------------------------------\n");
    serial_write("TECHUILAGUY OS RING-3 USERSPACE TEST\n");
    serial_write("------------------------------------\n");

    uint32_t helloLen = static_cast<uint32_t>(
        _binary_userland_hello_bin_end - _binary_userland_hello_bin_start
    );
    uint32_t evilLen = static_cast<uint32_t>(
        _binary_userland_evil_bin_end - _binary_userland_evil_bin_start
    );

    int helloPid = scheduler_create_user_task(
        _binary_userland_hello_bin_start, helloLen
    );
    int evilPid = scheduler_create_user_task(
        _binary_userland_evil_bin_start, evilLen
    );

    serial_write("[TEST] created ring-3 task 'hello' (pid ");
    writeDecimal(static_cast<uint32_t>(helloPid));
    serial_write(", ");
    writeDecimal(helloLen);
    serial_write(" bytes) and 'evil' (pid ");
    writeDecimal(static_cast<uint32_t>(evilPid));
    serial_write(", ");
    writeDecimal(evilLen);
    serial_write(" bytes)\n");

    pic_unmask_irq(0);
    serial_write("[TEST] IRQ0 unmasked\n");

    keyboard_init();

    // Printed before, not after, enabling interrupts: once the first
    // timer tick arrives, control permanently leaves kernel_main's
    // original boot stack for the task system (see scheduler_init /
    // scheduler_on_timer_tick), so nothing queued after this point on
    // this stack is guaranteed to run before that happens.
    serial_write("[TEST] enabling interrupts — preemptive scheduling active\n");
    interrupts_enable();

    while (true) {
        asm volatile("hlt");
    }
}
