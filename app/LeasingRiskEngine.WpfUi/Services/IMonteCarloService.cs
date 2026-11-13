using LeasingBridge;

namespace LeasingRiskEngine.WpfUi.Services;

public interface IMonteCarloService
{
    SimResultManaged PriceDiscountBond(
        HullWhiteParams p, int steps, int paths, SchemeKindManaged scheme,
        bool antithetic, int threads, ulong seed);

    SimResultManaged PriceEuropeanCall(
        GbmParams p, double strike, double discountRate,
        int steps, int paths, SchemeKindManaged scheme,
        bool antithetic, int threads, ulong seed);

    SimResultManaged TerminalMeanGbm(
        GbmParams p, int steps, int paths, SchemeKindManaged scheme,
        bool antithetic, int threads, ulong seed);
}

public sealed class MonteCarloService : IMonteCarloService
{
    private readonly MonteCarloEngineManaged _engine = new();

    public SimResultManaged PriceDiscountBond(
        HullWhiteParams p, int steps, int paths, SchemeKindManaged scheme,
        bool antithetic, int threads, ulong seed) =>
        _engine.PriceDiscountBond(p, steps, paths, scheme, antithetic, threads, seed);

    public SimResultManaged PriceEuropeanCall(
        GbmParams p, double strike, double discountRate,
        int steps, int paths, SchemeKindManaged scheme,
        bool antithetic, int threads, ulong seed) =>
        _engine.PriceEuropeanCall(p, strike, discountRate, steps, paths,
                                  scheme, antithetic, threads, seed);

    public SimResultManaged TerminalMeanGbm(
        GbmParams p, int steps, int paths, SchemeKindManaged scheme,
        bool antithetic, int threads, ulong seed) =>
        _engine.TerminalMeanGbm(p, steps, paths, scheme, antithetic, threads, seed);
}
