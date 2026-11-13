#include <leasing/mc/sde/Models.h>
#include <leasing/mc/scheme/Scheme.h>
#include <leasing/mc/PathGenerator.h>
#include <leasing/mc/Simulation.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <numeric>
#include <vector>

using namespace leasing::mc;

namespace {
int g_passed = 0;
int g_failed = 0;

#define REQUIRE(cond)                                                           \
    do { if (cond) { ++g_passed; }                                              \
         else { ++g_failed;                                                     \
             std::fprintf(stderr, "  FAIL  %s:%d  %s\n",                        \
                          __FILE__, __LINE__, #cond); } } while (0)

#define REQUIRE_NEAR(a, e, tol)                                                 \
    do { const double _a=(a), _e=(e);                                           \
         if (std::fabs(_a - _e) <= (tol)) { ++g_passed; }                       \
         else { ++g_failed;                                                     \
             std::fprintf(stderr,                                               \
                "  FAIL  %s:%d  |%.10f - %.10f| = %.3e > %.3e\n",               \
                __FILE__, __LINE__, _a, _e, std::fabs(_a-_e), (tol)); }         \
    } while (0)

// Welford

void test_welford_vs_naive() {
    std::printf("test_welford_vs_naive\n");
    std::vector<double> xs;
    xs.reserve(10'000);
    for (int i = 0; i < 10'000; ++i) xs.push_back(1'000.0 + i * 0.001);

    const double naive_mean =
        std::accumulate(xs.begin(), xs.end(), 0.0) / xs.size();
    double sumsq = 0.0;
    for (double v : xs) sumsq += (v - naive_mean) * (v - naive_mean);
    const double naive_var = sumsq / (xs.size() - 1);

    // Welford in 7 unequal chunks.
    const std::vector<int> chunks{100, 200, 400, 800, 1600, 3200, 3700};
    int cursor = 0;
    Welford total;
    for (int c : chunks) {
        Welford w;
        for (int i = 0; i < c; ++i) w.add(xs[static_cast<std::size_t>(cursor++)]);
        total.combine(w);
    }
    REQUIRE(total.count == static_cast<int64_t>(xs.size()));
    REQUIRE_NEAR(total.mean,     naive_mean, 1e-9);
    REQUIRE_NEAR(total.variance(), naive_var, 1e-9);
}

// Deterministic Hull-White (σ = 0)

void test_hull_white_deterministic() {
    std::printf("test_hull_white_deterministic\n");
    // σ = 0 ⇒ the SDE is actually an ODE
    const double a   = 0.5;
    const double th  = 0.05;
    const double r0  = 0.10;
    const double T   = 2.0;
    const double exact = th + (r0 - th) * std::exp(-a * T);

    auto sde = make_hull_white(a, th, /*sigma=*/0.0, r0, T);
    PathConfig cfg{T, 2000, false};

    auto result = Simulation::run<EulerScheme>(
        sde, cfg, /*total_paths=*/8,
         [](const std::vector<double>& p) { return p.back(); },
        2);

    REQUIRE(result.paths == 8);
    REQUIRE_NEAR(result.stddev, 0.0, 1e-12);
    // Euler is first-order, so tolerance scales with dt = T/steps.
    REQUIRE_NEAR(result.mean, exact, 1e-3);
}

// GBM moment matching

void test_gbm_terminal_mean() {
    std::printf("test_gbm_terminal_mean\n");
    // E[S(T)] = S0 * exp(μT).
    const double S0 = 100.0, mu = 0.05, sigma = 0.20, T = 1.0;
    const double expected = S0 * std::exp(mu * T);    // 105.1271...

    auto sde = make_gbm(mu, sigma, S0, T);
    PathConfig cfg{T, 252, false};

    auto result = Simulation::run<EulerScheme>(
        sde, cfg, /*total_paths=*/40'000,
        [](const std::vector<double>& p) { return p.back(); },
        4,
        12345);

    REQUIRE(result.paths == 40'000);
    REQUIRE_NEAR(result.mean, expected, 0.5);
    REQUIRE(result.stderr_mean > 0.0);
    REQUIRE(result.ci95_low < expected);
    REQUIRE(result.ci95_high > expected);
}

// Antithetic reduces stderr

void test_antithetic_variance_reduction() {
    std::printf("test_antithetic_variance_reduction\n");
    auto sde = make_gbm(0.05, 0.20, 100.0, 1.0);
    PathConfig cfg{1.0, 100, false};
    auto payoff = [](const std::vector<double>& p) { return p.back(); };

    constexpr int N = 20'000;
    auto plain = Simulation::run<EulerScheme>(
        sde, cfg, /*paths=*/N, payoff, /*threads=*/2, /*seed=*/1);
    auto anti  = Simulation::run_antithetic<EulerScheme>(
        sde, cfg, /*pairs=*/N / 2, payoff, /*threads=*/2, /*seed=*/1);

    REQUIRE(anti.stderr_mean < plain.stderr_mean);
}

// CIR stays non-negative (full-truncation Euler)

void test_cir_full_truncation_finite() {
    std::printf("test_cir_full_truncation_finite\n");
    auto sde = make_cir(/*kappa=*/1.0, /*theta=*/0.10,
                        0.20, /*x0=*/0.10, /*T=*/1.0);
    PathConfig cfg{1.0, 500, false};

    PathGenerator<EulerScheme> gen(sde, cfg, /*seed=*/42);
    std::vector<double> path;
    double global_min   = 1e9;
    bool   all_finite   = true;
    for (int p = 0; p < 2'000; ++p) {
        gen.next_path(path);
        for (double v : path) {
            if (!std::isfinite(v)) all_finite = false;
            if (v < global_min)    global_min = v;
        }
    }
    REQUIRE(all_finite);                       // no NaN / Inf, ever
    REQUIRE(global_min > -1e-6);
}

// Trivial payoff identity

void test_constant_payoff() {
    std::printf("test_constant_payoff\n");
    auto sde = make_gbm(0.05, 0.20, 100.0, 1.0);
    PathConfig cfg{1.0, 50, false};

    auto result = Simulation::run<EulerScheme>(
        sde, cfg, /*paths=*/1'000,
        [](const std::vector<double>&) { return 1.0; },
        2);

    REQUIRE_NEAR(result.mean,   1.0, 1e-12);
    REQUIRE_NEAR(result.stddev, 0.0, 1e-12);
    REQUIRE(result.paths == 1'000);
}

} // namespace

int main() {
    test_welford_vs_naive();
    test_hull_white_deterministic();
    test_gbm_terminal_mean();
    test_antithetic_variance_reduction();
    test_cir_full_truncation_finite();
    test_constant_payoff();

    std::printf("\n%d passed, %d failed\n", g_passed, g_failed);
    return g_failed == 0 ? 0 : 1;
}
