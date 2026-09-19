#pragma once

#include "../math/mat4.hpp"
#include "../render/framebuffer.hpp"

#include <vector>

// The component set for this ECS slice — see
// docs/ADR/0026-ecs-game-engine-foundation.md for exactly why each one
// is shaped the way it is (matching what Mat4/Scene already support,
// not inventing new transform/rendering capability here).

namespace ecs {

struct Transform {
    sci::Vec3 position{0, 0, 0};
    sci::Vec3 scale{1, 1, 1};
    double rotationZRadians = 0.0;

    // translation * rotationZ * scale, reusing graphics::Mat4 directly
    // — no parallel transform-composition logic.
    graphics::Mat4 toMatrix() const {
        graphics::Mat4 t = graphics::Mat4::translation(position.x, position.y, position.z);
        graphics::Mat4 r = graphics::Mat4::rotationZ(rotationZRadians);
        graphics::Mat4 s = graphics::Mat4::scale(scale.x, scale.y, scale.z);
        return t.multiply(r).multiply(s);
    }
};

struct Velocity {
    sci::Vec3 linear{0, 0, 0};
};

struct MeshTriangle {
    sci::Vec3 v0, v1, v2;  // entity-local space
    graphics::Color color;
};

struct MeshComponent {
    std::vector<MeshTriangle> triangles;
};

struct CameraComponent {
    sci::Vec3 target{0, 0, 0};
    double fovYRadians = 1.0472;  // ~60 degrees
};

}  // namespace ecs
