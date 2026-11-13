#include "leasing/core/LeasingCalculator.h"

#include <cmath>
#include <limits>
#include <stdexcept>

namespace leasing::core {
PaymentSchedule
LeasingCalculator::CalculateSchedule(const Contract& contract) const
{
    const int    n          = contract.term_months();
    const double principal  = contract.financed_amount();
    const double monthly_r  = contract.annual_rate() / 12.0;

    PaymentSchedule schedule;
    schedule.reserve(static_cast<std::size_t>(n));

    Date date = contract.start_date();

    if (contract.type() == PaymentType::Annuity) {
        // PMT = P * r / (1 - (1+r)^(-n))
        const double payment =
            (monthly_r == 0.0)
                ? principal / n
                : principal * monthly_r /
                      (1.0 - std::pow(1.0 + monthly_r, -n));

        double balance = principal;
        for (int m = 1; m <= n; ++m) {
            date = date.add_months(1);
            const double interest      = balance * monthly_r;
            double       principal_part = payment - interest;
            // Last period
            if (m == n) {
                principal_part = balance;
            }
            balance -= principal_part;
            schedule.add({
                m, date,
                interest + principal_part,  // = payment (unless last period)
                principal_part,
                interest,
                balance
            });
        }
    } else {
        // Differential
        const double principal_part = principal / n;
        double       balance        = principal;
        for (int m = 1; m <= n; ++m) {
            date = date.add_months(1);
            const double interest = balance * monthly_r;
            double       pp       = principal_part;
            if (m == n) pp = balance;    // soak up rounding
            balance -= pp;
            schedule.add({
                m, date,
                pp + interest,
                pp,
                interest,
                balance
            });
        }
    }

    return schedule;
}

double
LeasingCalculator::NPV(const std::vector<double>& monthly_cashflows,
                       double annual_rate)
{
    const double r = annual_rate / 12.0;
    const double one_plus_r = 1.0 + r;
    double npv    = 0.0;
    double factor = one_plus_r;             // (1+r)^1 at month 1
    for (double cf : monthly_cashflows) {
        npv    += cf / factor;
        factor *= one_plus_r;
    }
    return npv;
}

double
LeasingCalculator::NPV_with_initial(double initial_outflow,
                                    const PaymentSchedule& schedule,
                                    double annual_rate)
{
    std::vector<double> cfs;
    cfs.reserve(schedule.size());
    for (const auto& e : schedule.entries()) {
        cfs.push_back(e.payment);
    }
    return -initial_outflow + NPV(cfs, annual_rate);
}

double
LeasingCalculator::IRR(double initial_outflow,
                       const std::vector<double>& monthly_cashflows,
                       double low, double high,
                       double tol, int max_iter)
{
    auto f = [&](double rate) {
        return -initial_outflow + NPV(monthly_cashflows, rate);
    };

    double f_low  = f(low);
    double f_high = f(high);

    if ((f_low > 0 && f_high > 0) || (f_low < 0 && f_high < 0)) {
        return std::numeric_limits<double>::quiet_NaN();  // no sign change
    }

    for (int i = 0; i < max_iter; ++i) {
        const double mid   = 0.5 * (low + high);
        const double f_mid = f(mid);
        if (std::fabs(f_mid) < tol || (high - low) < tol) {
            return mid;
        }
        if ((f_low > 0 && f_mid > 0) || (f_low < 0 && f_mid < 0)) {
            low   = mid;
            f_low = f_mid;
        } else {
            high   = mid;
            f_high = f_mid;
        }
    }
    return 0.5 * (low + high);
}

}
