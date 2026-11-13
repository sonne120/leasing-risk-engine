using System;
using System.Collections.Generic;
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

public enum LossKind
{
    ZeroBondLoss,
    ResidualValueLoss,
}

public sealed class RiskDashboardViewModel : ViewModelBase
{
    private readonly IRiskService _risk;

    public HullWhiteInputModel HullWhite { get; } = new();
    public GbmInputModel       Gbm       { get; } = new();

    public LossKind[]          LossKinds { get; } =
        { LossKind.ZeroBondLoss, LossKind.ResidualValueLoss };
    public SchemeKindManaged[] Schemes   { get; } =
        { SchemeKindManaged.Euler, SchemeKindManaged.Milstein };

    private LossKind          _selectedLoss = LossKind.ZeroBondLoss;
    private SchemeKindManaged _scheme       = SchemeKindManaged.Euler;
    private int               _steps        = 252;
    private int               _paths        = 50_000;
    private int               _threads      = 0;
    private ulong             _seed         = 0xC0FFEEUL;

    public LossKind SelectedLoss
    {
        get => _selectedLoss;
        set { if (SetProperty(ref _selectedLoss, value)) {
            RaisePropertyChanged(nameof(ShowHullWhite));
            RaisePropertyChanged(nameof(ShowGbm));
        } }
    }
    public SchemeKindManaged Scheme  { get => _scheme;  set => SetProperty(ref _scheme, value); }
    public int               Steps   { get => _steps;   set => SetProperty(ref _steps, value); }
    public int               Paths   { get => _paths;   set => SetProperty(ref _paths, value); }
    public int               Threads { get => _threads; set => SetProperty(ref _threads, value); }
    public ulong             Seed    { get => _seed;    set => SetProperty(ref _seed, value); }

    public bool ShowHullWhite => SelectedLoss == LossKind.ZeroBondLoss;
    public bool ShowGbm       => SelectedLoss == LossKind.ResidualValueLoss;

    private bool     _isBusy;
    private string?  _errorMessage;
    private VaRResultManaged? _result;

    public bool IsBusy
    {
        get => _isBusy;
        private set { if (SetProperty(ref _isBusy, value)) {
            ComputeCommand.RaiseCanExecuteChanged();
        } }
    }
    public string? ErrorMessage
    {
        get => _errorMessage;
        private set { if (SetProperty(ref _errorMessage, value))
            RaisePropertyChanged(nameof(HasError)); }
    }
    public bool HasError => !string.IsNullOrEmpty(_errorMessage);

    public VaRResultManaged? Result
    {
        get => _result;
        private set { if (SetProperty(ref _result, value))
            RaisePropertyChanged(nameof(HasResult)); }
    }
    public bool HasResult => _result != null;

    public ObservableCollection<ObservablePoint> HistogramValues { get; } = new();

    public ISeries[]          Series   { get; }
    public ICartesianAxis[]   XAxes    { get; } = { new Axis { Name = "Loss" } };
    public ICartesianAxis[]   YAxes    { get; } = { new Axis { Name = "Frequency" } };
    public RectangularSection[] Sections { get; }

    public DelegateCommand ComputeCommand { get; }

    public RiskDashboardViewModel(IRiskService risk)
    {
        _risk = risk;

        Series = new ISeries[]
        {
            new ColumnSeries<ObservablePoint>
            {
                Name = "Loss distribution",
                Values = HistogramValues,
                Fill   = new SolidColorPaint(SKColor.Parse("#85B7EB")),
                Stroke = new SolidColorPaint(SKColor.Parse("#185FA5"), 1),
                MaxBarWidth = double.MaxValue,
                Padding = 0.5,
            },
        };

        Sections = new RectangularSection[]
        {
            new RectangularSection {
                Label = "VaR 95%",
                LabelPaint = new SolidColorPaint(SKColor.Parse("#993C1D")),
                Stroke = new SolidColorPaint(SKColor.Parse("#D85A30"), 2)
                {
                    PathEffect = new DashEffect(new float[] { 6, 3 }),
                },
            },
            new RectangularSection {
                Label = "VaR 99%",
                LabelPaint = new SolidColorPaint(SKColor.Parse("#791F1F")),
                Stroke = new SolidColorPaint(SKColor.Parse("#A32D2D"), 2)
                {
                    PathEffect = new DashEffect(new float[] { 6, 3 }),
                },
            },
        };

        ComputeCommand = new DelegateCommand(async () => await ComputeAsync(),
                                              () => !IsBusy);
    }

    private async Task ComputeAsync()
    {
        IsBusy = true;
        ErrorMessage = null;
        Result = null;
        HistogramValues.Clear();
        try
        {
            var run = await Task.Run(() => RunNative());
            Result = run.Risk;

            BuildHistogram(run.Samples, run.Risk.BestCase, run.Risk.WorstCase);

            Sections[0].Xi = Sections[0].Xj = Result!.Var95;
            Sections[1].Xi = Sections[1].Xj = Result!.Var99;
        }
        catch (Exception ex)
        {
            ErrorMessage = ex.Message;
        }
        finally
        {
            IsBusy = false;
        }
    }

    private RiskRunResultManaged RunNative()
    {
        return SelectedLoss switch
        {
            LossKind.ZeroBondLoss =>
                _risk.ZeroBondLoss(HullWhite.ToNative(), Steps, Paths,
                                    Scheme, Threads, Seed),
            _ =>
                _risk.ResidualValueLoss(Gbm.ToNative(), Steps, Paths,
                                         Scheme, Threads, Seed),
        };
    }

    private void BuildHistogram(IReadOnlyList<double> samples,
                                double min, double max)
    {
        const int Bins = 50;
        if (samples.Count == 0 || max <= min)
        {
            HistogramValues.Clear();
            return;
        }

        var counts = new int[Bins];
        var width  = (max - min) / Bins;

        foreach (var v in samples)
        {
            var idx = (int)Math.Floor((v - min) / width);
            if (idx < 0)       idx = 0;
            if (idx >= Bins)   idx = Bins - 1;
            counts[idx]++;
        }

        HistogramValues.Clear();
        for (int i = 0; i < Bins; ++i)
        {
            var center = min + (i + 0.5) * width;
            HistogramValues.Add(new ObservablePoint(center, counts[i]));
        }
    }
}
