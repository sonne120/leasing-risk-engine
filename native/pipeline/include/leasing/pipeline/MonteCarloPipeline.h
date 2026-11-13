#pragma once

#include "Snapshot.h"
#include "ProgressBus.h"

#include <leasing/common/ring/mempool.hpp>
#include <leasing/common/ring/mpmc_queue.hpp>
#include <leasing/common/ring/timing.hpp>
#include <leasing/mc/PathGenerator.h>
#include <leasing/mc/Simulation.h>
#include <leasing/mc/scheme/Scheme.h>
#include <leasing/mc/sde/Sde.h>

#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <functional>
#include <limits>
#include <thread>
#include <vector>

namespace leasing::pipeline {
struct PipelineConfig {
    leasing::mc::PathConfig path_cfg{};
    int           total_paths        = 10'000;
    int           worker_threads     = 0;       // 0 → hw concurrency
    int           progress_every_ms  = 50;      // 0 → publish every payoff
    std::uint64_t base_seed          = 0xC0FFEEULL;
};

// Compile-time sizes.
template <leasing::mc::Scheme SchemeT,
          std::size_t PathPoolCapacity    = 1024,
          std::size_t PayoffQueueCapacity = 65'536>
class MonteCarloPipeline {
public:
    using Payoff = leasing::mc::Payoff;

    MonteCarloPipeline() = default;

    MonteCarloPipeline(const MonteCarloPipeline&)            = delete;
    MonteCarloPipeline& operator=(const MonteCarloPipeline&) = delete;

    // Request graceful stop.
    void cancel() noexcept { cancelled_.store(true, std::memory_order_release); }

    // Reset cancellation state before a new run().
    void reset() noexcept { cancelled_.store(false, std::memory_order_release); }

    // Runs to completion (or until cancel()).
    leasing::mc::SimResult
    run(const leasing::mc::Sde& sde, PipelineConfig cfg,
        Payoff payoff, const ProgressBus& bus)
    {
        reset();

        const int workers = (cfg.worker_threads > 0)
            ? cfg.worker_threads
            : std::max<int>(1,
                static_cast<int>(std::thread::hardware_concurrency()));

        leasing::common::ring::MemPool<std::vector<double>, PathPoolCapacity> pool;
        leasing::common::ring::MpmcQueue<std::vector<double>*, PathPoolCapacity> path_q;
        leasing::common::ring::MpmcQueue<double, PayoffQueueCapacity>            payoff_q;

        std::atomic<bool> producer_done{false};
        std::atomic<int>  active_workers{workers};
        std::atomic<int>  paths_enqueued{0};

        leasing::common::ring::StopWatch clock;

        // Producer
        std::thread producer([&]() {
            leasing::mc::PathGenerator<SchemeT> gen(sde, cfg.path_cfg, cfg.base_seed);
            for (int i = 0; i < cfg.total_paths; ++i) {
                if (cancelled_.load(std::memory_order_acquire)) break;

                std::vector<double>* buf = nullptr;
                while (!(buf = pool.acquire())) {
                    if (cancelled_.load(std::memory_order_acquire)) break;
                    std::this_thread::yield();
                }
                if (!buf) break;

                gen.next_path(*buf);

                while (!path_q.try_enqueue(buf)) {
                    if (cancelled_.load(std::memory_order_acquire)) {
                        pool.release(buf);                // release OUR path
                        buf = nullptr;
                        break;
                    }
                    std::this_thread::yield();
                }
                if (!buf) break;
                paths_enqueued.fetch_add(1, std::memory_order_release);
            }
            producer_done.store(true, std::memory_order_release);
        });

        // Workers
        std::vector<std::thread> worker_threads;
        worker_threads.reserve(static_cast<std::size_t>(workers));
        for (int w = 0; w < workers; ++w) {
            worker_threads.emplace_back([&]() {
                for (;;) {
                    std::vector<double>* buf = nullptr;
                    if (path_q.try_dequeue(buf)) {
                        if (cancelled_.load(std::memory_order_acquire)) {
                            pool.release(buf);
                            continue;
                        }
                        const double v = payoff(*buf);
                        pool.release(buf);

                        while (!payoff_q.try_enqueue(v)) {
                            if (cancelled_.load(std::memory_order_acquire)) break;
                            std::this_thread::yield();
                        }
                    } else {
                        if (cancelled_.load(std::memory_order_acquire)) break;
                        if (producer_done.load(std::memory_order_acquire)
                            && path_q.empty_approx()) {
                            break;
                        }
                        std::this_thread::yield();
                    }
                }
                active_workers.fetch_sub(1, std::memory_order_acq_rel);
            });
        }

        // Aggregator (current thread)
        leasing::mc::Welford welford;
        double min_p = std::numeric_limits<double>::infinity();
        double max_p = -std::numeric_limits<double>::infinity();
        auto   last_publish = std::chrono::steady_clock::now();
        const auto publish_interval =
            std::chrono::milliseconds(cfg.progress_every_ms);

        for (;;) {
            double v = 0.0;
            if (payoff_q.try_dequeue(v)) {
                welford.add(v);
                if (v < min_p) min_p = v;
                if (v > max_p) max_p = v;

                // Periodic progress snapshot.
                if (cfg.progress_every_ms == 0 || bus.has_callback()) {
                    const auto now = std::chrono::steady_clock::now();
                    if (cfg.progress_every_ms == 0 ||
                        (now - last_publish) >= publish_interval) {
                        publish_snapshot(bus, cfg, welford, min_p, max_p,
                                         clock.elapsed_ms(),
                                         false, /*final=*/false);
                        last_publish = now;
                    }
                }
            } else {
                const bool cancelled = cancelled_.load(std::memory_order_acquire);
                const bool done_up   = (active_workers.load(std::memory_order_acquire) == 0);
                if (done_up && payoff_q.empty_approx()) break;
                if (cancelled && done_up && payoff_q.empty_approx()) break;
                std::this_thread::yield();
            }
        }

        producer.join();
        for (auto& t : worker_threads) t.join();

        // Final snapshot — guaranteed to fire exactly once per run.
        const bool was_cancelled = cancelled_.load(std::memory_order_acquire);
        publish_snapshot(bus, cfg, welford, min_p, max_p,
                         clock.elapsed_ms(), was_cancelled, /*final=*/true);

        leasing::mc::SimResult result;
        result.paths       = static_cast<int>(welford.count);
        result.mean        = welford.mean;
        result.stddev      = welford.stddev();
        result.stderr_mean =
            result.paths > 0 ? result.stddev / std::sqrt(static_cast<double>(result.paths)) : 0.0;
        result.ci95_low    = result.mean - 1.96 * result.stderr_mean;
        result.ci95_high   = result.mean + 1.96 * result.stderr_mean;
        return result;
    }

private:
    static void publish_snapshot(const ProgressBus&       bus,
                                 const PipelineConfig&    cfg,
                                 const leasing::mc::Welford& w,
                                 double min_p, double max_p,
                                 double elapsed_ms,
                                 bool cancelled, bool final_)
    {
        if (!bus.has_callback()) return;

        Snapshot s;
        s.paths_done   = static_cast<int>(w.count);
        s.paths_total  = cfg.total_paths;
        s.mean         = w.mean;
        s.stddev       = w.stddev();
        s.stderr_mean  = (s.paths_done > 0)
            ? s.stddev / std::sqrt(static_cast<double>(s.paths_done))
            : 0.0;
        s.min_payoff   = std::isfinite(min_p) ? min_p : 0.0;
        s.max_payoff   = std::isfinite(max_p) ? max_p : 0.0;
        s.elapsed_ms   = elapsed_ms;
        s.cancelled    = cancelled;
        s.final_       = final_;
        bus.publish(s);
    }

    std::atomic<bool> cancelled_{false};
};

} 
