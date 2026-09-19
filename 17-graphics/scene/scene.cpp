#include "scene.hpp"

#include "../render/rasterizer.hpp"

namespace graphics {

Mat4 lookAt(const sci::Vec3& eye, const sci::Vec3& target, const sci::Vec3& up) {
    sci::Vec3 forward = (target - eye).normalized();
    sci::Vec3 right = forward.cross(up).normalized();
    sci::Vec3 newUp = right.cross(forward);

    Mat4 result = Mat4::identity();
    result.m[0] = right.x;
    result.m[1] = right.y;
    result.m[2] = right.z;
    result.m[3] = -right.dot(eye);

    result.m[4] = newUp.x;
    result.m[5] = newUp.y;
    result.m[6] = newUp.z;
    result.m[7] = -newUp.dot(eye);

    result.m[8] = -forward.x;
    result.m[9] = -forward.y;
    result.m[10] = -forward.z;
    result.m[11] = forward.dot(eye);

    return result;
}

namespace {

bool projectToScreen(
    const sci::Vec3& worldPoint, const Mat4& viewProjection, size_t screenWidth, size_t screenHeight,
    ScreenVertex& out
) {
    double x, y, z, w;
    viewProjection.transformPoint(worldPoint, x, y, z, w);
    if (w <= 0.0) return false;  // behind the camera — see the ADR's "no clipping" limitation

    double ndcX = x / w;
    double ndcY = y / w;
    double ndcZ = z / w;

    out.x = (ndcX * 0.5 + 0.5) * static_cast<double>(screenWidth);
    out.y = (1.0 - (ndcY * 0.5 + 0.5)) * static_cast<double>(screenHeight);  // flip Y: +Y is up in NDC, down in screen space
    out.depth = ndcZ;
    return true;
}

}  // namespace

void renderScene(const Scene& scene, const Camera& camera, FrameBuffer& fb) {
    Mat4 view = lookAt(camera.position, camera.target);
    double aspect = static_cast<double>(fb.width()) / static_cast<double>(fb.height());
    Mat4 projection = Mat4::perspective(camera.fovYRadians, aspect, 0.1, 100.0);
    Mat4 viewProjection = projection.multiply(view);

    for (const auto& tri : scene.triangles) {
        ScreenVertex sv0, sv1, sv2;
        bool ok0 = projectToScreen(tri.v0, viewProjection, fb.width(), fb.height(), sv0);
        bool ok1 = projectToScreen(tri.v1, viewProjection, fb.width(), fb.height(), sv1);
        bool ok2 = projectToScreen(tri.v2, viewProjection, fb.width(), fb.height(), sv2);
        if (!ok0 || !ok1 || !ok2) continue;  // any vertex behind the camera: skip (no clipping yet)

        rasterizeTriangle(fb, sv0, sv1, sv2, tri.color);
    }
}

}  // namespace graphics
