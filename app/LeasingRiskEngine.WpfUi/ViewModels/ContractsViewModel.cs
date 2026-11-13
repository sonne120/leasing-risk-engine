using System;
using System.Collections.ObjectModel;
using System.Linq;
using LeasingRiskEngine.WpfUi.Services;
using LeasingBridge;
using Prism.Commands;

namespace LeasingRiskEngine.WpfUi.ViewModels;

public sealed class ContractsViewModel : ViewModelBase
{
    private readonly ICalculatorService _calculator;

    private string   _number       = "L-2026-00847";
    private double   _principal    = 850_000.00;
    private double   _downPayment  = 170_000.00;
    private double   _annualRate   = 0.185;
    private int      _termMonths   = 36;
    private PaymentTypeManaged _type = PaymentTypeManaged.Annuity;
    private DateTime _startDate   = DateTime.Today;

    public string Number
    {
        get => _number;
        set => SetProperty(ref _number, value);
    }
    public double Principal
    {
        get => _principal;
        set => SetProperty(ref _principal, value);
    }
    public double DownPayment
    {
        get => _downPayment;
        set => SetProperty(ref _downPayment, value);
    }
    public double AnnualRate
    {
        get => _annualRate;
        set => SetProperty(ref _annualRate, value);
    }
    public int TermMonths
    {
        get => _termMonths;
        set => SetProperty(ref _termMonths, value);
    }
    public PaymentTypeManaged Type
    {
        get => _type;
        set => SetProperty(ref _type, value);
    }
    public DateTime StartDate
    {
        get => _startDate;
        set => SetProperty(ref _startDate, value);
    }

    public PaymentTypeManaged[] PaymentTypes { get; } =
        new[] { PaymentTypeManaged.Annuity, PaymentTypeManaged.Differential };

    public ObservableCollection<PaymentEntryManaged> Schedule { get; } = new();

    private double _totalPayments;
    private double _totalInterest;
    private double _monthlyPayment;
    private string? _errorMessage;

    public double TotalPayments
    {
        get => _totalPayments;
        private set => SetProperty(ref _totalPayments, value);
    }
    public double TotalInterest
    {
        get => _totalInterest;
        private set => SetProperty(ref _totalInterest, value);
    }
    public double MonthlyPayment
    {
        get => _monthlyPayment;
        private set => SetProperty(ref _monthlyPayment, value);
    }
    public string? ErrorMessage
    {
        get => _errorMessage;
        private set => SetProperty(ref _errorMessage, value);
    }
    public bool HasError => !string.IsNullOrEmpty(_errorMessage);

    public DelegateCommand CalculateCommand { get; }
    public DelegateCommand ClearCommand     { get; }

    public ContractsViewModel(ICalculatorService calculator)
    {
        _calculator = calculator;
        CalculateCommand = new DelegateCommand(Calculate);
        ClearCommand     = new DelegateCommand(Clear);
    }

    private void Calculate()
    {
        try
        {
            ErrorMessage = null;
            var result = _calculator.CalculateSchedule(
                Number, Principal, AnnualRate, TermMonths,
                Type, StartDate, DownPayment);

            Schedule.Clear();
            foreach (var row in result.Entries)
                Schedule.Add(row);

            TotalPayments  = result.TotalPayments;
            TotalInterest  = result.TotalInterest;
            MonthlyPayment = Schedule.FirstOrDefault()?.Payment ?? 0.0;
            RaisePropertyChanged(nameof(HasError));
        }
        catch (Exception ex)
        {
            ErrorMessage = ex.Message;
            RaisePropertyChanged(nameof(HasError));
        }
    }

    private void Clear()
    {
        Schedule.Clear();
        TotalPayments = TotalInterest = MonthlyPayment = 0.0;
        ErrorMessage = null;
        RaisePropertyChanged(nameof(HasError));
    }
}
