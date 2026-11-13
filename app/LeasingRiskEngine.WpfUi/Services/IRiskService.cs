using LeasingBridge;

namespace LeasingRiskEngine.WpfUi.Services;

public interface IRiskService
{
    RiskRunResultManaged ZeroBondLoss(
        HullWhiteParams p, int steps, int paths, SchemeKindManaged scheme,
        int threads, ulong seed);

    RiskRunResultManaged ResidualValueLoss(
        GbmParams p, int steps, int paths, SchemeKindManaged scheme,
        int threads, ulong seed);

    StressResultManaged HullWhiteParallelShift(
        HullWhiteParams p, double shiftBps, int steps, int paths,
        SchemeKindManaged scheme, int threads, ulong seed);

    StressResultManaged GbmVolShock(
        GbmParams p, double volMultiplier, int steps, int paths,
        SchemeKindManaged scheme, int threads, ulong seed);
}

public sealed class RiskService : IRiskService
{
    private readonly RiskEngineManaged     _risk  = new();
    private readonly ScenarioEngineManaged _scen  = new();

    public RiskRunResultManaged ZeroBondLoss(
        HullWhiteParams p, int steps, int paths, SchemeKindManaged scheme,
        int threads, ulong seed) =>
        _risk.ZeroBondLoss(p, steps, paths, scheme, threads, seed);

    public RiskRunResultManaged ResidualValueLoss(
        GbmParams p, int steps, int paths, SchemeKindManaged scheme,
        int threads, ulong seed) =>
        _risk.ResidualValueLoss(p, steps, paths, scheme, threads, seed);

    public StressResultManaged HullWhiteParallelShift(
        HullWhiteParams p, double shiftBps, int steps, int paths,
        SchemeKindManaged scheme, int threads, ulong seed) =>
        _scen.HullWhiteParallelShift(p, shiftBps, steps, paths,
                                      scheme, threads, seed);

    public StressResultManaged GbmVolShock(
        GbmParams p, double volMultiplier, int steps, int paths,
        SchemeKindManaged scheme, int threads, ulong seed) =>
        _scen.GbmVolShock(p, volMultiplier, steps, paths,
                           scheme, threads, seed);
}
