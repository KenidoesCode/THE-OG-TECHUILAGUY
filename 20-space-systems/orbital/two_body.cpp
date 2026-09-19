#include "two_body.hpp"

#include <cmath>

namespace space {

namespace {

// State layout: [rx, ry, rz, vx, vy, vz].
sci::Derivative<6> makeTwoBodyDerivative(double mu) {
    return [mu](double, const sci::State<6>& s) -> sci::State<6> {
        sci::Vec3 r(s[0], s[1], s[2]);
        sci::Vec3 v(s[3], s[4], s[5]);

        double r3 = std::pow(r.norm(), 3);
        sci::Vec3 accel = r * (-mu / r3);

        return {v.x, v.y, v.z, accel.x, accel.y, accel.z};
    };
}

}  // namespace

OrbitalState propagateTwoBody(
    const OrbitalState& initial, double durationSeconds, double dt, double mu
) {
    sci::Derivative<6> f = makeTwoBodyDerivative(mu);

    sci::State<6> state = {
        initial.position.x, initial.position.y, initial.position.z,
        initial.velocity.x, initial.velocity.y, initial.velocity.z,
    };

    // durationSeconds is not generally an exact multiple of dt (e.g.
    // an orbital period computed from Kepler's third law is almost
    // never a whole number of seconds) — truncating to whole steps
    // and discarding the remainder would silently leave up to one
    // full step's worth of time unintegrated, which at orbital
    // velocities (several km/s) corresponds to several kilometers of
    // position error, not a negligible rounding artifact. A final
    // partial-duration RK4 step covers exactly the leftover time.
    uint32_t fullSteps = static_cast<uint32_t>(durationSeconds / dt);
    double remainder = durationSeconds - static_cast<double>(fullSteps) * dt;

    sci::State<6> result = sci::rk4Integrate<6>(f, 0.0, state, dt, fullSteps);

    if (remainder > 1e-9) {
        result = sci::rk4Step<6>(f, static_cast<double>(fullSteps) * dt, result, remainder);
    }

    return {
        sci::Vec3(result[0], result[1], result[2]),
        sci::Vec3(result[3], result[4], result[5]),
    };
}

double circularOrbitPeriodSeconds(double radiusKm, double mu) {
    return 2.0 * M_PI * std::sqrt(std::pow(radiusKm, 3) / mu);
}

double specificOrbitalEnergy(const OrbitalState& state, double mu) {
    double v2 = state.velocity.normSquared();
    double r = state.position.norm();
    return v2 / 2.0 - mu / r;
}

}  // namespace space
