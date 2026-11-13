#pragma once

#include "ManagedErrors.h"
#include "ManagedSde.h"
#include "NativeFacade.h"

namespace LeasingBridge {
using namespace System;

// Result returned to C# after a run.
public ref class SimResultManaged {
public:
    property double Mean;
    property double StdDev;
    property double StdError;
    property double Ci95Low;
    property double Ci95High;
    property int    Paths;
};

public ref class MonteCarloEngineManaged {
public:
    // Hull-White zero-coupon bond price under short-rate model.
    SimResultManaged^ PriceDiscountBond(HullWhiteParams params,
                                        int steps,
                                        int paths,
                                        SchemeKindManaged scheme,
                                        bool antithetic,
                                        int threads,
                                        unsigned long long seed)
    {
        try {
            auto sde = ToNative(params);
            leasing::mc::PathConfig cfg{ params.Expiration, steps, antithetic };
            auto payoff = leasing::bridge::make_zero_bond_payoff(
                params.Expiration, steps);
            return Dispatch(sde, cfg, paths, payoff, scheme, antithetic,
                            threads, seed);
        }
        catch (const std::exception& ex) {
            throw ToManagedException(ex);
        }
    }

    // European call under GBM.
    SimResultManaged^ PriceEuropeanCall(GbmParams params,
                                        double strike,
                                        double discountRate,
                                        int steps,
                                        int paths,
                                        SchemeKindManaged scheme,
                                        bool antithetic,
                                        int threads,
                                        unsigned long long seed)
    {
        try {
            auto sde = ToNative(params);
            leasing::mc::PathConfig cfg{ params.Expiration, steps, antithetic };
            auto payoff = leasing::bridge::make_european_call_payoff(
                strike, discountRate, params.Expiration);
            return Dispatch(sde, cfg, paths, payoff, scheme, antithetic,
                            threads, seed);
        }
        catch (const std::exception& ex) {
            throw ToManagedException(ex);
        }
    }

    SimResultManaged^ TerminalMeanHullWhite(HullWhiteParams params,
                                            int steps, int paths,
                                            SchemeKindManaged scheme,
                                            bool antithetic,
                                            int threads,
                                            unsigned long long seed)
    {
        try {
            auto sde = ToNative(params);
            leasing::mc::PathConfig cfg{ params.Expiration, steps, antithetic };
            auto payoff = leasing::bridge::make_terminal_value_payoff();
            return Dispatch(sde, cfg, paths, payoff, scheme, antithetic,
                            threads, seed);
        }
        catch (const std::exception& ex) {
            throw ToManagedException(ex);
        }
    }

    SimResultManaged^ TerminalMeanGbm(GbmParams params,
                                      int steps, int paths,
                                      SchemeKindManaged scheme,
                                      bool antithetic,
                                      int threads,
                                      unsigned long long seed)
    {
        try {
            auto sde = ToNative(params);
            leasing::mc::PathConfig cfg{ params.Expiration, steps, antithetic };
            auto payoff = leasing::bridge::make_terminal_value_payoff();
            return Dispatch(sde, cfg, paths, payoff, scheme, antithetic,
                            threads, seed);
        }
        catch (const std::exception& ex) {
            throw ToManagedException(ex);
        }
    }

private:
    SimResultManaged^ Dispatch(const leasing::mc::Sde&         sde,
                               leasing::mc::PathConfig         cfg,
                               int                             paths,
                               const leasing::mc::Payoff&      payoff,
                               SchemeKindManaged               scheme,
                               bool                            antithetic,
                               int                             threads,
                               unsigned long long              seed)
    {
        const auto native_scheme =
            scheme == SchemeKindManaged::Milstein
                ? leasing::bridge::SchemeKind::Milstein
                : leasing::bridge::SchemeKind::Euler;

        leasing::mc::SimResult r = antithetic
            ? leasing::bridge::run_simulation_antithetic(
                  sde, cfg, paths, payoff, native_scheme, threads, seed)
            : leasing::bridge::run_simulation(
                  sde, cfg, paths, payoff, native_scheme, threads, seed);

        auto m = gcnew SimResultManaged();
        m->Mean     = r.mean;
        m->StdDev   = r.stddev;
        m->StdError = r.stderr_mean;
        m->Ci95Low  = r.ci95_low;
        m->Ci95High = r.ci95_high;
        m->Paths    = r.paths;
        return m;
    }
};

} 
