#pragma once

#include "Contract.h"
#include "PaymentSchedule.h"
#include <vector>

namespace leasing::core {
class LeasingCalculator {
public:
    PaymentSchedule CalculateSchedule(const Contract& contract) const;

    // NPV of a cashflow stream against a flat annual rate.
    static double NPV(const std::vector<double>& monthly_cashflows,
                      double annual_rate);

    static double NPV_with_initial(double initial_outflow,
                                   const PaymentSchedule& schedule,
                                   double annual_rate);

    static double IRR(double initial_outflow,
                      const std::vector<double>& monthly_cashflows,
                      double low  = -0.99,
                      double high = 10.0,
                      double tol  = 1e-9,
                      int    max_iter = 200);
};

} 
