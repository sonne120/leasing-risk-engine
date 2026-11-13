#pragma once

#include "RiskMetrics.h"

#include <leasing/mc/PathGenerator.h>
#include <leasing/mc/Simulation.h>
#include <leasing/mc/sde/Sde.h>
#include <leasing/mc/scheme/Scheme.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <functional>
#include <thread>
#include <vector>

namespace leasing::risk {
struct RiskRunResult {
    leasing::mc::SimResult stats;
    VaRResult              risk;
    std::vector<double>    samples;   // SORTED after calculate_in_place
};

class RiskEngine {
public:
    template <leasing::mc::Scheme S>
    static RiskRunResult run(
        const leasing::mc::Sde&         sde,
        leasing::mc::PathConfig         cfg,
        int                             total_paths,
        const leasing::mc::Payoff&      payoff,
        int                             threads   = 0,
        std::uint64_t                   base_seed = 0xC0FFEEULL)
    {
        if (threads <= 0) {
            threads = static_cast<int>(std::thread::hardware_concurrency());
            if (threads <= 0) threads = 1;
        }
        if (total_paths <= 0) return {};

        const int paths_per = total_paths / threads;
        const int leftover  = total_paths - paths_per * threads;

        // Each worker fills its own vector of payoffs.
        std::vector<std::vector<double>> local_samples(
            static_cast<std::size_t>(threads));
        std::vector<leasing::mc::Welford> partial(
            static_cast<std::size_t>(threads));

        std::vector<std::thread> workers;
        workers.reserve(static_cast<std::size_t>(threads));

        for (int t = 0; t < threads; ++t) {
            const int my_paths = paths_per + (t < leftover ? 1 : 0);
            local_samples[static_cast<std::size_t>(t)].reserve(
                static_cast<std::size_t>(my_paths));
            workers.emplace_back(
                [t, my_paths, &sde, cfg, &payoff, base_seed,
                 &local_samples, &partial]() {
                    leasing::mc::PathGenerator<S> gen(
                        sde, cfg,
                        base_seed ^ (0x9E3779B97F4A7C15ULL * (t + 1)));
                    std::vector<double> path;
                    leasing::mc::Welford w;
                    auto& bucket = local_samples[static_cast<std::size_t>(t)];
                    for (int i = 0; i < my_paths; ++i) {
                        gen.next_path(path);
                        const double v = payoff(path);
                        w.add(v);
                        bucket.push_back(v);
                    }
                    partial[static_cast<std::size_t>(t)] = w;
                });
        }
        for (auto& th : workers) th.join();

        // Merge Welfords for stats.
        leasing::mc::Welford total;
        for (const auto& w : partial) total.combine(w);

        // Concatenate samples into one buffer (expected size known exactly).
        RiskRunResult out;
        out.samples.reserve(static_cast<std::size_t>(total_paths));
        for (auto& b : local_samples) {
            out.samples.insert(out.samples.end(), b.begin(), b.end());
            b.clear();
            b.shrink_to_fit();        // free each partial as we drain it
        }

        out.stats.paths       = static_cast<int>(total.count);
        out.stats.mean        = total.mean;
        out.stats.stddev      = total.stddev();
        out.stats.stderr_mean =
            out.stats.paths > 0
                ? out.stats.stddev / std::sqrt(static_cast<double>(out.stats.paths))
                : 0.0;
        out.stats.ci95_low    = out.stats.mean - 1.96 * out.stats.stderr_mean;
        out.stats.ci95_high   = out.stats.mean + 1.96 * out.stats.stderr_mean;

        // Risk on the collected empirical distribution.
        out.risk = RiskMetrics::calculate_in_place(out.samples);
        return out;
    }
};

} // namespace leasing::risk
