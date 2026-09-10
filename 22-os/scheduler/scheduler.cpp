#include <stdint.h>

enum class ProcessState : uint8_t {
    Ready,
    Running,
    Blocked,
    Dead
};

struct Process {
    uint32_t pid;
    ProcessState state;
};

static Process idle_process {
    0,
    ProcessState::Ready
};

extern "C" void scheduler_init() {
    idle_process.state = ProcessState::Running;
}
