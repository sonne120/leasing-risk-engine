#pragma once

#include "Sde.h"
#include <cmath>
#include <stdexcept>

namespace leasing::mc {
inline Sde make_hull_white(double a, double theta, double sigma,
                           double r0, double expiration = 1.0)
{
    if (a <= 0.0)     throw std::invalid_argument("Hull-White a must be > 0");
    if (sigma < 0.0)  throw std::invalid_argument("Hull-White σ must be ≥ 0");
    return Sde{
                        [a, theta](double, double r)  { return a * (theta - r); },
                    [sigma]  (double, double)      { return sigma; },
         []       (double, double)      { return 0.0;   },
        r0,
        expiration,
    };
}

inline Sde make_cir(double kappa, double theta, double sigma,
                    double x0, double expiration = 1.0)
{
    if (kappa <= 0.0) throw std::invalid_argument("CIR κ must be > 0");
    if (theta < 0.0)  throw std::invalid_argument("CIR θ must be ≥ 0");
    if (sigma < 0.0)  throw std::invalid_argument("CIR σ must be ≥ 0");
    if (x0 < 0.0)     throw std::invalid_argument("CIR x0 must be ≥ 0");
    return Sde{
        [kappa, theta](double, double x) { return kappa * (theta - x); },
        [sigma]       (double, double x) {
            return sigma * std::sqrt(x > 0.0 ? x : 0.0);
        },
        [sigma]       (double, double x) {
            return x > 1e-12 ? 0.5 * sigma / std::sqrt(x) : 0.0;
        },
        x0,
        expiration,
    };
}

inline Sde make_gbm(double mu, double sigma, double s0, double expiration = 1.0)
{
    if (s0 <= 0.0)   throw std::invalid_argument("GBM S0 must be > 0");
    if (sigma < 0.0) throw std::invalid_argument("GBM σ must be ≥ 0");
    return Sde{
        [mu]   (double, double s) { return mu * s;    },
        [sigma](double, double s) { return sigma * s; },
        [sigma](double, double)   { return sigma;     },   // ∂(σS)/∂S = σ
        s0,
        expiration,
    };
}

} // namespace leasing::mc
