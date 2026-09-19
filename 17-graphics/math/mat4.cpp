#include "mat4.hpp"

#include <cmath>

namespace graphics {

Mat4 Mat4::identity() {
    Mat4 result;
    result.m = {
        1, 0, 0, 0,
        0, 1, 0, 0,
        0, 0, 1, 0,
        0, 0, 0, 1,
    };
    return result;
}

Mat4 Mat4::translation(double x, double y, double z) {
    Mat4 result = identity();
    result.m[3] = x;
    result.m[7] = y;
    result.m[11] = z;
    return result;
}

Mat4 Mat4::scale(double x, double y, double z) {
    Mat4 result = identity();
    result.m[0] = x;
    result.m[5] = y;
    result.m[10] = z;
    return result;
}

Mat4 Mat4::rotationZ(double radians) {
    Mat4 result = identity();
    double c = std::cos(radians);
    double s = std::sin(radians);
    result.m[0] = c;
    result.m[1] = -s;
    result.m[4] = s;
    result.m[5] = c;
    return result;
}

Mat4 Mat4::perspective(double fovYRadians, double aspect, double nearPlane, double farPlane) {
    Mat4 result;
    result.m.fill(0.0);

    double f = 1.0 / std::tan(fovYRadians / 2.0);
    result.m[0] = f / aspect;
    result.m[5] = f;
    result.m[10] = (farPlane + nearPlane) / (nearPlane - farPlane);
    result.m[11] = (2 * farPlane * nearPlane) / (nearPlane - farPlane);
    result.m[14] = -1.0;
    return result;
}

Mat4 Mat4::multiply(const Mat4& other) const {
    Mat4 result;
    result.m.fill(0.0);
    for (int row = 0; row < 4; ++row) {
        for (int col = 0; col < 4; ++col) {
            double sum = 0.0;
            for (int k = 0; k < 4; ++k) {
                sum += m[row * 4 + k] * other.m[k * 4 + col];
            }
            result.m[row * 4 + col] = sum;
        }
    }
    return result;
}

void Mat4::transformPoint(
    const sci::Vec3& p, double& outX, double& outY, double& outZ, double& outW
) const {
    outX = m[0] * p.x + m[1] * p.y + m[2] * p.z + m[3];
    outY = m[4] * p.x + m[5] * p.y + m[6] * p.z + m[7];
    outZ = m[8] * p.x + m[9] * p.y + m[10] * p.z + m[11];
    outW = m[12] * p.x + m[13] * p.y + m[14] * p.z + m[15];
}

}  // namespace graphics
