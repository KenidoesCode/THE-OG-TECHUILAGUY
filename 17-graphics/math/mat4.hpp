#pragma once

#include "../../18-scientific-computing/linalg/vec3.hpp"

#include <array>

// A real 4x4 matrix for graphics transforms, operating on
// 18-scientific-computing's existing Vec3 for points — a genuine
// cross-layer integration (scientific computing -> graphics), not a
// parallel vector type. See
// docs/ADR/0025-graphics-software-rasterizer-foundation.md.

namespace graphics {

class Mat4 {
public:
    // Row-major 4x4, stored flat (m[row*4+col]).
    std::array<double, 16> m{};

    static Mat4 identity();
    static Mat4 translation(double x, double y, double z);
    static Mat4 scale(double x, double y, double z);
    static Mat4 rotationZ(double radians);

    // Standard field-of-view perspective projection (fovYRadians,
    // aspect = width/height, near/far > 0, near < far).
    static Mat4 perspective(double fovYRadians, double aspect, double nearPlane, double farPlane);

    Mat4 multiply(const Mat4& other) const;

    // Transforms a point (implicit w=1) and returns the resulting
    // homogeneous (x, y, z, w) — the caller performs the perspective
    // divide (dividing x/y/z by w) since not every use (e.g. an
    // affine-only transform) wants it applied.
    void transformPoint(const sci::Vec3& p, double& outX, double& outY, double& outZ, double& outW) const;
};

}  // namespace graphics
