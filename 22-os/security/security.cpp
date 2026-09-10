#include <stdint.h>

struct Capability {
    uint64_t object;
    uint64_t rights;
};

enum Rights : uint64_t {
    READ   = 1ull << 0,
    WRITE  = 1ull << 1,
    EXEC   = 1ull << 2,
    CREATE = 1ull << 3,
    ADMIN  = 1ull << 4
};

extern "C" void security_init() {
    // Capability security foundation.
    // Every privileged kernel object will eventually
    // require an explicit capability.
}
