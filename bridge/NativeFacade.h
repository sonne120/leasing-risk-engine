#pragma once

#include <leasing/core/Contract.h>
#include <leasing/core/PaymentSchedule.h>
#include <leasing/mc/sde/Sde.h>
#include <leasing/mc/Simulation.h>
#include <leasing/risk/RiskMetrics.h>
#include <leasing/risk/RiskEngine.h>
#include <leasing/risk/ScenarioEngine.h>
#include <leasing/pipeline/Snapshot.h>
#include <leasing/pipeline/ProgressBus.h>

#include <cstdint>
#include <functional>
#include <vector>

namespace leasing::bridge {
enum class SchemeKind { Euler, Milstein };

leasing::core::PaymentSchedule
compute_schedule(const leasing::core::Contract& c);

double compute_npv(const std::vector<double>& monthly_cashflows,
                   double annual_rate);

double compute_irr(double initial_outflow,
                   const std::vector<double>& monthly_cashflows);

// Runs plain Monte Carlo under the chosen scheme.
leasing::mc::SimResult
run_simulation(const leasing::mc::Sde& sde,
               leasing::mc::PathConfig cfg,
               int total_paths,
               const leasing::mc::Payoff& payoff,
               SchemeKind scheme,
               int threads,
               std::uint64_t seed);

// Runs antithetic Monte Carlo.
leasing::mc::SimResult
run_simulation_antithetic(const leasing::mc::Sde& sde,
                          leasing::mc::PathConfig cfg,
                          int total_pairs,
                          const leasing::mc::Payoff& payoff,
                          SchemeKind scheme,
                          int threads,
                          std::uint64_t seed);

leasing::mc::Payoff make_zero_bond_payoff(double T, int steps);

// GBM European call payoff
leasing::mc::Payoff make_european_call_payoff(double strike,
                                              double discount_rate,
                                              double T);

leasing::mc::Payoff make_terminal_value_payoff();

// Risk

leasing::risk::RiskRunResult
run_risk_simulation(const leasing::mc::Sde& sde,
                    leasing::mc::PathConfig cfg,
                    int total_paths,
                    const leasing::mc::Payoff& payoff,
                    SchemeKind scheme,
                    int threads,
                    std::uint64_t seed);

// Generic stress — baseline and shifted SDE share the seed.
leasing::risk::StressResult
run_stress(const leasing::mc::Sde& baseline,
           const leasing::mc::Sde& shifted,
           leasing::mc::PathConfig cfg,
           int total_paths,
           const leasing::mc::Payoff& payoff,
           SchemeKind scheme,
           int threads,
           std::uint64_t seed);

// Pipeline (opaque handles)

using EulerPipelineHandle    = void*;
using MilsteinPipelineHandle = void*;

EulerPipelineHandle    create_euler_pipeline();
MilsteinPipelineHandle create_milstein_pipeline();

void destroy_euler_pipeline(EulerPipelineHandle p) noexcept;
void destroy_milstein_pipeline(MilsteinPipelineHandle p) noexcept;

void cancel_euler_pipeline(EulerPipelineHandle p) noexcept;
void cancel_milstein_pipeline(MilsteinPipelineHandle p) noexcept;

leasing::mc::SimResult
run_euler_pipeline(EulerPipelineHandle p,
                   const leasing::mc::Sde& sde,
                   leasing::mc::PathConfig path_cfg,
                   int total_paths,
                   int worker_threads,
                   int progress_every_ms,
                   std::uint64_t base_seed,
                   const leasing::mc::Payoff& payoff,
                   const leasing::pipeline::ProgressBus& bus);

leasing::mc::SimResult
run_milstein_pipeline(MilsteinPipelineHandle p,
                      const leasing::mc::Sde& sde,
                      leasing::mc::PathConfig path_cfg,
                      int total_paths,
                      int worker_threads,
                      int progress_every_ms,
                      std::uint64_t base_seed,
                      const leasing::mc::Payoff& payoff,
                      const leasing::pipeline::ProgressBus& bus);

} 
