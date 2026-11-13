using System;
using System.Windows.Threading;
using LeasingRiskEngine.WpfUi.Services;
using Prism.Mvvm;

namespace LeasingRiskEngine.WpfUi.ViewModels;

public class MainWindowViewModel : BindableBase
{
    private readonly DispatcherTimer _clock;

    private string _statusText = "Ready";
    private string _nativeCoreInfo = "LeasingBridge.dll (native core)";
    private DateTime _currentTime = DateTime.Now;

    public string StatusText
    {
        get => _statusText;
        set => SetProperty(ref _statusText, value);
    }

    public string NativeCoreInfo
    {
        get => _nativeCoreInfo;
        set => SetProperty(ref _nativeCoreInfo, value);
    }

    public DateTime CurrentTime
    {
        get => _currentTime;
        set => SetProperty(ref _currentTime, value);
    }

    public MainWindowViewModel(IPipelineService pipelineService)
    {
        _clock = new DispatcherTimer { Interval = TimeSpan.FromSeconds(1) };
        _clock.Tick += (_, _) => CurrentTime = DateTime.Now;
        _clock.Start();

        pipelineService.BusyChanged += busy =>
        {
            StatusText = busy ? "Simulation..." : "Ready";
        };
    }

    public void OnNavigated(string navigationKey)
    {
        StatusText = navigationKey switch
        {
            "ContractsView"       => "Contract calculation",
            "MonteCarloView"      => "Monte Carlo simulation",
            "RiskDashboardView"   => "Risk metrics",
            "StressTestsView"     => "Stress tests",
            "HistoryView"         => "Simulation history",
            _                     => "Ready",
        };
    }
}
