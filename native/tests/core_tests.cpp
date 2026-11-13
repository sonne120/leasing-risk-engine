#include <leasing/core/Date.h>
#include <leasing/core/Contract.h>
#include <leasing/core/PaymentSchedule.h>
#include <leasing/core/LeasingCalculator.h>

#include <cmath>
#include <cstdio>
#include <stdexcept>
#include <string>
#include <vector>

using namespace leasing::core;

namespace {
int g_passed = 0;
int g_failed = 0;

#define REQUIRE(cond)                                                           \
    do {                                                                        \
        if (cond) { ++g_passed; }                                               \
        else {                                                                  \
            ++g_failed;                                                         \
            std::fprintf(stderr,                                                \
                "  FAIL  %s:%d  %s\n", __FILE__, __LINE__, #cond);              \
        }                                                                       \
    } while (0)

#define REQUIRE_NEAR(actual, expected, tol)                                     \
    do {                                                                        \
        const double _a = (actual);                                             \
        const double _e = (expected);                                           \
        if (std::fabs(_a - _e) <= (tol)) { ++g_passed; }                        \
        else {                                                                  \
            ++g_failed;                                                         \
            std::fprintf(stderr,                                                \
                "  FAIL  %s:%d  |%.10f - %.10f| = %.3e > %.3e\n",               \
                __FILE__, __LINE__, _a, _e,                                     \
                std::fabs(_a - _e), (tol));                                     \
        }                                                                       \
    } while (0)

#define REQUIRE_THROWS(expr)                                                    \
    do {                                                                        \
        bool _threw = false;                                                    \
        try { (void)(expr); }                                                   \
        catch (...) { _threw = true; }                                          \
        if (_threw) { ++g_passed; }                                             \
        else {                                                                  \
            ++g_failed;                                                         \
            std::fprintf(stderr, "  FAIL  %s:%d  did not throw: %s\n",          \
                         __FILE__, __LINE__, #expr);                            \
        }                                                                       \
    } while (0)

// Date
void test_date_basic() {
    std::printf("test_date_basic\n");
    REQUIRE(Date::is_leap(2024));
    REQUIRE(!Date::is_leap(2023));
    REQUIRE(!Date::is_leap(1900));
    REQUIRE(Date::is_leap(2000));
    REQUIRE(Date::days_in_month(2024, 2) == 29);
    REQUIRE(Date::days_in_month(2023, 2) == 28);
    REQUIRE(Date::days_in_month(2026, 4) == 30);
    REQUIRE_THROWS(Date::days_in_month(2026, 13));
}

void test_date_add_months() {
    std::printf("test_date_add_months\n");
    const Date a{2026, 1, 15};
    REQUIRE(a.add_months(1)  == (Date{2026, 2, 15}));
    REQUIRE(a.add_months(12) == (Date{2027, 1, 15}));
    REQUIRE(a.add_months(0)  == a);

    // Day clamp
    const Date jan31{2026, 1, 31};
    REQUIRE(jan31.add_months(1) == (Date{2026, 2, 28}));
    const Date jan31_leap{2024, 1, 31};
    REQUIRE(jan31_leap.add_months(1) == (Date{2024, 2, 29}));

    // Negative / crossing years
    const Date mar1{2026, 3, 15};
    REQUIRE(mar1.add_months(-3) == (Date{2025, 12, 15}));
    REQUIRE(mar1.add_months(-15) == (Date{2024, 12, 15}));
}

// Contract validation

void test_contract_validation() {
    std::printf("test_contract_validation\n");
    REQUIRE_THROWS(Contract("x", -1.0, 0.1, 12, PaymentType::Annuity,
                            Date{2026,1,1}));
    REQUIRE_THROWS(Contract("x", 1000.0, 3.0, 12, PaymentType::Annuity,
                            Date{2026,1,1}));                  // rate > 2.0
    REQUIRE_THROWS(Contract("x", 1000.0, 0.1, 0, PaymentType::Annuity,
                            Date{2026,1,1}));
    REQUIRE_THROWS(Contract("x", 1000.0, 0.1, 12, PaymentType::Annuity,
                            Date{2026,1,1}, /*down=*/1000.0));  // down >= principal

    // Valid construction works.
    Contract c("L-1", 100'000.0, 0.12, 12, PaymentType::Annuity,
               Date{2026, 1, 1}, 20'000.0);
    REQUIRE(c.financed_amount() == 80'000.0);
}

// Annuity schedule

void test_annuity_schedule() {
    std::printf("test_annuity_schedule\n");
    Contract c("L-ANN", 100'000.0, 0.12, 12, PaymentType::Annuity,
               Date{2026, 1, 1});
    LeasingCalculator calc;
    auto schedule = calc.CalculateSchedule(c);

    REQUIRE(schedule.size() == 12);

    const double expected_pmt = 8'884.87887;
    // Every row (except possibly the last) has the same payment.
    for (std::size_t i = 0; i + 1 < schedule.size(); ++i) {
        REQUIRE_NEAR(schedule.entries()[i].payment, expected_pmt, 1e-2);
    }

    // Final balance is exactly zero after our last-period cleanup.
    REQUIRE_NEAR(schedule.entries().back().balance, 0.0, 1e-6);

    // Aggregates vs. closed-form reference.
    REQUIRE_NEAR(schedule.total_principal(), 100'000.0, 1e-6);
    REQUIRE_NEAR(schedule.total_interest(),  6'618.55,  1.0);  // 1 UAH tol
    REQUIRE_NEAR(schedule.total_payments(),  106'618.55, 1.0);

    // Dates advance one month at a time.
    REQUIRE(schedule.entries().front().date == (Date{2026, 2, 1}));
    REQUIRE(schedule.entries().back().date  == (Date{2027, 1, 1}));
}

void test_annuity_zero_rate() {
    std::printf("test_annuity_zero_rate\n");
    // Zero rate — degenerate branch.
    Contract c("L-ZERO", 120'000.0, 0.0, 12, PaymentType::Annuity,
               Date{2026, 1, 1});
    LeasingCalculator calc;
    auto s = calc.CalculateSchedule(c);
    for (const auto& e : s.entries()) {
        REQUIRE_NEAR(e.payment, 10'000.0, 1e-9);
        REQUIRE_NEAR(e.interest, 0.0,     1e-9);
    }
    REQUIRE_NEAR(s.total_interest(), 0.0, 1e-9);
}

// Differential schedule

void test_differential_schedule() {
    std::printf("test_differential_schedule\n");
    Contract c("L-DIFF", 120'000.0, 0.12, 12, PaymentType::Differential,
               Date{2026, 1, 1});
    LeasingCalculator calc;
    auto s = calc.CalculateSchedule(c);

    REQUIRE(s.size() == 12);
    REQUIRE_NEAR(s.entries()[0].payment, 11'200.0, 1e-6);  // 10k + 1'200
    REQUIRE_NEAR(s.entries()[0].interest, 1'200.0, 1e-6);
    REQUIRE_NEAR(s.entries()[11].payment, 10'100.0, 1e-6); // 10k + 100
    REQUIRE_NEAR(s.entries()[11].interest, 100.0, 1e-6);

    REQUIRE_NEAR(s.total_interest(),  7'800.0, 1e-6);
    REQUIRE_NEAR(s.total_principal(), 120'000.0, 1e-6);
    REQUIRE_NEAR(s.entries().back().balance, 0.0, 1e-6);
}

// NPV
void test_npv_flat_series() {
    std::printf("test_npv_flat_series\n");
    // Three monthly cashflows of 1000 at annual 12% (monthly 1%).
    const std::vector<double> cfs{1'000.0, 1'000.0, 1'000.0};
    REQUIRE_NEAR(LeasingCalculator::NPV(cfs, 0.12), 2'940.98457, 1e-3);
}

void test_npv_zero_rate() {
    std::printf("test_npv_zero_rate\n");
    // At rate 0 NPV = sum of cashflows...
    const std::vector<double> cfs{100.0, 200.0, 300.0};
    REQUIRE_NEAR(LeasingCalculator::NPV(cfs, 0.0), 600.0, 1e-9);
}

// IRR

void test_irr_two_payment_example() {
    std::printf("test_irr_two_payment_example\n");
    // Invest 1000 now, receive 525 in month 1 and month 2.
    const std::vector<double> cfs{525.0, 525.0};
    const double irr = LeasingCalculator::IRR(1'000.0, cfs);
    REQUIRE_NEAR(irr, 0.397835, 1e-4);
}

void test_irr_schedule_consistency() {
    std::printf("test_irr_schedule_consistency\n");
    Contract c("L-IRR", 50'000.0, 0.18, 24, PaymentType::Annuity,
               Date{2026, 1, 1});
    LeasingCalculator calc;
    auto s = calc.CalculateSchedule(c);

    std::vector<double> cfs;
    cfs.reserve(s.size());
    for (const auto& e : s.entries()) cfs.push_back(e.payment);

    const double irr = LeasingCalculator::IRR(c.financed_amount(), cfs);
    REQUIRE_NEAR(irr, 0.18, 1e-5);
}

} 

int main() {
    test_date_basic();
    test_date_add_months();
    test_contract_validation();
    test_annuity_schedule();
    test_annuity_zero_rate();
    test_differential_schedule();
    test_npv_flat_series();
    test_npv_zero_rate();
    test_irr_two_payment_example();
    test_irr_schedule_consistency();

    std::printf("\n%d passed, %d failed\n", g_passed, g_failed);
    return g_failed == 0 ? 0 : 1;
}
