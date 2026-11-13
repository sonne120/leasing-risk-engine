#pragma once

#include "ManagedTypes.h"
#include "NativeFacade.h"

namespace LeasingBridge {
using namespace System;
using namespace System::Collections::Generic;

public ref class ScheduleResultManaged {
public:
    property List<PaymentEntryManaged^>^ Entries;
    property double TotalPayments;
    property double TotalInterest;
    property double TotalPrincipal;
};

public ref class LeasingCalculatorManaged {
public:
    // Builds the payment schedule for a contract.
    ScheduleResultManaged^ CalculateSchedule(ContractManaged^ contract)
    {
        auto native = leasing::bridge::compute_schedule(*contract->Native());

        auto result = gcnew ScheduleResultManaged();
        result->Entries = gcnew List<PaymentEntryManaged^>(
            static_cast<int>(native.size()));

        for (const auto& e : native.entries()) {
            auto row = gcnew PaymentEntryManaged();
            row->MonthNumber = e.month_number;
            row->Date        = DateTime(e.date.year, e.date.month, e.date.day);
            row->Payment     = e.payment;
            row->Principal   = e.principal;
            row->Interest    = e.interest;
            row->Balance     = e.balance;
            result->Entries->Add(row);
        }

        result->TotalPayments  = native.total_payments();
        result->TotalInterest  = native.total_interest();
        result->TotalPrincipal = native.total_principal();
        return result;
    }

    // Flat NPV over a monthly cashflow stream.
    double NPV(array<double>^ monthlyCashflows, double annualRate)
    {
        std::vector<double> cfs(static_cast<std::size_t>(monthlyCashflows->Length));
        for (int i = 0; i < monthlyCashflows->Length; ++i) {
            cfs[static_cast<std::size_t>(i)] = monthlyCashflows[i];
        }
        return leasing::bridge::compute_npv(cfs, annualRate);
    }

    // IRR with the native bisection defaults.
    double IRR(double initialOutflow, array<double>^ monthlyCashflows)
    {
        std::vector<double> cfs(static_cast<std::size_t>(monthlyCashflows->Length));
        for (int i = 0; i < monthlyCashflows->Length; ++i) {
            cfs[static_cast<std::size_t>(i)] = monthlyCashflows[i];
        }
        return leasing::bridge::compute_irr(initialOutflow, cfs);
    }
};

} // namespace LeasingBridge
