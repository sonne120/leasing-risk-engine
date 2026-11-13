#include <leasing/async/McStdexec.h>
#include <leasing/mc/Simulation.h>
#include <leasing/mc/sde/Models.h>
#include <leasing/mc/scheme/Scheme.h>

#include <cmath>
#include <cstdio>

using namespace leasing::mc;
using namespace leasing::async;

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

void test_stdexec_gbm_terminal_mean()
{
    std::printf("test_stdexec_gbm_terminal_mean\n");
    // Same test as mc_tests.test_gbm_terminal_mean, via the stdexec path.
    const double S0 = 100.0, mu = 0.05, sigma = 0.20, T = 1.0;
    const double expected = S0 * std::exp(mu * T);   // 105.1271...

    auto sde = make_gbm(mu, sigma, S0, T);
    PathConfig cfg{T, 252, false};

    auto r = run_mc_stdexec(
        sde, cfg, /*paths=*/40'000,
        [](const std::vector<double>& p) { return p.back(); },
        4, /*seed=*/12345);

    REQUIRE(r.paths == 40'000);
    REQUIRE_NEAR(r.mean, expected, 0.5);
    REQUIRE(r.stderr_mean > 0.0);
    REQUIRE(r.ci95_low  < expected);
    REQUIRE(r.ci95_high > expected);
}

void test_stdexec_matches_handrolled_within_noise()
{
    std::printf("test_stdexec_matches_handrolled_within_noise\n");
    auto sde = make_gbm(0.03, 0.15, 50.0, 2.0);
    PathConfig cfg{2.0, 100, false};
    auto payoff = [](const std::vector<double>& p) { return p.back(); };

    auto hand = Simulation::run<EulerScheme>(
        sde, cfg, 20'000, payoff, 4, 0xAAAA);
    auto sdex = run_mc_stdexec(
        sde, cfg, 20'000, payoff, 4, 0xAAAA);

    REQUIRE(hand.paths == 20'000);
    REQUIRE(sdex.paths == 20'000);
    const double combined = hand.stderr_mean + sdex.stderr_mean;
    REQUIRE(std::fabs(hand.mean - sdex.mean) < 4.0 * combined);
}

void test_stdexec_stress_direction()
{
    std::printf("test_stdexec_stress_direction\n");
    // Payoff = terminal short rate.
    PathConfig cfg{1.0, 50, false};
    auto payoff = [](const std::vector<double>& p) { return p.back(); };

    const double a = 0.1, th = 0.03, sigma = 0.01, r0 = 0.05;
    const double shift = 0.01;                        // +100 bps

    auto base = make_hull_white(a, th,         sigma, r0,         cfg.T);
    auto shk  = make_hull_white(a, th + shift, sigma, r0 + shift, cfg.T);

    auto s = stress_stdexec(
        base, shk, cfg, /*paths=*/10'000, payoff, /*threads=*/4, /*seed=*/555);

    REQUIRE(s.delta > 0.0);
    REQUIRE(s.shifted_mean > s.baseline_mean);
    REQUIRE(s.delta > s.baseline_stderr + s.shifted_stderr);
}

void test_stdexec_zero_paths_edge_case()
{
    std::printf("test_stdexec_zero_paths_edge_case\n");
    auto sde = make_gbm(0.05, 0.20, 100.0, 1.0);
    auto r = run_mc_stdexec(
        sde, PathConfig{1.0, 10, false}, /*paths=*/0,
        [](const std::vector<double>& p) { return p.back(); });
    REQUIRE(r.paths == 0);
}

} // namespace

int main()
{
    test_stdexec_gbm_terminal_mean();
    test_stdexec_matches_handrolled_within_noise();
    test_stdexec_stress_direction();
    test_stdexec_zero_paths_edge_case();

    std::printf("\n%d passed, %d failed\n", g_passed, g_failed);
    return g_failed == 0 ? 0 : 1;
}
