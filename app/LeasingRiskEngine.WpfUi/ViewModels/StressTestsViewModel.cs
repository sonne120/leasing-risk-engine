using System;
using System.Collections.ObjectModel;
using System.Globalization;
using System.Text.Json;
using System.Threading.Tasks;
using LeasingRiskEngine.Data;
using LeasingRiskEngine.WpfUi.Models;
using LeasingRiskEngine.WpfUi.Services;
using LeasingBridge;
using Prism.Commands;

namespace LeasingRiskEngine.WpfUi.ViewModels;

public sealed class StressTestsViewModel : ViewModelBase
{
    private readonly IRiskService     _risk;
    private readonly IRunHistoryStore _history;

    public HullWhiteInputModel HullWhite { get; } = new();
    public GbmInputModel       Gbm       { get; } = new();

    public StressKind[]        Kinds   { get; } =
        { StressKind.HullWhiteParallelShift, StressKind.GbmVolShock };
    public SchemeKindManaged[] Schemes { get; } =
        { SchemeKindManaged.Euler, SchemeKindManaged.Milstein };

    private StressKind        _kind          = StressKind.HullWhiteParallelShift;
    private SchemeKindManaged _scheme        = SchemeKindManaged.Euler;
    private double            _shiftBps      = 100.0;
    private double            _volMultiplier = 1.5;
    private int               _steps         = 100;
    private int               _paths         = 20_000;
    private int               _threads       = 0;
    private ulong             _seed          = 0xC0FFEEUL;

    public StressKind Kind
    {
        get => _kind;
        set { if (SetProperty(ref _kind, value)) {
            RaisePropertyChanged(nameof(ShowShift));
            RaisePropertyChanged(nameof(ShowVol));
        } }
    }
    public SchemeKindManaged Scheme        { get => _scheme;        set => SetProperty(ref _scheme, value); }
    public double           ShiftBps       { get => _shiftBps;      set => SetProperty(ref _shiftBps, value); }
    public double           VolMultiplier  { get => _volMultiplier; set => SetProperty(ref _volMultiplier, value); }
    public int              Steps          { get => _steps;         set => SetProperty(ref _steps, value); }
    public int              Paths          { get => _paths;         set => SetProperty(ref _paths, value); }
    public int              Threads        { get => _threads;       set => SetProperty(ref _threads, value); }
    public ulong            Seed           { get => _seed;          set => SetProperty(ref _seed, value); }

    public bool ShowShift => Kind == StressKind.HullWhiteParallelShift;
    public bool ShowVol   => Kind == StressKind.GbmVolShock;

    private bool _isBusy;
    private string? _errorMessage;

    public bool IsBusy
    {
        get => _isBusy;
        private set { if (SetProperty(ref _isBusy, value)) {
            RunCommand.RaiseCanExecuteChanged();
            ClearCommand.RaiseCanExecuteChanged();
        } }
    }
    public string? ErrorMessage
    {
        get => _errorMessage;
        private set { if (SetProperty(ref _errorMessage, value))
            RaisePropertyChanged(nameof(HasError)); }
    }
    public bool HasError => !string.IsNullOrEmpty(_errorMessage);

    public ObservableCollection<StressRow> Rows { get; } = new();

    public DelegateCommand RunCommand   { get; }
    public DelegateCommand ClearCommand { get; }

    public StressTestsViewModel(IRiskService risk, IRunHistoryStore history)
    {
        _risk        = risk;
        _history     = history;
        RunCommand   = new DelegateCommand(async () => await RunAsync(),
                                            () => !IsBusy);
        ClearCommand = new DelegateCommand(() => Rows.Clear(),
                                            () => !IsBusy);
    }

    private async Task RunAsync()
    {
        IsBusy = true;
        ErrorMessage = null;

        // Captured now: the inputs can be edited while the run is in flight.
        var record = new SimulationRunRecord
        {
            Kind   = "Stress",
            Scheme = Scheme.ToString(),
            Steps  = Steps,
            Paths  = Paths,
        };
        object model = Kind == StressKind.HullWhiteParallelShift
            ? HullWhite.ToNative() : Gbm.ToNative();
        var shock   = Kind == StressKind.HullWhiteParallelShift
            ? ShiftBps : VolMultiplier;
        var seed    = Seed;
        var threads = Threads;

        try
        {
            var row = await Task.Run(RunNative);
            Rows.Insert(0, row);

            record.Scenario = row.Scenario;
            record.Mean     = row.ShiftedMean;
            record.Delta    = row.Delta;
            record.ParametersJson = JsonSerializer.Serialize(new
            {
                Model = model,
                Shock = shock,
                row.BaselineMean,
                Seed = seed,
                Threads = threads,
            }, RunJson.Options);
        }
        catch (Exception ex)
        {
            ErrorMessage = ex.Message;
            return;
        }
        finally
        {
            IsBusy = false;
        }

        try
        {
            await _history.AddAsync(record);
        }
        catch (Exception ex)
        {
            ErrorMessage = RunJson.SaveFailed(ex);
        }
    }

    private StressRow RunNative()
    {
        switch (Kind)
        {
            case StressKind.HullWhiteParallelShift:
            {
                var r = _risk.HullWhiteParallelShift(
                    HullWhite.ToNative(), ShiftBps, Steps, Paths,
                    Scheme, Threads, Seed);
                return new StressRow
                {
                    Timestamp     = DateTime.Now,
                    Scenario      = string.Format(CultureInfo.InvariantCulture,
                                       "HW parallel shift {0:+0;-0;0} bps", ShiftBps),
                    BaselineMean  = r.BaselineMean,
                    ShiftedMean   = r.ShiftedMean,
                    Delta         = r.Delta,
                    DeltaPercent  = r.DeltaPercent,
                };
            }

            case StressKind.GbmVolShock:
            default:
            {
                var r = _risk.GbmVolShock(
                    Gbm.ToNative(), VolMultiplier, Steps, Paths,
                    Scheme, Threads, Seed);
                return new StressRow
                {
                    Timestamp     = DateTime.Now,
                    Scenario      = string.Format(CultureInfo.InvariantCulture,
                                       "GBM σ × {0:F2}", VolMultiplier),
                    BaselineMean  = r.BaselineMean,
                    ShiftedMean   = r.ShiftedMean,
                    Delta         = r.Delta,
                    DeltaPercent  = r.DeltaPercent,
                };
            }
        }
    }
}
