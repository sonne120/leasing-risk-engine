using System;
using System.Collections.Generic;

namespace LeasingRiskEngine.Data;

public sealed class Customer
{
    public int      Id         { get; set; }
    public string   Name       { get; set; } = "";
    public DateTime CreatedUtc { get; set; }

    public List<ContractRecord> Contracts { get; set; } = new();
}

public sealed class ContractRecord
{
    public int      Id          { get; set; }
    public int      CustomerId  { get; set; }
    public Customer Customer    { get; set; } = null!;

    public string   Number      { get; set; } = "";
    public double   Principal   { get; set; }
    public double   DownPayment { get; set; }
    public double   AnnualRate  { get; set; }
    public int      TermMonths  { get; set; }
    public int      PaymentType { get; set; }
    public DateTime StartDate   { get; set; }

    // Results of the last calculation, kept so lists need no recalculation.
    public double   MonthlyPayment { get; set; }
    public double   TotalPayments  { get; set; }
    public double   TotalInterest  { get; set; }

    public DateTime SavedUtc { get; set; }
}

/// <summary>One finished Monte Carlo, risk or stress run.</summary>
public sealed class SimulationRunRecord
{
    public int      Id         { get; set; }
    public DateTime CreatedUtc { get; set; }

    /// <summary>"MonteCarlo", "Risk" or "Stress".</summary>
    public string   Kind     { get; set; } = "";
    public string   Scenario { get; set; } = "";
    public string   Scheme   { get; set; } = "";
    public int      Steps    { get; set; }
    public int      Paths    { get; set; }

    public double   Mean     { get; set; }
    public double?  StdError { get; set; }
    public double?  Var95    { get; set; }
    public double?  Var99    { get; set; }
    /// <summary>Shifted minus baseline, stress runs only.</summary>
    public double?  Delta    { get; set; }

    /// <summary>Model parameters and seed as JSON, for reproducing the run.</summary>
    public string   ParametersJson { get; set; } = "{}";

    public DateTime CreatedLocal => CreatedUtc.ToLocalTime();
}
