#pragma once

#include <leasing/core/Contract.h>
#include <leasing/core/PaymentSchedule.h>

#include <msclr/marshal_cppstd.h>

namespace LeasingBridge {
using namespace System;
using namespace System::Collections::Generic;

public enum class PaymentTypeManaged {
    Annuity      = 0,
    Differential = 1,
};

public value struct DateManaged {
    int Year;
    int Month;
    int Day;

    DateManaged(int y, int m, int d) : Year(y), Month(m), Day(d) {}

    static DateTime op_Implicit(DateManaged d) {
        return DateTime(d.Year, d.Month, d.Day);
    }
    static DateManaged op_Implicit(DateTime dt) {
        return DateManaged(dt.Year, dt.Month, dt.Day);
    }

    String^ ToIso() {
        return String::Format("{0:D4}-{1:D2}-{2:D2}", Year, Month, Day);
    }
};

// One row of the amortisation schedule.
public ref class PaymentEntryManaged {
public:
    property int      MonthNumber;
    property DateTime Date;
    property double   Payment;
    property double   Principal;
    property double   Interest;
    property double   Balance;
};

// Strongly-typed wrapper around leasing::core::Contract.
public ref class ContractManaged {
public:
    ContractManaged(String^ number,
                    double principal,
                    double annualRate,
                    int    termMonths,
                    PaymentTypeManaged type,
                    DateTime startDate,
                    double downPayment)
    {
        msclr::interop::marshal_context ctx;
        std::string native_number = ctx.marshal_as<std::string>(number);

        leasing::core::Date d{ startDate.Year, startDate.Month, startDate.Day };
        try {
            m_native = new leasing::core::Contract(
                native_number, principal, annualRate, termMonths,
                static_cast<leasing::core::PaymentType>(type),
                d, downPayment);
        }
        catch (const std::exception& ex) {
            throw gcnew ArgumentException(gcnew String(ex.what()));
        }
    }

    ~ContractManaged() { this->!ContractManaged(); }
    !ContractManaged() { delete m_native; m_native = nullptr; }

    property String^ Number {
        String^ get() {
            return gcnew String(m_native->number().c_str());
        }
    }
    property double   Principal   { double   get() { return m_native->principal(); } }
    property double   AnnualRate  { double   get() { return m_native->annual_rate(); } }
    property int      TermMonths  { int      get() { return m_native->term_months(); } }
    property PaymentTypeManaged Type {
        PaymentTypeManaged get() {
            return static_cast<PaymentTypeManaged>(m_native->type());
        }
    }
    property DateTime StartDate {
        DateTime get() {
            auto d = m_native->start_date();
            return DateTime(d.year, d.month, d.day);
        }
    }
    property double   DownPayment { double   get() { return m_native->down_payment(); } }
    property double   FinancedAmount {
        double get() { return m_native->financed_amount(); }
    }

    leasing::core::Contract* Native() { return m_native; }

private:
    leasing::core::Contract* m_native;
};

} 
