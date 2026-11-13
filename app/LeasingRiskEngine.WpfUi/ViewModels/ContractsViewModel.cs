using System;
using System.Collections.ObjectModel;
using System.Linq;
using System.Threading.Tasks;
using LeasingRiskEngine.Data;
using LeasingRiskEngine.WpfUi.Services;
using LeasingBridge;
using Prism.Commands;

namespace LeasingRiskEngine.WpfUi.ViewModels;

public sealed class ContractsViewModel : ViewModelBase
{
    private readonly ICalculatorService _calculator;
    private readonly IContractStore     _contracts;

    private string   _customerName = "";
    private string   _number       = "L-2026-00847";
    private double   _principal    = 850_000.00;
    private double   _downPayment  = 170_000.00;
    private double   _annualRate   = 0.185;
    private int      _termMonths   = 36;
    private PaymentTypeManaged _type = PaymentTypeManaged.Annuity;
    private DateTime _startDate   = DateTime.Today;

    public string CustomerName
    {
        get => _customerName;
        set => SetProperty(ref _customerName, value);
    }
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
        private set
        {
            if (SetProperty(ref _errorMessage, value))
                RaisePropertyChanged(nameof(HasError));
        }
    }
    public bool HasError => !string.IsNullOrEmpty(_errorMessage);

    // Saved contracts (database)

    public ObservableCollection<SavedContract> SavedContracts { get; } = new();

    private SavedContract? _selectedSaved;
    private string?        _statusMessage;
    private bool           _isStoreBusy;
    private bool           _savedLoaded;

    public SavedContract? SelectedSaved
    {
        get => _selectedSaved;
        set
        {
            if (SetProperty(ref _selectedSaved, value))
                RaiseStoreCommands();
        }
    }
    public string? StatusMessage
    {
        get => _statusMessage;
        private set
        {
            if (SetProperty(ref _statusMessage, value))
                RaisePropertyChanged(nameof(HasStatus));
        }
    }
    public bool HasStatus => !string.IsNullOrEmpty(_statusMessage);

    public bool IsStoreBusy
    {
        get => _isStoreBusy;
        private set
        {
            if (SetProperty(ref _isStoreBusy, value))
                RaiseStoreCommands();
        }
    }

    public DelegateCommand CalculateCommand { get; }
    public DelegateCommand ClearCommand     { get; }
    public DelegateCommand SaveCommand      { get; }
    public DelegateCommand LoadCommand      { get; }
    public DelegateCommand DeleteCommand    { get; }

    public ContractsViewModel(ICalculatorService calculator,
                              IContractStore contracts)
    {
        _calculator = calculator;
        _contracts  = contracts;
        CalculateCommand = new DelegateCommand(Calculate);
        ClearCommand     = new DelegateCommand(Clear);
        SaveCommand      = new DelegateCommand(async () => await SaveAsync(),
                                               () => !IsStoreBusy);
        LoadCommand      = new DelegateCommand(LoadSelected,
                                               () => !IsStoreBusy && SelectedSaved != null);
        DeleteCommand    = new DelegateCommand(async () => await DeleteSelectedAsync(),
                                               () => !IsStoreBusy && SelectedSaved != null);
    }

    public override async void OnNavigatedTo(
        Prism.Navigation.Regions.NavigationContext navigationContext)
    {
        if (_savedLoaded) return;
        _savedLoaded = await RunStoreAsync(ReloadSavedAsync);
    }

    private void RaiseStoreCommands()
    {
        SaveCommand.RaiseCanExecuteChanged();
        LoadCommand.RaiseCanExecuteChanged();
        DeleteCommand.RaiseCanExecuteChanged();
    }

    // Runs a database operation; a failure is shown, never thrown.
    private async Task<bool> RunStoreAsync(Func<Task> operation)
    {
        IsStoreBusy = true;
        try
        {
            await operation();
            return true;
        }
        catch (Exception ex)
        {
            StatusMessage = null;
            ErrorMessage  = "База даних: " + ex.GetBaseException().Message;
            return false;
        }
        finally
        {
            IsStoreBusy = false;
        }
    }

    private async Task ReloadSavedAsync()
    {
        var keep = SelectedSaved?.Number;
        var all  = await _contracts.ListAsync();

        SavedContracts.Clear();
        foreach (var c in all) SavedContracts.Add(c);
        SelectedSaved = SavedContracts.FirstOrDefault(c => c.Number == keep);
    }

    private async Task SaveAsync()
    {
        StatusMessage = null;
        if (string.IsNullOrWhiteSpace(CustomerName))
        {
            ErrorMessage = "Вкажіть клієнта, щоб зберегти договір.";
            return;
        }

        // Save the figures that belong to the inputs on screen.
        Calculate();
        if (HasError) return;

        var number = Number.Trim();
        var contract = new SavedContract
        {
            CustomerName   = CustomerName.Trim(),
            Number         = number,
            Principal      = Principal,
            DownPayment    = DownPayment,
            AnnualRate     = AnnualRate,
            TermMonths     = TermMonths,
            PaymentType    = (int)Type,
            StartDate      = StartDate,
            MonthlyPayment = MonthlyPayment,
            TotalPayments  = TotalPayments,
            TotalInterest  = TotalInterest,
        };

        var saved = await RunStoreAsync(async () =>
        {
            await _contracts.SaveAsync(contract);
            await ReloadSavedAsync();
            SelectedSaved = SavedContracts.FirstOrDefault(c => c.Number == number);
        });
        if (saved) StatusMessage = $"Договір {number} збережено.";
    }

    private void LoadSelected()
    {
        if (SelectedSaved is not { } c) return;

        CustomerName = c.CustomerName;
        Number       = c.Number;
        Principal    = c.Principal;
        DownPayment  = c.DownPayment;
        AnnualRate   = c.AnnualRate;
        TermMonths   = c.TermMonths;
        Type         = (PaymentTypeManaged)c.PaymentType;
        StartDate    = c.StartDate;

        StatusMessage = null;
        Calculate();
    }

    private async Task DeleteSelectedAsync()
    {
        if (SelectedSaved is not { } c) return;

        StatusMessage = null;
        var deleted = await RunStoreAsync(async () =>
        {
            await _contracts.DeleteAsync(c.Number);
            await ReloadSavedAsync();
        });
        if (deleted) StatusMessage = $"Договір {c.Number} видалено.";
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
