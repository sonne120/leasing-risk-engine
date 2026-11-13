#pragma once

#include "Date.h"
#include <stdexcept>
#include <string>

namespace leasing::core {
enum class PaymentType {
    Annuity,
    Differential,
};

class Contract {
public:
    Contract(std::string number,
             double      principal,
             double      annual_rate,
             int         term_months,
             PaymentType type,
             Date        start_date,
             double      down_payment = 0.0)
        : number_(std::move(number)),
          principal_(principal),
          annual_rate_(annual_rate),
          term_months_(term_months),
          type_(type),
          start_date_(start_date),
          down_payment_(down_payment)
    {
        validate();
    }

    const std::string& number()       const noexcept { return number_;       }
    double             principal()    const noexcept { return principal_;    }
    double             annual_rate()  const noexcept { return annual_rate_;  }
    int                term_months()  const noexcept { return term_months_;  }
    PaymentType        type()         const noexcept { return type_;         }
    Date               start_date()   const noexcept { return start_date_;   }
    double             down_payment() const noexcept { return down_payment_; }

    // Principal net of the down payment — the amount actually financed.
    double financed_amount() const noexcept {
        return principal_ - down_payment_;
    }

    void set_down_payment(double v) {
        down_payment_ = v;
        validate();
    }

private:
    void validate() const {
        if (principal_ <= 0.0) {
            throw std::invalid_argument("principal must be positive");
        }
        if (annual_rate_ < 0.0 || annual_rate_ > 2.0) {
            throw std::invalid_argument(
                "annual_rate must be in [0, 2.0] (as a decimal, e.g. 0.18 for 18%)");
        }
        if (term_months_ <= 0 || term_months_ > 600) {
            throw std::invalid_argument(
                "term_months must be in (0, 600]");
        }
        if (down_payment_ < 0.0 || down_payment_ >= principal_) {
            throw std::invalid_argument(
                "down_payment must be in [0, principal)");
        }
    }

    std::string number_;
    double      principal_;
    double      annual_rate_;
    int         term_months_;
    PaymentType type_;
    Date        start_date_;
    double      down_payment_;
};

} 
