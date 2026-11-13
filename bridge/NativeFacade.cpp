#include "NativeFacade.h"

#include <leasing/core/LeasingCalculator.h>
#include <leasing/mc/scheme/Scheme.h>
#include <leasing/pipeline/MonteCarloPipeline.h>

#include <algorithm>
#include <cmath>

namespace leasing::bridge {
using namespace leasing::core;
using namespace leasing::mc;
using namespace leasing::risk;
using namespace leasing::pipeline;

using EulerPipeline    = leasing::pipeline::MonteCarloPipeline<EulerScheme>;
using MilsteinPipeline = leasing::pipeline::MonteCarloPipeline<MilsteinScheme>;

PaymentSchedule compute_schedule(const Contract& c)
{
    LeasingCalculator calc;
    return calc.CalculateSchedule(c);
}

double compute_npv(const std::vector<double>& cfs, double annual_rate)
{
    return LeasingCalculator::NPV(cfs, annual_rate);
}

double compute_irr(double initial_outflow, const std::vector<double>& cfs)
{
    return LeasingCalculator::IRR(initial_outflow, cfs);
}

SimResult run_simulation(const Sde& sde, PathConfig cfg,
                         int total_paths, const Payoff& payoff,
                         SchemeKind scheme, int threads,
                         std::uint64_t seed)
{
    switch (scheme) {
        case SchemeKind::Euler:
            return Simulation::run<EulerScheme>(
                sde, cfg, total_paths, payoff, threads, seed);
        case SchemeKind::Milstein:
            return Simulation::run<MilsteinScheme>(
                sde, cfg, total_paths, payoff, threads, seed);
    }
    return {};
}

SimResult run_simulation_antithetic(const Sde& sde, PathConfig cfg,
                                    int total_pairs, const Payoff& payoff,
                                    SchemeKind scheme, int threads,
                                    std::uint64_t seed)
{
    switch (scheme) {
        case SchemeKind::Euler:
            return Simulation::run_antithetic<EulerScheme>(
                sde, cfg, total_pairs, payoff, threads, seed);
        case SchemeKind::Milstein:
            return Simulation::run_antithetic<MilsteinScheme>(
                sde, cfg, total_pairs, payoff, threads, seed);
    }
    return {};
}

Payoff make_zero_bond_payoff(double T, int steps)
{
    const double dt = T / steps;
    return [dt](const std::vector<double>& path) {
        // Trapezoidal rule for ∫₀ᵀ r(t) dt.
        double integral = 0.0;
        for (std::size_t i = 0; i + 1 < path.size(); ++i) {
            integral += 0.5 * (path[i] + path[i + 1]) * dt;
        }
        return std::exp(-integral);
    };
}

Payoff make_european_call_payoff(double strike, double discount_rate, double T)
{
    const double disc = std::exp(-discount_rate * T);
    return [strike, disc](const std::vector<double>& path) {
        const double S_T = path.back();
        return disc * std::max(S_T - strike, 0.0);
    };
}

Payoff make_terminal_value_payoff()
{
    return [](const std::vector<double>& path) { return path.back(); };
}

Payoff make_zero_bond_loss_payoff(double T, int steps)
{
    // Negate the bond price so high values mean losses.
    auto zcb = make_zero_bond_payoff(T, steps);
    return [zcb](const std::vector<double>& path) { return -zcb(path); };
}

Payoff make_residual_value_loss_payoff(double s0)
{
    return [s0](const std::vector<double>& path) { return s0 - path.back(); };
}

// Risk

RiskRunResult
run_risk_simulation(const Sde& sde, PathConfig cfg,
                    int total_paths, const Payoff& payoff,
                    SchemeKind scheme, int threads,
                    std::uint64_t seed)
{
    switch (scheme) {
        case SchemeKind::Euler:
            return RiskEngine::run<EulerScheme>(
                sde, cfg, total_paths, payoff, threads, seed);
        case SchemeKind::Milstein:
            return RiskEngine::run<MilsteinScheme>(
                sde, cfg, total_paths, payoff, threads, seed);
    }
    return {};
}

StressResult
run_stress(const Sde& baseline, const Sde& shifted,
           PathConfig cfg, int total_paths,
           const Payoff& payoff, SchemeKind scheme,
           int threads, std::uint64_t seed)
{
    switch (scheme) {
        case SchemeKind::Euler:
            return ScenarioEngine::stress<EulerScheme>(
                baseline, shifted, cfg, total_paths,
                payoff, threads, seed);
        case SchemeKind::Milstein:
            return ScenarioEngine::stress<MilsteinScheme>(
                baseline, shifted, cfg, total_paths,
                payoff, threads, seed);
    }
    return {};
}

// Pipeline

EulerPipelineHandle create_euler_pipeline()
{
    return static_cast<void*>(new EulerPipeline());
}

MilsteinPipelineHandle create_milstein_pipeline()
{
    return static_cast<void*>(new MilsteinPipeline());
}

void destroy_euler_pipeline(EulerPipelineHandle p) noexcept
{
    delete static_cast<EulerPipeline*>(p);
}

void destroy_milstein_pipeline(MilsteinPipelineHandle p) noexcept
{
    delete static_cast<MilsteinPipeline*>(p);
}

void cancel_euler_pipeline(EulerPipelineHandle p) noexcept
{
    if (p) static_cast<EulerPipeline*>(p)->cancel();
}

void cancel_milstein_pipeline(MilsteinPipelineHandle p) noexcept
{
    if (p) static_cast<MilsteinPipeline*>(p)->cancel();
}

SimResult
run_euler_pipeline(EulerPipelineHandle p, const Sde& sde,
                   PathConfig path_cfg, int total_paths,
                   int worker_threads, int progress_every_ms,
                   std::uint64_t base_seed,
                   const Payoff& payoff, const ProgressBus& bus)
{
    auto* pipe = static_cast<EulerPipeline*>(p);
    PipelineConfig cfg;
    cfg.path_cfg          = path_cfg;
    cfg.total_paths       = total_paths;
    cfg.worker_threads    = worker_threads;
    cfg.progress_every_ms = progress_every_ms;
    cfg.base_seed         = base_seed;
    return pipe->run(sde, cfg, payoff, bus);
}

SimResult
run_milstein_pipeline(MilsteinPipelineHandle p, const Sde& sde,
                      PathConfig path_cfg, int total_paths,
                      int worker_threads, int progress_every_ms,
                      std::uint64_t base_seed,
                      const Payoff& payoff, const ProgressBus& bus)
{
    auto* pipe = static_cast<MilsteinPipeline*>(p);
    PipelineConfig cfg;
    cfg.path_cfg          = path_cfg;
    cfg.total_paths       = total_paths;
    cfg.worker_threads    = worker_threads;
    cfg.progress_every_ms = progress_every_ms;
    cfg.base_seed         = base_seed;
    return pipe->run(sde, cfg, payoff, bus);
}

} // namespace leasing::bridge
