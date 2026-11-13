#pragma once

#include "../sde/Sde.h"
#include <concepts>

namespace leasing::mc {
template <typename S>
concept Scheme = requires(const Sde& sde, double t, double x, double dt, double dW) {
    { S::step(sde, t, x, dt, dW) } -> std::same_as<double>;
};

struct EulerScheme {
    static double step(const Sde& sde, double t, double x,
                       double dt, double dW)
    {
        return x + sde.drift(t, x) * dt + sde.diffusion(t, x) * dW;
    }
};

struct MilsteinScheme {
    static double step(const Sde& sde, double t, double x,
                       double dt, double dW)
    {
        const double sig  = sde.diffusion(t, x);
        const double dsig = sde.diffusion_derivative(t, x);
        return x + sde.drift(t, x) * dt
                 + sig * dW
                 + 0.5 * sig * dsig * (dW * dW - dt);
    }
};

static_assert(Scheme<EulerScheme>);
static_assert(Scheme<MilsteinScheme>);

} 
