using LeasingBridge;
using Prism.Mvvm;

namespace LeasingRiskEngine.WpfUi.Models;

public enum PayoffKind
{
    DiscountBond,
    GbmTerminal,
}

public sealed class HullWhiteInputModel : BindableBase
{
    private double _a          = 0.10;
    private double _theta      = 0.15;
    private double _sigma      = 0.015;
    private double _r0         = 0.185;
    private double _expiration = 3.0;

    public double A          { get => _a;          set => SetProperty(ref _a, value); }
    public double Theta      { get => _theta;      set => SetProperty(ref _theta, value); }
    public double Sigma      { get => _sigma;      set => SetProperty(ref _sigma, value); }
    public double R0         { get => _r0;         set => SetProperty(ref _r0, value); }
    public double Expiration { get => _expiration; set => SetProperty(ref _expiration, value); }

    public HullWhiteParams ToNative() => new()
    {
        A = A, Theta = Theta, Sigma = Sigma, R0 = R0, Expiration = Expiration
    };
}

public sealed class GbmInputModel : BindableBase
{
    private double _mu         = 0.05;
    private double _sigma      = 0.20;
    private double _s0         = 100.0;
    private double _expiration = 1.0;

    public double Mu         { get => _mu;         set => SetProperty(ref _mu, value); }
    public double Sigma      { get => _sigma;      set => SetProperty(ref _sigma, value); }
    public double S0         { get => _s0;         set => SetProperty(ref _s0, value); }
    public double Expiration { get => _expiration; set => SetProperty(ref _expiration, value); }

    public GbmParams ToNative() => new()
    {
        Mu = Mu, Sigma = Sigma, S0 = S0, Expiration = Expiration
    };
}
