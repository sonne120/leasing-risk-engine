#include <leasing/mc/sde/Models.h>
#include <leasing/risk/RiskMetrics.h>
#include <leasing/risk/RiskEngine.h>
#include <leasing/risk/ScenarioEngine.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <random>
#include <vector>

using namespace leasing::risk;
using namespace leasing::mc;

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

// Quantile / tail-mean on simple sequences

void test_quantile_on_integer_sequence() {
    std::printf("test_quantile_on_integer_sequence\n");
    // Sorted 1..100.
    std::vector<double> xs;
    xs.reserve(100);
    for (int i = 1; i <= 100; ++i) xs.push_back(static_cast<double>(i));

    REQUIRE_NEAR(RiskMetrics::quantile_sorted(xs, 0.50), 50.5,  1e-9);
    REQUIRE_NEAR(RiskMetrics::quantile_sorted(xs, 0.95), 95.05, 1e-9);
    REQUIRE_NEAR(RiskMetrics::quantile_sorted(xs, 0.99), 99.01, 1e-9);
    REQUIRE_NEAR(RiskMetrics::quantile_sorted(xs, 0.00), 1.0,   1e-9);
    REQUIRE_NEAR(RiskMetrics::quantile_sorted(xs, 1.00), 100.0, 1e-9);
}

void test_tail_mean_on_integer_sequence() {
    std::printf("test_tail_mean_on_integer_sequence\n");
    // 1..100 at α = 0.95
    std::vector<double> xs;
    for (int i = 1; i <= 100; ++i) xs.push_back(static_cast<double>(i));

    REQUIRE_NEAR(RiskMetrics::tail_mean_sorted(xs, 0.95), 98.0,  1e-9);
    REQUIRE_NEAR(RiskMetrics::tail_mean_sorted(xs, 0.99), 100.0, 1e-9);
}

void test_single_sample_edge_case() {
    std::printf("test_single_sample_edge_case\n");
    std::vector<double> xs{42.0};
    auto r = RiskMetrics::calculate(xs);
    REQUIRE(r.sample_size == 1);
    REQUIRE_NEAR(r.mean,      42.0, 1e-9);
    REQUIRE_NEAR(r.var95,     42.0, 1e-9);
    REQUIRE_NEAR(r.var99,     42.0, 1e-9);
    REQUIRE_NEAR(r.cvar95,    42.0, 1e-9);
    REQUIRE_NEAR(r.worst_case, 42.0, 1e-9);
    REQUIRE_NEAR(r.stddev,    0.0,  1e-9);
}

void test_empty_sample_edge_case() {
    std::printf("test_empty_sample_edge_case\n");
    auto r = RiskMetrics::calculate({});
    REQUIRE(r.sample_size == 0);
    // All other fields are zero by default — no NaN, no crash.
    REQUIRE_NEAR(r.mean, 0.0, 0.0);
    REQUIRE_NEAR(r.var95, 0.0, 0.0);
}

// Normal-distribution reference

void test_normal_distribution_reference() {
    std::printf("test_normal_distribution_reference\n");
    // Analytical for N(0,1)
    std::mt19937_64 rng(0xBEEF);
    std::normal_distribution<double> N(0.0, 1.0);
    std::vector<double> xs;
    xs.reserve(200'000);
    for (int i = 0; i < 200'000; ++i) xs.push_back(N(rng));

    auto r = RiskMetrics::calculate_in_place(xs);
    REQUIRE_NEAR(r.mean,    0.0,    0.01);
    REQUIRE_NEAR(r.stddev,  1.0,    0.01);
    REQUIRE_NEAR(r.var95,   1.6449, 0.03);
    REQUIRE_NEAR(r.var99,   2.3263, 0.05);
    REQUIRE_NEAR(r.cvar95,  2.0627, 0.03);
    REQUIRE_NEAR(r.cvar99,  2.6652, 0.06);
}

// RiskEngine end-to-end

void test_risk_engine_gbm() {
    std::printf("test_risk_engine_gbm\n");
    // GBM terminal "loss" = S(T).
    auto sde = make_gbm(0.05, 0.20, 100.0, 1.0);
    PathConfig cfg{1.0, 50, false};

    auto out = RiskEngine::run<EulerScheme>(
        sde, cfg, 20'000,
        [](const std::vector<double>& p) { return p.back(); },
        4, /*seed=*/777);

    REQUIRE(out.stats.paths == 20'000);
    REQUIRE(static_cast<int>(out.samples.size()) == 20'000);
    REQUIRE(std::is_sorted(out.samples.begin(), out.samples.end()));

    // Risk metrics shape — exact relationships that MUST hold.
    REQUIRE(out.risk.var95   < out.risk.var99);
    REQUIRE(out.risk.cvar95  > out.risk.var95);     // CVaR > VaR by definition
    REQUIRE(out.risk.cvar99  > out.risk.var99);
    REQUIRE(out.risk.worst_case >= out.risk.var99);
    REQUIRE(out.risk.best_case  <= out.risk.var95);
    REQUIRE_NEAR(out.risk.mean, out.stats.mean, 1e-9);  // same number
}

// ScenarioEngine common random numbers

void test_hw_parallel_shift_direction() {
    std::printf("test_hw_parallel_shift_direction\n");
    // Payoff = terminal short rate.
    PathConfig cfg{1.0, 50, false};

    auto payoff = [](const std::vector<double>& p) { return p.back(); };

    auto s = ScenarioEngine::hw_parallel_shift<EulerScheme>(
        cfg, /*paths=*/10'000,
        0.1, /*theta=*/0.03, /*sigma=*/0.01, /*r0=*/0.05,
        100.0,       // +1.00%
        payoff, /*threads=*/4, /*seed=*/555);

    REQUIRE(s.delta > 0.0);
    REQUIRE(s.delta > s.baseline_stderr + s.shifted_stderr);   // CRN-tight
    REQUIRE(s.shifted_mean > s.baseline_mean);
}

void test_gbm_vol_shock_mean_invariant() {
    std::printf("test_gbm_vol_shock_mean_invariant\n");
    // GBM terminal MEAN is independent of σ
    PathConfig cfg{1.0, 50, false};

    auto payoff = [](const std::vector<double>& p) { return p.back(); };

    auto s = ScenarioEngine::gbm_vol_shock<EulerScheme>(
        cfg, /*paths=*/20'000,
        0.05, /*sigma=*/0.20, /*s0=*/100.0,
        2.0,
        payoff, /*threads=*/4, /*seed=*/333);

    // |delta| should be modest — within ~3 combined stderrs.
    REQUIRE(std::fabs(s.delta) < 2.0);
}

} // namespace

int main() {
    test_quantile_on_integer_sequence();
    test_tail_mean_on_integer_sequence();
    test_single_sample_edge_case();
    test_empty_sample_edge_case();
    test_normal_distribution_reference();
    test_risk_engine_gbm();
    test_hw_parallel_shift_direction();
    test_gbm_vol_shock_mean_invariant();

    std::printf("\n%d passed, %d failed\n", g_passed, g_failed);
    return g_failed == 0 ? 0 : 1;
}
