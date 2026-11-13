#pragma once

#include "ManagedErrors.h"
#include "ManagedSde.h"
#include "ManagedTypes.h"
#include "NativeFacade.h"

namespace LeasingBridge {
using namespace System;

public ref class VaRResultManaged {
public:
    property double Var95;
    property double Var99;
    property double Cvar95;
    property double Cvar99;
    property double Mean;
    property double StdDev;
    property double WorstCase;
    property double BestCase;
    property int    SampleSize;
};

public ref class RiskRunResultManaged {
public:
    property double Mean;
    property double StdDev;
    property double StdError;
    property double Ci95Low;
    property double Ci95High;
    property int    Paths;

    property VaRResultManaged^ Risk;

    // Sorted ascending.
    property array<double>^ Samples;
};

public ref class StressResultManaged {
public:
    property double BaselineMean;
    property double ShiftedMean;
    property double Delta;
    property double DeltaPercent;
    property double BaselineStdError;
    property double ShiftedStdError;
};

public ref class RiskEngineManaged {
public:
    // Hull-White
    RiskRunResultManaged^ ZeroBondLoss(HullWhiteParams params,
                                       int steps, int paths,
                                       SchemeKindManaged scheme,
                                       int threads,
                                       unsigned long long seed)
    {
        try {
            auto sde = ToNative(params);
            leasing::mc::PathConfig cfg{ params.Expiration, steps, false };
            auto loss_payoff = leasing::bridge::make_zero_bond_loss_payoff(
                params.Expiration, steps);

            auto res = leasing::bridge::run_risk_simulation(
                sde, cfg, paths, loss_payoff, NativeScheme(scheme), threads, seed);
            return ToManaged(res);
        }
        catch (const std::exception& ex) {
            throw ToManagedException(ex);
        }
    }

    // GBM
    RiskRunResultManaged^ ResidualValueLoss(GbmParams params,
                                            int steps, int paths,
                                            SchemeKindManaged scheme,
                                            int threads,
                                            unsigned long long seed)
    {
        try {
            auto sde = ToNative(params);
            leasing::mc::PathConfig cfg{ params.Expiration, steps, false };
            auto loss_payoff =
                leasing::bridge::make_residual_value_loss_payoff(params.S0);

            auto res = leasing::bridge::run_risk_simulation(
                sde, cfg, paths, loss_payoff, NativeScheme(scheme), threads, seed);
            return ToManaged(res);
        }
        catch (const std::exception& ex) {
            throw ToManagedException(ex);
        }
    }

private:
    static leasing::bridge::SchemeKind NativeScheme(SchemeKindManaged s) {
        return s == SchemeKindManaged::Milstein
            ? leasing::bridge::SchemeKind::Milstein
            : leasing::bridge::SchemeKind::Euler;
    }

    static RiskRunResultManaged^ ToManaged(const leasing::risk::RiskRunResult& r)
    {
        auto m = gcnew RiskRunResultManaged();
        m->Mean     = r.stats.mean;
        m->StdDev   = r.stats.stddev;
        m->StdError = r.stats.stderr_mean;
        m->Ci95Low  = r.stats.ci95_low;
        m->Ci95High = r.stats.ci95_high;
        m->Paths    = r.stats.paths;

        auto risk = gcnew VaRResultManaged();
        risk->Var95      = r.risk.var95;
        risk->Var99      = r.risk.var99;
        risk->Cvar95     = r.risk.cvar95;
        risk->Cvar99     = r.risk.cvar99;
        risk->Mean       = r.risk.mean;
        risk->StdDev     = r.risk.stddev;
        risk->WorstCase  = r.risk.worst_case;
        risk->BestCase   = r.risk.best_case;
        risk->SampleSize = r.risk.sample_size;
        m->Risk = risk;

        // Pin + bulk copy.
        const int n = static_cast<int>(r.samples.size());
        array<double>^ managed_samples = gcnew array<double>(n);
        if (n > 0) {
            pin_ptr<double> dst = &managed_samples[0];
            std::memcpy(static_cast<void*>(dst),
                        r.samples.data(),
                        sizeof(double) * static_cast<std::size_t>(n));
        }
        m->Samples = managed_samples;
        return m;
    }
};

public ref class ScenarioEngineManaged {
public:
    StressResultManaged^ HullWhiteParallelShift(HullWhiteParams base,
                                                double shiftBps,
                                                int steps, int paths,
                                                SchemeKindManaged scheme,
                                                int threads,
                                                unsigned long long seed)
    {
        try {
            const double shift = shiftBps / 10'000.0;
            auto base_sde = leasing::mc::make_hull_white(
                base.A, base.Theta, base.Sigma, base.R0, base.Expiration);
            auto shifted  = leasing::mc::make_hull_white(
                base.A, base.Theta + shift, base.Sigma,
                base.R0 + shift, base.Expiration);
            leasing::mc::PathConfig cfg{ base.Expiration, steps, false };
            auto payoff = leasing::bridge::make_terminal_value_payoff();

            auto r = leasing::bridge::run_stress(
                base_sde, shifted, cfg, paths, payoff,
                RiskEngineManagedNativeScheme(scheme), threads, seed);
            return ToManaged(r);
        }
        catch (const std::exception& ex) {
            throw ToManagedException(ex);
        }
    }

    // Multiply GBM σ by `volMultiplier`.
    StressResultManaged^ GbmVolShock(GbmParams base,
                                     double volMultiplier,
                                     int steps, int paths,
                                     SchemeKindManaged scheme,
                                     int threads,
                                     unsigned long long seed)
    {
        try {
            auto base_sde = leasing::mc::make_gbm(
                base.Mu, base.Sigma, base.S0, base.Expiration);
            auto shifted  = leasing::mc::make_gbm(
                base.Mu, base.Sigma * volMultiplier, base.S0, base.Expiration);
            leasing::mc::PathConfig cfg{ base.Expiration, steps, false };
            auto payoff = leasing::bridge::make_terminal_value_payoff();

            auto r = leasing::bridge::run_stress(
                base_sde, shifted, cfg, paths, payoff,
                RiskEngineManagedNativeScheme(scheme), threads, seed);
            return ToManaged(r);
        }
        catch (const std::exception& ex) {
            throw ToManagedException(ex);
        }
    }

private:
    static leasing::bridge::SchemeKind
    RiskEngineManagedNativeScheme(SchemeKindManaged s) {
        return s == SchemeKindManaged::Milstein
            ? leasing::bridge::SchemeKind::Milstein
            : leasing::bridge::SchemeKind::Euler;
    }

    static StressResultManaged^ ToManaged(const leasing::risk::StressResult& r)
    {
        auto m = gcnew StressResultManaged();
        m->BaselineMean     = r.baseline_mean;
        m->ShiftedMean      = r.shifted_mean;
        m->Delta            = r.delta;
        m->DeltaPercent     = r.delta_pct;
        m->BaselineStdError = r.baseline_stderr;
        m->ShiftedStdError  = r.shifted_stderr;
        return m;
    }
};

}
