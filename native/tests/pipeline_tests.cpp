#include <leasing/common/ring/mpmc_queue.hpp>
#include <leasing/mc/sde/Models.h>
#include <leasing/mc/scheme/Scheme.h>
#include <leasing/pipeline/MonteCarloPipeline.h>
#include <leasing/pipeline/ProgressBus.h>

#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <mutex>
#include <set>
#include <thread>
#include <vector>

using namespace leasing::mc;
using namespace leasing::pipeline;
using leasing::common::ring::MpmcQueue;

namespace {
int g_passed = 0;
int g_failed = 0;

#define REQUIRE(cond)                                                           \
    do { if (cond) { ++g_passed; } else { ++g_failed;                           \
        std::fprintf(stderr, "  FAIL  %s:%d  %s\n",                             \
                     __FILE__, __LINE__, #cond); } } while (0)

#define REQUIRE_NEAR(a, e, tol)                                                 \
    do { const double _a=(a), _e=(e);                                           \
         if (std::fabs(_a - _e) <= (tol)) { ++g_passed; }                       \
         else { ++g_failed;                                                     \
             std::fprintf(stderr,                                               \
                "  FAIL  %s:%d  |%.10f - %.10f| = %.3e > %.3e\n",               \
                __FILE__, __LINE__, _a, _e, std::fabs(_a-_e), (tol)); }         \
    } while (0)

// MpmcQueue basics

void test_mpmc_single_threaded() {
    std::printf("test_mpmc_single_threaded\n");
    MpmcQueue<int, 4> q;
    REQUIRE(q.empty_approx());
    REQUIRE(q.try_enqueue(1));
    REQUIRE(q.try_enqueue(2));
    REQUIRE(q.try_enqueue(3));
    REQUIRE(q.try_enqueue(4));
    REQUIRE(!q.try_enqueue(5));                    // full
    int x = 0;
    REQUIRE(q.try_dequeue(x)); REQUIRE(x == 1);
    REQUIRE(q.try_dequeue(x)); REQUIRE(x == 2);
    REQUIRE(q.try_enqueue(5));                     // room again
    REQUIRE(q.try_enqueue(6));
    REQUIRE(q.try_dequeue(x)); REQUIRE(x == 3);
    REQUIRE(q.try_dequeue(x)); REQUIRE(x == 4);
    REQUIRE(q.try_dequeue(x)); REQUIRE(x == 5);
    REQUIRE(q.try_dequeue(x)); REQUIRE(x == 6);
    REQUIRE(!q.try_dequeue(x));
}

void test_mpmc_concurrent_unique_values() {
    std::printf("test_mpmc_concurrent_unique_values\n");
    // 4 producers push 25'000 unique ints each; 4 consumers pop them all.
    constexpr int P = 4, C = 4, PER = 25'000;
    MpmcQueue<int, 1024> q;

    std::vector<std::thread> producers, consumers;
    std::atomic<int> popped{0};

    std::mutex mu;
    std::vector<int> collected;
    collected.reserve(P * PER);

    for (int p = 0; p < P; ++p) {
        producers.emplace_back([p, &q]() {
            for (int i = 0; i < PER; ++i) {
                while (!q.try_enqueue(p * PER + i)) {
                    std::this_thread::yield();
                }
            }
        });
    }
    for (int c = 0; c < C; ++c) {
        consumers.emplace_back([&]() {
            std::vector<int> local;
            local.reserve(PER);
            int v;
            while (popped.load(std::memory_order_acquire) < P * PER) {
                if (q.try_dequeue(v)) {
                    local.push_back(v);
                    popped.fetch_add(1, std::memory_order_acq_rel);
                } else {
                    std::this_thread::yield();
                }
            }
            std::lock_guard<std::mutex> lk(mu);
            collected.insert(collected.end(), local.begin(), local.end());
        });
    }
    for (auto& t : producers) t.join();
    for (auto& t : consumers) t.join();

    REQUIRE(collected.size() == static_cast<std::size_t>(P * PER));
    std::set<int> unique(collected.begin(), collected.end());
    REQUIRE(unique.size() == static_cast<std::size_t>(P * PER));
    REQUIRE(*unique.begin() == 0);
    REQUIRE(*unique.rbegin() == P * PER - 1);
}

// Pipeline convergence

void test_pipeline_gbm_terminal_mean() {
    std::printf("test_pipeline_gbm_terminal_mean\n");
    // E[S(T)] = S0 * exp(mu T).
    const double S0 = 100.0, mu = 0.05, sigma = 0.20, T = 1.0;
    const double expected = S0 * std::exp(mu * T);

    auto sde = make_gbm(mu, sigma, S0, T);

    PipelineConfig cfg;
    cfg.path_cfg          = PathConfig{T, 252, false};
    cfg.total_paths       = 40'000;
    cfg.worker_threads    = 4;
    cfg.progress_every_ms = 0;
    cfg.base_seed         = 12345;

    ProgressBus bus;
    std::atomic<int> callbacks{0};
    bus.set_callback([&](const Snapshot&) { callbacks.fetch_add(1); });

    MonteCarloPipeline<EulerScheme> pipe;
    auto result = pipe.run(sde, cfg,
                           [](const std::vector<double>& p) { return p.back(); },
                           bus);

    REQUIRE(result.paths == 40'000);
    REQUIRE_NEAR(result.mean, expected, 0.5);
    REQUIRE(result.ci95_low  < expected);
    REQUIRE(result.ci95_high > expected);
    REQUIRE(callbacks.load() > 0);              // at least one publish
}

// Cancellation
void test_pipeline_cancel() {
    std::printf("test_pipeline_cancel\n");
    auto sde = make_gbm(0.05, 0.20, 100.0, 1.0);

    PipelineConfig cfg;
    cfg.path_cfg          = PathConfig{1.0, 100, false};
    cfg.total_paths       = 2'000'000;              // too many to finish fast
    cfg.worker_threads    = 2;
    cfg.progress_every_ms = 5;

    ProgressBus bus;
    Snapshot last_snapshot{};
    std::mutex last_mu;
    bus.set_callback([&](const Snapshot& s) {
        std::lock_guard<std::mutex> lk(last_mu);
        last_snapshot = s;
    });

    MonteCarloPipeline<EulerScheme> pipe;

    std::thread canceller([&]() {
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        pipe.cancel();
    });

    const auto t0 = std::chrono::steady_clock::now();
    auto result = pipe.run(sde, cfg,
                           [](const std::vector<double>& p) { return p.back(); },
                           bus);
    const auto elapsed_ms =
        std::chrono::duration<double, std::milli>(
            std::chrono::steady_clock::now() - t0).count();

    canceller.join();

    REQUIRE(elapsed_ms < 2000.0);                   // bailed out fast
    REQUIRE(result.paths < cfg.total_paths);        // did not finish fully

    std::lock_guard<std::mutex> lk(last_mu);
    REQUIRE(last_snapshot.final_);
    REQUIRE(last_snapshot.cancelled);
}

// Progress cadence
void test_pipeline_progress_callback_fires() {
    std::printf("test_pipeline_progress_callback_fires\n");
    auto sde = make_gbm(0.05, 0.20, 100.0, 1.0);

    PipelineConfig cfg;
    cfg.path_cfg          = PathConfig{1.0, 50, false};
    cfg.total_paths       = 20'000;
    cfg.worker_threads    = 2;
    cfg.progress_every_ms = 1;                      // publish often

    std::atomic<int> intermediate{0};
    std::atomic<int> finals{0};
    ProgressBus bus;
    bus.set_callback([&](const Snapshot& s) {
        if (s.final_) finals.fetch_add(1);
        else          intermediate.fetch_add(1);
    });

    MonteCarloPipeline<EulerScheme> pipe;
    (void)pipe.run(sde, cfg,
                   [](const std::vector<double>& p) { return p.back(); },
                   bus);

    REQUIRE(finals.load() == 1);              // exactly one final snapshot
    REQUIRE(intermediate.load() >= 1);        // at least one intermediate
}

} 

int main() {
    test_mpmc_single_threaded();
    test_mpmc_concurrent_unique_values();
    test_pipeline_gbm_terminal_mean();
    test_pipeline_cancel();
    test_pipeline_progress_callback_fires();

    std::printf("\n%d passed, %d failed\n", g_passed, g_failed);
    return g_failed == 0 ? 0 : 1;
}
