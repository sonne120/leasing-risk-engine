#pragma once

#include <array>
#include <cstdint>
#include <stdexcept>
#include <string>

namespace leasing::core {
struct Date {
    int year  = 1970;
    int month = 1;
    int day   = 1;

    static bool is_leap(int y) noexcept {
        return (y % 4 == 0 && y % 100 != 0) || (y % 400 == 0);
    }

    static int days_in_month(int y, int m) {
        constexpr std::array<int, 12> kDays{
            31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
        if (m < 1 || m > 12) {
            throw std::invalid_argument("month out of range");
        }
        int d = kDays[static_cast<std::size_t>(m - 1)];
        if (m == 2 && is_leap(y)) d = 29;
        return d;
    }

    // Adds `months` (can be negative), then clamps the day.
    Date add_months(int months) const {
        int total = (year * 12 + (month - 1)) + months;
        int new_year  = total / 12;
        int new_month = total % 12 + 1;
        if (total < 0 && total % 12 != 0) {
            new_year  -= 1;
            new_month += 12;
        }
        int new_day = day;
        const int dim = days_in_month(new_year, new_month);
        if (new_day > dim) new_day = dim;
        return Date{new_year, new_month, new_day};
    }

    bool operator==(const Date&) const = default;

    // ISO 8601 "YYYY-MM-DD" — used by the WPF bridge.
    std::string to_iso() const {
        char buf[11];
        std::snprintf(buf, sizeof(buf), "%04d-%02d-%02d", year, month, day);
        return buf;
    }
};

} 
