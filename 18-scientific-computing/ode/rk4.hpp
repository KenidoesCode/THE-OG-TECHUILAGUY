#pragma once

#include <array>
#include <cstdint>
#include <functional>

// A generic, real fixed-step 4th-order Runge-Kutta ODE integrator —
// the standard, textbook RK4 method (not an invented shortcut),
// reused directly by Layer 20's orbital propagator
// (20-space-systems/orbital/) rather than a bespoke integrator built
// just for orbits. See docs/ADR/0016-scientific-computing-linalg.md
// for accuracy characterization and scope (fixed step size only, no
// adaptive step control, no implicit/stiff-system methods).

namespace sci {

template <size_t N>
using State = std::array<double, N>;

template <size_t N>
using Derivative = std::function<State<N>(double t, const State<N>&)>;

template <size_t N>
State<N> addScaled(const State<N>& a, const State<N>& b, double scale) {
    State<N> result;
    for (size_t i = 0; i < N; ++i) result[i] = a[i] + b[i] * scale;
    return result;
}

// Advances `state` from time `t` by one step of size `dt` using
// classical RK4: y_{n+1} = y_n + (h/6)(k1 + 2k2 + 2k3 + k4). Local
// truncation error is O(h^5) per step (O(h^4) global) for a smooth
// derivative function `f` — the standard RK4 accuracy guarantee,
// verified in tests against problems with known exact solutions.
template <size_t N>
State<N> rk4Step(const Derivative<N>& f, double t, const State<N>& state, double dt) {
    State<N> k1 = f(t, state);
    State<N> k2 = f(t + dt / 2.0, addScaled(state, k1, dt / 2.0));
    State<N> k3 = f(t + dt / 2.0, addScaled(state, k2, dt / 2.0));
    State<N> k4 = f(t + dt, addScaled(state, k3, dt));

    State<N> result;
    for (size_t i = 0; i < N; ++i) {
        result[i] = state[i] + (dt / 6.0) * (k1[i] + 2.0 * k2[i] + 2.0 * k3[i] + k4[i]);
    }
    return result;
}

// Integrates from `t0` to `t0 + steps*dt`, taking `steps` fixed-size
// RK4 steps, and returns the final state (intermediate states are not
// retained — a caller needing the full trajectory should call
// rk4Step in its own loop instead).
template <size_t N>
State<N> rk4Integrate(
    const Derivative<N>& f, double t0, const State<N>& initialState,
    double dt, uint32_t steps
) {
    State<N> state = initialState;
    double t = t0;
    for (uint32_t i = 0; i < steps; ++i) {
        state = rk4Step(f, t, state, dt);
        t += dt;
    }
    return state;
}

}  // namespace sci
