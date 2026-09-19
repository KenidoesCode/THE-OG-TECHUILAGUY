// Real assertion-based tests for the deterministic software rasterizer
// foundation (17-graphics/) — hand-computed expected transforms and
// pixel positions, not visual inspection. See
// docs/ADR/0025-graphics-software-rasterizer-foundation.md.

#include "../math/mat4.hpp"
#include "../render/framebuffer.hpp"
#include "../render/rasterizer.hpp"
#include "../scene/scene.hpp"

#include <cmath>
#include <iostream>
#include <limits>
#include <string>

namespace {

int failures = 0;

void check(bool condition, const std::string& description) {
    if (condition) {
        std::cout << "[PASS] " << description << "\n";
    } else {
        std::cout << "[FAIL] " << description << "\n";
        failures++;
    }
}

bool nearlyEqual(double a, double b, double tolerance = 1e-9) {
    return std::fabs(a - b) < tolerance;
}

}  // namespace

void testFrameBufferBasics() {
    graphics::FrameBuffer fb(4, 4);
    graphics::Color black{0, 0, 0};
    graphics::Color white{255, 255, 255};

    fb.clear(black);
    check(fb.getPixel(0, 0) == black && fb.getPixel(3, 3) == black,
          "graphics: clear() sets every pixel to the given color");
    check(std::isinf(fb.getDepth(1, 1)), "graphics: clear() resets the depth buffer to +infinity everywhere");

    fb.setPixel(2, 1, white);
    check(fb.getPixel(2, 1) == white, "graphics: setPixel() actually changes the targeted pixel");
    check(fb.getPixel(0, 0) == black, "graphics: setPixel() does not affect any other pixel");
}

void testMat4IdentityAndTranslationAndScale() {
    sci::Vec3 p(1, 2, 3);
    double x, y, z, w;

    graphics::Mat4::identity().transformPoint(p, x, y, z, w);
    check(nearlyEqual(x, 1) && nearlyEqual(y, 2) && nearlyEqual(z, 3) && nearlyEqual(w, 1),
          "graphics: the identity matrix leaves a point unchanged");

    graphics::Mat4::translation(2, 3, 4).transformPoint(sci::Vec3(1, 1, 1), x, y, z, w);
    check(nearlyEqual(x, 3) && nearlyEqual(y, 4) && nearlyEqual(z, 5),
          "graphics: translation(2,3,4) applied to (1,1,1) gives the hand-computed (3,4,5)");

    graphics::Mat4::scale(2, 3, 4).transformPoint(sci::Vec3(1, 1, 1), x, y, z, w);
    check(nearlyEqual(x, 2) && nearlyEqual(y, 3) && nearlyEqual(z, 4),
          "graphics: scale(2,3,4) applied to (1,1,1) gives the hand-computed (2,3,4)");
}

void testMat4RotationZ() {
    double x, y, z, w;
    graphics::Mat4::rotationZ(M_PI / 2.0).transformPoint(sci::Vec3(1, 0, 0), x, y, z, w);
    check(nearlyEqual(x, 0, 1e-9) && nearlyEqual(y, 1, 1e-9),
          "graphics: rotating (1,0,0) by 90 degrees around Z gives the hand-computed (0,1,0)");
}

void testMat4Multiply() {
    // T translates by (10,0,0); S scales by 2. combined = T * S means
    // "apply S first, then T" under this project's row-major
    // row-vector-on-the-right convention (out = M * p).
    graphics::Mat4 t = graphics::Mat4::translation(10, 0, 0);
    graphics::Mat4 s = graphics::Mat4::scale(2, 2, 2);
    graphics::Mat4 combined = t.multiply(s);

    double x, y, z, w;
    combined.transformPoint(sci::Vec3(1, 0, 0), x, y, z, w);
    check(nearlyEqual(x, 12) && nearlyEqual(y, 0) && nearlyEqual(z, 0),
          "graphics: (translate * scale) applied to (1,0,0) scales first then translates, giving the hand-computed (12,0,0)");
}

void testPerspectiveMatrixEntries() {
    // fovY = 90 degrees -> tan(45deg) = 1 -> f = 1/tan(fov/2) = 1.
    graphics::Mat4 proj = graphics::Mat4::perspective(M_PI / 2.0, 1.0, 0.1, 100.0);
    check(nearlyEqual(proj.m[0], 1.0, 1e-9) && nearlyEqual(proj.m[5], 1.0, 1e-9),
          "graphics: a 90-degree-fovY, aspect=1 perspective matrix has the hand-computed f=1 on its diagonal");
    check(nearlyEqual(proj.m[14], -1.0), "graphics: the perspective matrix's w-row encodes the standard -1 for the perspective divide");
}

void testPointDirectlyInFrontOfCameraProjectsToScreenCenter() {
    sci::Vec3 eye(0, 0, 5);
    sci::Vec3 target(0, 0, 0);
    graphics::Mat4 view = graphics::lookAt(eye, target);
    graphics::Mat4 projection = graphics::Mat4::perspective(M_PI / 2.0, 1.0, 0.1, 100.0);
    graphics::Mat4 vp = projection.multiply(view);

    double x, y, z, w;
    vp.transformPoint(sci::Vec3(0, 0, 0), x, y, z, w);
    double ndcX = x / w;
    double ndcY = y / w;

    check(nearlyEqual(ndcX, 0.0, 1e-9) && nearlyEqual(ndcY, 0.0, 1e-9),
          "graphics: a world point directly on the camera's view axis projects to NDC (0,0) — the exact screen center");
}

void testRasterizeTriangleColorsInsideNotOutside() {
    graphics::FrameBuffer fb(10, 10);
    graphics::Color background{0, 0, 0};
    graphics::Color red{255, 0, 0};
    fb.clear(background);

    graphics::ScreenVertex v0{2, 2, 0.5};
    graphics::ScreenVertex v1{8, 2, 0.5};
    graphics::ScreenVertex v2{5, 8, 0.5};
    graphics::rasterizeTriangle(fb, v0, v1, v2, red);

    // (5,4) is the triangle's centroid pixel — well inside.
    check(fb.getPixel(5, 4) == red, "graphics: a pixel at the triangle's centroid is colored with the triangle's color");
    // (0,0) is far outside the triangle's bounding region entirely.
    check(fb.getPixel(0, 0) == background, "graphics: a pixel clearly outside the triangle remains the background color");
}

void testDepthTestKeepsNearerTriangleRegardlessOfDrawOrder() {
    graphics::Color background{0, 0, 0};
    graphics::Color red{255, 0, 0};   // near (depth 0.2)
    graphics::Color blue{0, 0, 255};  // far (depth 0.8)

    graphics::ScreenVertex v0{2, 2, 0.0};
    graphics::ScreenVertex v1{8, 2, 0.0};
    graphics::ScreenVertex v2{5, 8, 0.0};

    graphics::ScreenVertex nearV0 = v0, nearV1 = v1, nearV2 = v2;
    nearV0.depth = nearV1.depth = nearV2.depth = 0.2;
    graphics::ScreenVertex farV0 = v0, farV1 = v1, farV2 = v2;
    farV0.depth = farV1.depth = farV2.depth = 0.8;

    graphics::FrameBuffer fbFarThenNear(10, 10);
    fbFarThenNear.clear(background);
    graphics::rasterizeTriangle(fbFarThenNear, farV0, farV1, farV2, blue);
    graphics::rasterizeTriangle(fbFarThenNear, nearV0, nearV1, nearV2, red);

    graphics::FrameBuffer fbNearThenFar(10, 10);
    fbNearThenFar.clear(background);
    graphics::rasterizeTriangle(fbNearThenFar, nearV0, nearV1, nearV2, red);
    graphics::rasterizeTriangle(fbNearThenFar, farV0, farV1, farV2, blue);

    check(fbFarThenNear.getPixel(5, 4) == red, "graphics: drawing far-then-near, the nearer triangle correctly ends up visible");
    check(fbNearThenFar.getPixel(5, 4) == red,
          "graphics: drawing near-then-far, the farther triangle does NOT overwrite the nearer one (real depth testing, not painter's algorithm)");
}

void testRenderSceneIsDeterministic() {
    graphics::Scene scene;
    scene.triangles.push_back(graphics::Triangle{
        sci::Vec3(-1, -1, 0), sci::Vec3(1, -1, 0), sci::Vec3(0, 1, 0), graphics::Color{255, 0, 0}
    });
    graphics::Camera camera{sci::Vec3(0, 0, 5), sci::Vec3(0, 0, 0), 1.0472};

    graphics::FrameBuffer fb1(20, 20);
    fb1.clear(graphics::Color{0, 0, 0});
    graphics::renderScene(scene, camera, fb1);

    graphics::FrameBuffer fb2(20, 20);
    fb2.clear(graphics::Color{0, 0, 0});
    graphics::renderScene(scene, camera, fb2);

    bool identical = true;
    for (size_t y = 0; y < 20 && identical; ++y) {
        for (size_t x = 0; x < 20; ++x) {
            if (!(fb1.getPixel(x, y) == fb2.getPixel(x, y))) {
                identical = false;
                break;
            }
        }
    }
    check(identical, "graphics: rendering the identical scene twice produces byte-for-byte identical framebuffers (determinism end to end)");

    check(fb1.getPixel(10, 10) == graphics::Color{255, 0, 0},
          "graphics: a triangle centered in front of the camera actually renders visibly near the screen center");
}

int main() {
    testFrameBufferBasics();
    testMat4IdentityAndTranslationAndScale();
    testMat4RotationZ();
    testMat4Multiply();
    testPerspectiveMatrixEntries();
    testPointDirectlyInFrontOfCameraProjectsToScreenCenter();
    testRasterizeTriangleColorsInsideNotOutside();
    testDepthTestKeepsNearerTriangleRegardlessOfDrawOrder();
    testRenderSceneIsDeterministic();

    if (failures == 0) {
        std::cout << "\nALL TESTS PASSED\n";
    } else {
        std::cout << "\n" << failures << " TEST(S) FAILED\n";
    }

    return failures == 0 ? 0 : 1;
}
