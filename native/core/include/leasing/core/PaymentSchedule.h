#pragma once

#include "Date.h"
#include <cstddef>
#include <vector>

namespace leasing::core {
struct PaymentEntry {
    int    month_number = 0;
    Date   date{};
    double payment   = 0.0;     // total paid this month
    double principal = 0.0;     // share that goes against principal
    double interest  = 0.0;     // share that is interest
    double balance   = 0.0;     // remaining principal AFTER this payment
};

class PaymentSchedule {
public:
    void add(PaymentEntry e) { entries_.push_back(e); }

    const std::vector<PaymentEntry>& entries() const noexcept { return entries_; }

    std::size_t size() const noexcept { return entries_.size(); }
    bool        empty() const noexcept { return entries_.empty(); }

    double total_payments() const noexcept {
        double s = 0.0;
        for (const auto& e : entries_) s += e.payment;
        return s;
    }

    double total_interest() const noexcept {
        double s = 0.0;
        for (const auto& e : entries_) s += e.interest;
        return s;
    }

    double total_principal() const noexcept {
        double s = 0.0;
        for (const auto& e : entries_) s += e.principal;
        return s;
    }

    void reserve(std::size_t n) { entries_.reserve(n); }
    void clear()                 { entries_.clear(); }

private:
    std::vector<PaymentEntry> entries_;
};

} 
