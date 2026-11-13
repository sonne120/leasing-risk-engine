#pragma once

#include <leasing/mc/sde/Models.h>

namespace LeasingBridge {
public enum class SchemeKindManaged {
    Euler    = 0,
    Milstein = 1,
};

public value struct HullWhiteParams {
    double A;           // mean-reversion speed
    double Theta;       // long-term mean
    double Sigma;       // volatility
    double R0;          // initial rate
    double Expiration;  // T, years
};

public value struct CirParams {
    double Kappa;
    double Theta;
    double Sigma;
    double X0;
    double Expiration;
};

public value struct GbmParams {
    double Mu;
    double Sigma;
    double S0;
    double Expiration;
};

// Non-managed helpers used by ManagedMonteCarlo to materialise an `Sde`.
inline leasing::mc::Sde ToNative(HullWhiteParams p) {
    return leasing::mc::make_hull_white(
        p.A, p.Theta, p.Sigma, p.R0, p.Expiration);
}
inline leasing::mc::Sde ToNative(CirParams p) {
    return leasing::mc::make_cir(
        p.Kappa, p.Theta, p.Sigma, p.X0, p.Expiration);
}
inline leasing::mc::Sde ToNative(GbmParams p) {
    return leasing::mc::make_gbm(p.Mu, p.Sigma, p.S0, p.Expiration);
}

} 
