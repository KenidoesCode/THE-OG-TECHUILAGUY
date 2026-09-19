#pragma once

#include "../math/mat4.hpp"
#include "../render/framebuffer.hpp"

#include <vector>

// The real end-to-end path: world-space triangles + a camera ->
// FrameBuffer pixels. See
// docs/ADR/0025-graphics-software-rasterizer-foundation.md.

namespace graphics {

struct Triangle {
    sci::Vec3 v0, v1, v2;
    Color color;
};

struct Camera {
    sci::Vec3 position;
    sci::Vec3 target;
    double fovYRadians = 1.0472;  // ~60 degrees
};

struct Scene {
    std::vector<Triangle> triangles;
};

// Real lookAt view-matrix construction (right-handed, camera looks
// down -Z in view space) — a genuine, reusable camera transform, not
// a placeholder identity matrix.
Mat4 lookAt(const sci::Vec3& eye, const sci::Vec3& target, const sci::Vec3& up = sci::Vec3(0, 1, 0));

// Renders every triangle in `scene` from `camera`'s point of view into
// `fb`. `fb` is NOT cleared first — callers control clearing so a
// scene can be composited over an existing background.
void renderScene(const Scene& scene, const Camera& camera, FrameBuffer& fb);

}  // namespace graphics
