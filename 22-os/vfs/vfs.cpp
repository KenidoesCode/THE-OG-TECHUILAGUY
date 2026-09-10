#include <stdint.h>

enum class NodeType : uint8_t {
    File,
    Directory,
    Device
};

struct VNode {
    NodeType type;
    uint64_t size;
};

static VNode root {
    NodeType::Directory,
    0
};

extern "C" void vfs_init() {
    root.type = NodeType::Directory;
    root.size = 0;
}
