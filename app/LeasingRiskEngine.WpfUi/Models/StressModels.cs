using System;

namespace LeasingRiskEngine.WpfUi.Models;

public enum StressKind
{
    HullWhiteParallelShift,
    GbmVolShock,
}

public sealed class StressRow
{
    public DateTime Timestamp     { get; init; }
    public string   Scenario      { get; init; } = "";
    public double   BaselineMean  { get; init; }
    public double   ShiftedMean   { get; init; }
    public double   Delta         { get; init; }
    public double   DeltaPercent  { get; init; }
}
