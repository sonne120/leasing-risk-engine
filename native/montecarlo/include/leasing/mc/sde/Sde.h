// Stochastic differential equation — in the Duffy/Wilmott style.

#pragma once

#include <functional>

namespace leasing::mc {
using SdeFunction = std::function<double(double, double)>;

struct Sde {
    SdeFunction drift;                 // μ(t, x)
    SdeFunction diffusion;             // σ(t, x)
    SdeFunction diffusion_derivative;  // ∂σ/∂x — used by Milstein
    double      initial_value   = 0.0;
    double      expiration      = 1.0; // T in years (informational)
};

} // namespace leasing::mc
