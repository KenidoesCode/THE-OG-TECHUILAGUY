#pragma once

#include <cstdint>

// A generation-checked entity handle — see
// docs/ADR/0026-ecs-game-engine-foundation.md's "Entity model" for
// exactly why the generation counter exists (stale-handle detection
// after an id is recycled).

namespace ecs {

struct Entity {
    uint32_t id = 0;
    uint32_t generation = 0;

    bool operator==(const Entity& other) const {
        return id == other.id && generation == other.generation;
    }
};

}  // namespace ecs
