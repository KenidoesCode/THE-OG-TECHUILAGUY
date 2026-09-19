#pragma once

#include "framebuffer.hpp"

// Real barycentric-coordinate triangle rasterization with a per-pixel
// depth test — see
// docs/ADR/0025-graphics-software-rasterizer-foundation.md.

namespace graphics {

struct ScreenVertex {
    double x = 0.0;
    double y = 0.0;
    double depth = 0.0;  // smaller = nearer, matching FrameBuffer's depth convention
};

// Rasterizes the triangle (v0, v1, v2) — already in screen-pixel
// space — into `fb` with `color`, testing each covered pixel's
// interpolated depth against `fb`'s existing depth buffer so nearer
// geometry correctly wins regardless of draw order. A degenerate
// (zero-area) triangle draws nothing.
void rasterizeTriangle(
    FrameBuffer& fb, const ScreenVertex& v0, const ScreenVertex& v1, const ScreenVertex& v2,
    const Color& color
);

}  // namespace graphics
