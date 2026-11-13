#pragma once

#include <leasing/mc/sde/Sde.h>
#include <leasing/mc/Simulation.h>
#include <leasing/risk/ScenarioEngine.h>

#include <cstdint>

namespace leasing::async {
// Fork-join style via ex::bulk.
leasing::mc::SimResult
run_mc_stdexec(const leasing::mc::Sde&    sde,
               leasing::mc::PathConfig    cfg,
               int                        total_paths,
               const leasing::mc::Payoff& payoff,
               int                        threads   = 0,
               std::uint64_t              base_seed = 0xC0FFEEULL);

leasing::risk::StressResult
stress_stdexec(const leasing::mc::Sde&    baseline,
               const leasing::mc::Sde&    shifted,
               leasing::mc::PathConfig    cfg,
               int                        total_paths,
               const leasing::mc::Payoff& payoff,
               int                        threads   = 0,
               std::uint64_t              seed      = 0xC0FFEEULL);

} 