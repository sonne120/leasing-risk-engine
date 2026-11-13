#pragma once

#include "PathGenerator.h"
#include "scheme/Scheme.h"
#include "sde/Sde.h"

#include <atomic>
#include <cmath>
#include <cstdint>
#include <functional>
#include <thread>
#include <vector>

namespace leasing::mc {
struct SimResult {
    double mean         = 0.0;
    double stddev       = 0.0;
    double stderr_mean  = 0.0;   // SE of the mean = stddev / √n
    double ci95_low     = 0.0;
    double ci95_high    = 0.0;
    int    paths        = 0;
};

using Payoff = std::function<double(const std::vector<double>&)>;

// Welford's online statistics, with a parallel-combine step.
struct Welford {
    std::int64_t count = 0;
    double       mean  = 0.0;
    double       m2    = 0.0;      // Σ(x - mean)²

    void add(double x) {
        ++count;
        const double delta  = x - mean;
        mean               += delta / count;
        const double delta2 = x - mean;
        m2                 += delta * delta2;
    }

    void combine(const Welford& other) {
        if (other.count == 0) return;
        if (count == 0) { *this = other; return; }
        const std::int64_t n = count + other.count;
        const double delta = other.mean - mean;
        const double new_mean = mean + delta * other.count / n;
        m2    = m2 + other.m2 + delta * delta * count * other.count / n;
        count = n;
        mean  = new_mean;
    }

    double variance() const {
        return count > 1 ? m2 / (count - 1) : 0.0;
    }
    double stddev() const { return std::sqrt(variance()); }
};

class Simulation {
public:
    // Non-antithetic plain-MC driver.
    template <Scheme S>
    static SimResult run(const Sde& sde, PathConfig cfg,
                         int total_paths, const Payoff& payoff,
                         int threads = 0, std::uint64_t base_seed = 0xC0FFEEULL)
    {
        if (threads <= 0) {
            threads = static_cast<int>(std::thread::hardware_concurrency());
            if (threads <= 0) threads = 1;
        }
        if (total_paths <= 0) return {};

        const int paths_per = total_paths / threads;
        const int leftover  = total_paths - paths_per * threads;

        std::vector<Welford> partial(static_cast<std::size_t>(threads));
        std::vector<std::thread> workers;
        workers.reserve(static_cast<std::size_t>(threads));

        for (int t = 0; t < threads; ++t) {
            const int my_paths = paths_per + (t < leftover ? 1 : 0);
            workers.emplace_back(
                [t, my_paths, &sde, cfg, &payoff, base_seed, &partial]() {
                    PathGenerator<S> gen(sde, cfg,
                                         base_seed ^ (0x9E3779B97F4A7C15ULL * (t + 1)));
                    std::vector<double> path;
                    Welford w;
                    for (int i = 0; i < my_paths; ++i) {
                        gen.next_path(path);
                        w.add(payoff(path));
                    }
                    partial[static_cast<std::size_t>(t)] = w;
                });
        }
        for (auto& th : workers) th.join();

        Welford total;
        for (auto& w : partial) total.combine(w);

        SimResult r;
        r.paths       = static_cast<int>(total.count);
        r.mean        = total.mean;
        r.stddev      = total.stddev();
        r.stderr_mean = r.stddev / std::sqrt(static_cast<double>(r.paths));
        r.ci95_low    = r.mean - 1.96 * r.stderr_mean;
        r.ci95_high   = r.mean + 1.96 * r.stderr_mean;
        return r;
    }

    template <Scheme S>
    static SimResult run_antithetic(const Sde& sde, PathConfig cfg,
                                    int total_pairs, const Payoff& payoff,
                                    int threads = 0,
                                    std::uint64_t base_seed = 0xC0FFEEULL)
    {
        if (threads <= 0) {
            threads = static_cast<int>(std::thread::hardware_concurrency());
            if (threads <= 0) threads = 1;
        }
        if (total_pairs <= 0) return {};

        const int pairs_per = total_pairs / threads;
        const int leftover  = total_pairs - pairs_per * threads;

        std::vector<Welford> partial(static_cast<std::size_t>(threads));
        std::vector<std::thread> workers;
        workers.reserve(static_cast<std::size_t>(threads));

        for (int t = 0; t < threads; ++t) {
            const int my_pairs = pairs_per + (t < leftover ? 1 : 0);
            workers.emplace_back(
                [t, my_pairs, &sde, cfg, &payoff, base_seed, &partial]() {
                    PathGenerator<S> gen(sde, cfg,
                                         base_seed ^ (0x9E3779B97F4A7C15ULL * (t + 1)));
                    std::vector<double> p1, p2;
                    Welford w;
                    for (int i = 0; i < my_pairs; ++i) {
                        gen.next_antithetic_pair(p1, p2);
                        w.add(0.5 * (payoff(p1) + payoff(p2)));
                    }
                    partial[static_cast<std::size_t>(t)] = w;
                });
        }
        for (auto& th : workers) th.join();

        Welford total;
        for (auto& w : partial) total.combine(w);

        SimResult r;
        r.paths       = static_cast<int>(total.count);
        r.mean        = total.mean;
        r.stddev      = total.stddev();
        r.stderr_mean = r.stddev / std::sqrt(static_cast<double>(r.paths));
        r.ci95_low    = r.mean - 1.96 * r.stderr_mean;
        r.ci95_high   = r.mean + 1.96 * r.stderr_mean;
        return r;
    }
};

} 
