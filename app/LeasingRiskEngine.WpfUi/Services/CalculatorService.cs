using System;
using System.Collections.Generic;
using LeasingBridge;

namespace LeasingRiskEngine.WpfUi.Services;

public sealed class CalculatorService : ICalculatorService
{
    private readonly LeasingCalculatorManaged _calc = new();

    public ScheduleResultManaged CalculateSchedule(
        string number, double principal, double annualRate,
        int termMonths, PaymentTypeManaged type,
        DateTime startDate, double downPayment)
    {
        using var contract = new ContractManaged(
            number, principal, annualRate, termMonths,
            type, startDate, downPayment);
        return _calc.CalculateSchedule(contract);
    }

    public double NPV(IReadOnlyList<double> cashflows, double annualRate)
    {
        var arr = new double[cashflows.Count];
        for (int i = 0; i < arr.Length; ++i) arr[i] = cashflows[i];
        return _calc.NPV(arr, annualRate);
    }

    public double IRR(double initialOutflow, IReadOnlyList<double> cashflows)
    {
        var arr = new double[cashflows.Count];
        for (int i = 0; i < arr.Length; ++i) arr[i] = cashflows[i];
        return _calc.IRR(initialOutflow, arr);
    }
}
