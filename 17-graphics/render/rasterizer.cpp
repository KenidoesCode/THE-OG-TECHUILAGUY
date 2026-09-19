#include "rasterizer.hpp"

#include <algorithm>
#include <cmath>

namespace graphics {

namespace {

double edgeFunction(const ScreenVertex& a, const ScreenVertex& b, double px, double py) {
    return (px - a.x) * (b.y - a.y) - (py - a.y) * (b.x - a.x);
}

}  // namespace

void rasterizeTriangle(
    FrameBuffer& fb, const ScreenVertex& v0, const ScreenVertex& v1, const ScreenVertex& v2,
    const Color& color
) {
    double area = edgeFunction(v0, v1, v2.x, v2.y);
    if (area == 0.0) return;  // degenerate triangle

    double minXd = std::min({v0.x, v1.x, v2.x});
    double maxXd = std::max({v0.x, v1.x, v2.x});
    double minYd = std::min({v0.y, v1.y, v2.y});
    double maxYd = std::max({v0.y, v1.y, v2.y});

    long minX = std::max(0L, static_cast<long>(std::floor(minXd)));
    long maxX = std::min(static_cast<long>(fb.width()) - 1, static_cast<long>(std::ceil(maxXd)));
    long minY = std::max(0L, static_cast<long>(std::floor(minYd)));
    long maxY = std::min(static_cast<long>(fb.height()) - 1, static_cast<long>(std::ceil(maxYd)));

    for (long y = minY; y <= maxY; ++y) {
        for (long x = minX; x <= maxX; ++x) {
            double px = static_cast<double>(x) + 0.5;
            double py = static_cast<double>(y) + 0.5;

            double w0 = edgeFunction(v1, v2, px, py);
            double w1 = edgeFunction(v2, v0, px, py);
            double w2 = edgeFunction(v0, v1, px, py);

            bool inside = (w0 >= 0 && w1 >= 0 && w2 >= 0) || (w0 <= 0 && w1 <= 0 && w2 <= 0);
            if (!inside) continue;

            double b0 = w0 / area;
            double b1 = w1 / area;
            double b2 = w2 / area;
            double depth = b0 * v0.depth + b1 * v1.depth + b2 * v2.depth;

            size_t ux = static_cast<size_t>(x);
            size_t uy = static_cast<size_t>(y);
            if (depth < fb.getDepth(ux, uy)) {
                fb.setPixel(ux, uy, color);
                fb.setDepth(ux, uy, depth);
            }
        }
    }
}

}  // namespace graphics
