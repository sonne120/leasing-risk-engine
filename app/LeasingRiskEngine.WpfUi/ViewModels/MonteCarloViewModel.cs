using System;
using System.Collections.ObjectModel;
using System.Threading.Tasks;
using LeasingRiskEngine.WpfUi.Models;
using LeasingRiskEngine.WpfUi.Services;
using LeasingBridge;
using LiveChartsCore;
using LiveChartsCore.Defaults;
using LiveChartsCore.Kernel.Sketches;
using LiveChartsCore.SkiaSharpView;
using LiveChartsCore.SkiaSharpView.Painting;
using LiveChartsCore.SkiaSharpView.Painting.Effects;
using Prism.Commands;
using SkiaSharp;

namespace LeasingRiskEngine.WpfUi.ViewModels;

public sealed class MonteCarloViewModel : ViewModelBase, IDisposable
{
    private readonly IPipelineService _pipeline;

    public HullWhiteInputModel HullWhite { get; } = new();
    public GbmInputModel       Gbm       { get; } = new();

    public PayoffKind[] PayoffKinds { get; } =
        { PayoffKind.DiscountBond, PayoffKind.GbmTerminal };
    public SchemeKindManaged[] Schemes { get; } =
        { SchemeKindManaged.Euler, SchemeKindManaged.Milstein };

    private PayoffKind        _selectedPayoff = PayoffKind.DiscountBond;
    private SchemeKindManaged _scheme         = SchemeKindManaged.Euler;
    private int               _steps          = 252;
    private int               _paths          = 100_000;
    private int               _threads        = 0;
    private int               _progressEveryMs = 50;
    private ulong             _seed           = 0xC0FFEEUL;

    public PayoffKind SelectedPayoff
    {
        get => _selectedPayoff;
        set { if (SetProperty(ref _selectedPayoff, value)) {
            RaisePropertyChanged(nameof(ShowHullWhite));
            RaisePropertyChanged(nameof(ShowGbm));
        } }
    }
    public SchemeKindManaged Scheme
    {
        get => _scheme; set => SetProperty(ref _scheme, value);
    }
    public int Steps           { get => _steps;           set => SetProperty(ref _steps, value); }
    public int Paths           { get => _paths;           set => SetProperty(ref _paths, value); }
    public int Threads         { get => _threads;         set => SetProperty(ref _threads, value); }
    public int ProgressEveryMs { get => _progressEveryMs; set => SetProperty(ref _progressEveryMs, value); }
    public ulong Seed          { get => _seed;            set => SetProperty(ref _seed, value); }

    public bool ShowHullWhite => SelectedPayoff == PayoffKind.DiscountBond;
    public bool ShowGbm       => SelectedPayoff == PayoffKind.GbmTerminal;

    private bool   _isBusy;
    private int    _pathsDone;
    private int    _pathsTotal;
    private double _runningMean;
    private double _runningStdErr;
    private double _ci95Low;
    private double _ci95High;
    private double _elapsedMs;
    private SimResultManaged? _finalResult;
    private string? _errorMessage;

    public bool IsBusy
    {
        get => _isBusy;
        private set
        {
            if (SetProperty(ref _isBusy, value))
            {
                RunCommand.RaiseCanExecuteChanged();
                CancelCommand.RaiseCanExecuteChanged();
                RaisePropertyChanged(nameof(ProgressPercent));
            }
        }
    }

    public int PathsDone
    {
        get => _pathsDone;
        private set
        {
            if (SetProperty(ref _pathsDone, value))
                RaisePropertyChanged(nameof(ProgressPercent));
        }
    }
    public int PathsTotal
    {
        get => _pathsTotal;
        private set
        {
            if (SetProperty(ref _pathsTotal, value))
                RaisePropertyChanged(nameof(ProgressPercent));
        }
    }
    public double ProgressPercent =>
        PathsTotal > 0 ? 100.0 * PathsDone / PathsTotal : 0.0;

    public double RunningMean   { get => _runningMean;   private set => SetProperty(ref _runningMean, value); }
    public double RunningStdErr { get => _runningStdErr; private set => SetProperty(ref _runningStdErr, value); }
    public double Ci95Low       { get => _ci95Low;       private set => SetProperty(ref _ci95Low, value); }
    public double Ci95High      { get => _ci95High;      private set => SetProperty(ref _ci95High, value); }
    public double ElapsedMs     { get => _elapsedMs;     private set => SetProperty(ref _elapsedMs, value); }

    public SimResultManaged? FinalResult
    {
        get => _finalResult;
        private set
        {
            if (SetProperty(ref _finalResult, value))
                RaisePropertyChanged(nameof(HasFinalResult));
        }
    }
    public bool HasFinalResult => _finalResult != null;

    public string? ErrorMessage
    {
        get => _errorMessage;
        private set
        {
            if (SetProperty(ref _errorMessage, value))
                RaisePropertyChanged(nameof(HasError));
        }
    }
    public bool HasError => !string.IsNullOrEmpty(_errorMessage);

    public ObservableCollection<ObservablePoint> MeanHistory     { get; } = new();
    public ObservableCollection<ObservablePoint> Ci95LowHistory  { get; } = new();
    public ObservableCollection<ObservablePoint> Ci95HighHistory { get; } = new();

    public ISeries[]         Series { get; }
    public ICartesianAxis[]  XAxes  { get; }
    public ICartesianAxis[]  YAxes  { get; }

    public DelegateCommand RunCommand    { get; }
    public DelegateCommand CancelCommand { get; }

    private readonly Action<SnapshotManaged> _progressHandler;
    private readonly Action<bool>            _busyHandler;

    public MonteCarloViewModel(IPipelineService pipeline)
    {
        _pipeline = pipeline;

        _progressHandler = OnProgress;
        _busyHandler     = busy => IsBusy = busy;
        _pipeline.Progress    += _progressHandler;
        _pipeline.BusyChanged += _busyHandler;

        RunCommand    = new DelegateCommand(async () => await RunAsync(),
                                             () => !IsBusy);
        CancelCommand = new DelegateCommand(() => _pipeline.Cancel(),
                                             () => IsBusy);

        Series = new ISeries[]
        {
            new LineSeries<ObservablePoint>
            {
                Name = "95% CI low",
                Values = Ci95LowHistory,
                Stroke = new SolidColorPaint(SKColors.LightGray, 1)
                {
                    PathEffect = new DashEffect(new float[] { 4, 4 }),
                },
                Fill = null,
                GeometrySize = 0,
                LineSmoothness = 0.2,
            },
            new LineSeries<ObservablePoint>
            {
                Name = "Running mean",
                Values = MeanHistory,
                Stroke = new SolidColorPaint(SKColor.Parse("#378ADD"), 2),
                Fill = null,
                GeometrySize = 0,
                LineSmoothness = 0.2,
            },
            new LineSeries<ObservablePoint>
            {
                Name = "95% CI high",
                Values = Ci95HighHistory,
                Stroke = new SolidColorPaint(SKColors.LightGray, 1)
                {
                    PathEffect = new DashEffect(new float[] { 4, 4 }),
                },
                Fill = null,
                GeometrySize = 0,
                LineSmoothness = 0.2,
            },
        };

        XAxes = new ICartesianAxis[] { new Axis { Name = "Paths processed" } };
        YAxes = new ICartesianAxis[] { new Axis { Name = "Mean (running)" } };
    }

    private void OnProgress(SnapshotManaged s)
    {
        PathsDone     = s.PathsDone;
        PathsTotal    = s.PathsTotal;
        RunningMean   = s.Mean;
        RunningStdErr = s.StdError;
        Ci95Low       = s.Mean - 1.96 * s.StdError;
        Ci95High      = s.Mean + 1.96 * s.StdError;
        ElapsedMs     = s.ElapsedMs;

        MeanHistory.Add    (new ObservablePoint(s.PathsDone, s.Mean));
        Ci95LowHistory.Add (new ObservablePoint(s.PathsDone, Ci95Low));
        Ci95HighHistory.Add(new ObservablePoint(s.PathsDone, Ci95High));
    }

    private async Task RunAsync()
    {
        ResetForNewRun();
        try
        {
            SimResultManaged result;
            switch (SelectedPayoff)
            {
                case PayoffKind.DiscountBond:
                    result = await _pipeline.RunDiscountBondAsync(
                        HullWhite.ToNative(), Steps, Paths, Scheme,
                        Threads, ProgressEveryMs, Seed);
                    break;

                case PayoffKind.GbmTerminal:
                default:
                    result = await _pipeline.RunGbmTerminalAsync(
                        Gbm.ToNative(), Steps, Paths, Scheme,
                        Threads, ProgressEveryMs, Seed);
                    break;
            }
            FinalResult = result;
        }
        catch (Exception ex)
        {
            ErrorMessage = ex.Message;
        }
    }

    private void ResetForNewRun()
    {
        MeanHistory.Clear();
        Ci95LowHistory.Clear();
        Ci95HighHistory.Clear();
        PathsDone = 0;
        PathsTotal = 0;
        RunningMean = RunningStdErr = Ci95Low = Ci95High = ElapsedMs = 0.0;
        FinalResult = null;
        ErrorMessage = null;
    }

    public void Dispose()
    {
        _pipeline.Progress    -= _progressHandler;
        _pipeline.BusyChanged -= _busyHandler;
    }
}
