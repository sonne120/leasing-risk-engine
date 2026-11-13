// Stress tests over Monte Carlo pricing.

#pragma once

#include <leasing/mc/Simulation.h>
#include <leasing/mc/sde/Models.h>
#include <leasing/mc/sde/Sde.h>
#include <leasing/mc/scheme/Scheme.h>

#include <cstdint>
#include <functional>
#include <stdexcept>

namespace leasing::risk {
struct StressResult {
    double baseline_mean   = 0.0;
    double shifted_mean    = 0.0;
    double delta           = 0.0;    // shifted - baseline
    double delta_pct       = 0.0;    // (delta / |baseline|) * 100
    double baseline_stderr = 0.0;
    double shifted_stderr  = 0.0;
};

class ScenarioEngine {
public:
    template <leasing::mc::Scheme S>
    static StressResult stress(
        const leasing::mc::Sde&         baseline_sde,
        const leasing::mc::Sde&         shifted_sde,
        leasing::mc::PathConfig         cfg,
        int                             total_paths,
        const leasing::mc::Payoff&      payoff,
        int                             threads   = 0,
        std::uint64_t                   seed      = 0xC0FFEEULL)
    {
        auto base = leasing::mc::Simulation::run<S>(
            baseline_sde, cfg, total_paths, payoff, threads, seed);
        auto shk  = leasing::mc::Simulation::run<S>(
            shifted_sde,  cfg, total_paths, payoff, threads, seed);

        StressResult r;
        r.baseline_mean   = base.mean;
        r.shifted_mean    = shk.mean;
        r.delta           = shk.mean - base.mean;
        r.delta_pct       = (base.mean != 0.0)
            ? r.delta / std::abs(base.mean) * 100.0
            : 0.0;
        r.baseline_stderr = base.stderr_mean;
        r.shifted_stderr  = shk.stderr_mean;
        return r;
    }

    template <leasing::mc::Scheme S>
    static StressResult hw_parallel_shift(
        leasing::mc::PathConfig         cfg,
        int                             total_paths,
        double                          a,
        double                          theta,
        double                          sigma,
        double                          r0,
        double                          shift_bps,
        const leasing::mc::Payoff&      payoff,
        int                             threads = 0,
        std::uint64_t                   seed    = 0xC0FFEEULL)
    {
        const double shift = shift_bps / 10'000.0;
        auto base = leasing::mc::make_hull_white(a, theta, sigma, r0, cfg.T);
        auto shk  = leasing::mc::make_hull_white(a, theta + shift, sigma,
                                                 r0 + shift, cfg.T);
        return stress<S>(base, shk, cfg, total_paths, payoff, threads, seed);
    }

    // GBM volatility shock — multiply σ by factor (e.g. 1.5 for 50% vol up).
    template <leasing::mc::Scheme S>
    static StressResult gbm_vol_shock(
        leasing::mc::PathConfig         cfg,
        int                             total_paths,
        double                          mu,
        double                          sigma,
        double                          s0,
        double                          vol_multiplier,
        const leasing::mc::Payoff&      payoff,
        int                             threads = 0,
        std::uint64_t                   seed    = 0xC0FFEEULL)
    {
        if (vol_multiplier <= 0.0) {
            throw std::invalid_argument("vol_multiplier must be > 0");
        }
        auto base = leasing::mc::make_gbm(mu, sigma,                    s0, cfg.T);
        auto shk  = leasing::mc::make_gbm(mu, sigma * vol_multiplier,   s0, cfg.T);
        return stress<S>(base, shk, cfg, total_paths, payoff, threads, seed);
    }
};

} // namespace leasing::risk
