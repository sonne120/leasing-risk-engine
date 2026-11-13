#include "leasing/async/McStdexec.h"

#include <leasing/mc/PathGenerator.h>
#include <leasing/mc/scheme/Scheme.h>

#include <stdexec/execution.hpp>
#include <exec/static_thread_pool.hpp>

#include <cmath>
#include <cstdint>
#include <thread>
#include <vector>

namespace ex = stdexec;

namespace leasing::async {
namespace {
// Per-worker loop.
leasing::mc::Welford
run_one_worker(const leasing::mc::Sde&    sde,
               leasing::mc::PathConfig    cfg,
               int                        my_paths,
               const leasing::mc::Payoff& payoff,
               std::uint64_t              seed)
{
    leasing::mc::PathGenerator<leasing::mc::EulerScheme> gen(sde, cfg, seed);
    std::vector<double> path;
    leasing::mc::Welford w;
    for (int i = 0; i < my_paths; ++i) {
        gen.next_path(path);
        w.add(payoff(path));
    }
    return w;
}

leasing::mc::SimResult
welford_to_sim_result(const leasing::mc::Welford& total)
{
    leasing::mc::SimResult r;
    r.paths       = static_cast<int>(total.count);
    r.mean        = total.mean;
    r.stddev      = total.stddev();
    r.stderr_mean = r.paths > 0
        ? r.stddev / std::sqrt(static_cast<double>(r.paths)) : 0.0;
    r.ci95_low    = r.mean - 1.96 * r.stderr_mean;
    r.ci95_high   = r.mean + 1.96 * r.stderr_mean;
    return r;
}

leasing::mc::SimResult
run_mc_on_pool(exec::static_thread_pool&  pool,
               const leasing::mc::Sde&    sde,
               leasing::mc::PathConfig    cfg,
               int                        total_paths,
               const leasing::mc::Payoff& payoff,
               std::uint64_t              base_seed)
{
    const int workers = static_cast<int>(pool.available_parallelism());
    const int paths_per = total_paths / workers;
    const int leftover  = total_paths - paths_per * workers;

    std::vector<leasing::mc::Welford> partials(
        static_cast<std::size_t>(workers));

    auto work = ex::schedule(pool.get_scheduler())
              | ex::bulk(ex::par, workers,
                    [&](int t) {
                        const int my_paths =
                            paths_per + (t < leftover ? 1 : 0);
                        partials[static_cast<std::size_t>(t)] =
                            run_one_worker(sde, cfg, my_paths, payoff,
                                base_seed ^ (0x9E3779B97F4A7C15ULL * (t + 1)));
                    })
              | ex::then([&]() {
                    leasing::mc::Welford total;
                    for (const auto& w : partials) total.combine(w);
                    return welford_to_sim_result(total);
                });

    auto result = ex::sync_wait(std::move(work));
    if (!result.has_value()) return {};         // stopped / cancelled
    auto [r] = result.value();
    return r;
}

} // namespace

leasing::mc::SimResult
run_mc_stdexec(const leasing::mc::Sde&    sde,
               leasing::mc::PathConfig    cfg,
               int                        total_paths,
               const leasing::mc::Payoff& payoff,
               int                        threads,
               std::uint64_t              base_seed)
{
    if (total_paths <= 0) return {};
    if (threads <= 0) {
        threads = static_cast<int>(std::thread::hardware_concurrency());
        if (threads <= 0) threads = 1;
    }
    exec::static_thread_pool pool{static_cast<unsigned>(threads)};
    return run_mc_on_pool(pool, sde, cfg, total_paths, payoff, base_seed);
}

leasing::risk::StressResult
stress_stdexec(const leasing::mc::Sde&    baseline,
               const leasing::mc::Sde&    shifted,
               leasing::mc::PathConfig    cfg,
               int                        total_paths,
               const leasing::mc::Payoff& payoff,
               int                        threads,
               std::uint64_t              seed)
{
    if (threads <= 0) {
        threads = static_cast<int>(std::thread::hardware_concurrency());
        if (threads <= 0) threads = 1;
    }
    exec::static_thread_pool pool{static_cast<unsigned>(threads)};

    // Baseline and shifted are built as senders on the same pool.
    auto s_base = ex::just()
                | ex::then([&] {
                    return run_mc_on_pool(pool, baseline, cfg, total_paths,
                                           payoff, seed);
                  });
    auto s_shk  = ex::just()
                | ex::then([&] {
                    return run_mc_on_pool(pool, shifted,  cfg, total_paths,
                                           payoff, seed);
                  });

    auto work = ex::when_all(std::move(s_base), std::move(s_shk))
              | ex::then([](leasing::mc::SimResult b,
                            leasing::mc::SimResult s) {
                    leasing::risk::StressResult r;
                    r.baseline_mean   = b.mean;
                    r.shifted_mean    = s.mean;
                    r.delta           = s.mean - b.mean;
                    r.delta_pct       = (b.mean != 0.0)
                        ? r.delta / std::abs(b.mean) * 100.0 : 0.0;
                    r.baseline_stderr = b.stderr_mean;
                    r.shifted_stderr  = s.stderr_mean;
                    return r;
                });

    auto result = ex::sync_wait(std::move(work));
    if (!result.has_value()) return {};
    auto [r] = result.value();
    return r;
}

} // namespace leasing::async
