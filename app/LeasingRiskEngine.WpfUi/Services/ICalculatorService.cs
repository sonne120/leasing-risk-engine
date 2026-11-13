using System;
using System.Collections.Generic;
using LeasingBridge;

namespace LeasingRiskEngine.WpfUi.Services;

public interface ICalculatorService
{
    ScheduleResultManaged CalculateSchedule(
        string number, double principal, double annualRate,
        int termMonths, PaymentTypeManaged type,
        DateTime startDate, double downPayment);

    double NPV(IReadOnlyList<double> cashflows, double annualRate);
    double IRR(double initialOutflow, IReadOnlyList<double> cashflows);
}
