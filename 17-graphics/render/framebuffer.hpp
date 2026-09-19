#pragma once

#include <cstddef>
#include <cstdint>
#include <limits>
#include <vector>

// A plain, self-contained RGB pixel buffer + parallel depth buffer —
// deliberately not tied to any GPU/window API, so any future consumer
// (an image-file writer, a real window blit, a headless test) just
// reads the pixel array. See
// docs/ADR/0025-graphics-software-rasterizer-foundation.md.

namespace graphics {

struct Color {
    uint8_t r = 0, g = 0, b = 0;

    bool operator==(const Color& other) const {
        return r == other.r && g == other.g && b == other.b;
    }
};

class FrameBuffer {
public:
    FrameBuffer(size_t width, size_t height);

    size_t width() const { return widthPixels; }
    size_t height() const { return heightPixels; }

    void clear(const Color& color);

    // Unchecked, like std::vector::operator[] — callers needing safety
    // should check x < width() / y < height() first.
    void setPixel(size_t x, size_t y, const Color& color);
    Color getPixel(size_t x, size_t y) const;

    // Depth buffer: initialized to +infinity by clear() (nothing is
    // "in front of" an empty buffer). A smaller value is nearer.
    double getDepth(size_t x, size_t y) const;
    void setDepth(size_t x, size_t y, double depth);

private:
    size_t widthPixels;
    size_t heightPixels;
    std::vector<Color> pixels;
    std::vector<double> depths;
};

}  // namespace graphics
