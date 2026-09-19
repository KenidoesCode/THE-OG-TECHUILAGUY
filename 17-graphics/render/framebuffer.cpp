#include "framebuffer.hpp"

namespace graphics {

FrameBuffer::FrameBuffer(size_t width, size_t height)
    : widthPixels(width),
      heightPixels(height),
      pixels(width * height),
      depths(width * height, std::numeric_limits<double>::infinity()) {}

void FrameBuffer::clear(const Color& color) {
    for (auto& pixel : pixels) pixel = color;
    for (auto& depth : depths) depth = std::numeric_limits<double>::infinity();
}

void FrameBuffer::setPixel(size_t x, size_t y, const Color& color) {
    pixels[y * widthPixels + x] = color;
}

Color FrameBuffer::getPixel(size_t x, size_t y) const {
    return pixels[y * widthPixels + x];
}

double FrameBuffer::getDepth(size_t x, size_t y) const {
    return depths[y * widthPixels + x];
}

void FrameBuffer::setDepth(size_t x, size_t y, double depth) {
    depths[y * widthPixels + x] = depth;
}

}  // namespace graphics
