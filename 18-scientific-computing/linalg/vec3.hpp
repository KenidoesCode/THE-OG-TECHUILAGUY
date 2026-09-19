#pragma once

#include <cmath>

// Minimal 3D vector algebra — the numerical foundation Layer 20
// (Space Systems)'s orbital mechanics is built directly on top of
// (see 20-space-systems/orbital/), a real cross-layer dependency
// chain (scientific computing -> space systems), not two layers
// sitting side by side. Deliberately small: this is not a general
// linear-algebra library (no arbitrary-size matrices, no linear
// solvers yet) — see docs/ADR/0016-scientific-computing-linalg.md.

namespace sci {

struct Vec3 {
    double x = 0.0, y = 0.0, z = 0.0;

    Vec3() = default;
    Vec3(double x, double y, double z) : x(x), y(y), z(z) {}

    Vec3 operator+(const Vec3& o) const { return {x + o.x, y + o.y, z + o.z}; }
    Vec3 operator-(const Vec3& o) const { return {x - o.x, y - o.y, z - o.z}; }
    Vec3 operator-() const { return {-x, -y, -z}; }
    Vec3 operator*(double s) const { return {x * s, y * s, z * s}; }
    Vec3 operator/(double s) const { return {x / s, y / s, z / s}; }

    Vec3& operator+=(const Vec3& o) { x += o.x; y += o.y; z += o.z; return *this; }

    double dot(const Vec3& o) const { return x * o.x + y * o.y + z * o.z; }

    Vec3 cross(const Vec3& o) const {
        return {
            y * o.z - z * o.y,
            z * o.x - x * o.z,
            x * o.y - y * o.x
        };
    }

    double normSquared() const { return dot(*this); }
    double norm() const { return std::sqrt(normSquared()); }

    Vec3 normalized() const {
        double n = norm();
        return n > 0.0 ? (*this) / n : Vec3(0, 0, 0);
    }
};

inline Vec3 operator*(double s, const Vec3& v) { return v * s; }

}  // namespace sci
