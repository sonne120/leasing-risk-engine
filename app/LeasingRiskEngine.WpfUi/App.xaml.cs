using System;
using System.IO;
using System.Windows;
using LeasingRiskEngine.Data;
using LeasingRiskEngine.WpfUi.Services;
using LeasingRiskEngine.WpfUi.ViewModels;
using LeasingRiskEngine.WpfUi.Views;
using Prism.DryIoc;
using Prism.Ioc;
using Prism.Navigation.Regions;

namespace LeasingRiskEngine.WpfUi;

public partial class App : PrismApplication
{
    protected override Window CreateShell() => Container.Resolve<MainWindow>();

    protected override void RegisterTypes(IContainerRegistry containerRegistry)
    {
        containerRegistry.RegisterSingleton<ICalculatorService, CalculatorService>();
        containerRegistry.RegisterSingleton<IMonteCarloService, MonteCarloService>();
        containerRegistry.RegisterSingleton<IRiskService, RiskService>();
        containerRegistry.RegisterSingleton<IPipelineService, PipelineService>();
        
        containerRegistry.RegisterInstance(new LeasingDatabase(LoadDatabaseOptions()));
        containerRegistry.RegisterSingleton<IContractStore, ContractStore>();
        containerRegistry.RegisterSingleton<IRunHistoryStore, RunHistoryStore>();

        containerRegistry.RegisterForNavigation<ContractsView, ContractsViewModel>(
            nameof(ContractsView));
        containerRegistry.RegisterForNavigation<MonteCarloView, MonteCarloViewModel>(
            nameof(MonteCarloView));
        containerRegistry.RegisterForNavigation<RiskDashboardView, RiskDashboardViewModel>(
            nameof(RiskDashboardView));
        containerRegistry.RegisterForNavigation<StressTestsView, StressTestsViewModel>(
            nameof(StressTestsView));
        containerRegistry.RegisterForNavigation<HistoryView, HistoryViewModel>(
            nameof(HistoryView));
    }

    private static DatabaseOptions LoadDatabaseOptions()
    {
        var path = Path.Combine(AppContext.BaseDirectory, "appsettings.json");
        try
        {
            return DatabaseOptions.Load(path);
        }
        catch (Exception ex)
        {
            MessageBox.Show(
                $"Не вдалося прочитати {path}:\n{ex.Message}\n\n" +
                "Використовуються типові налаштування бази даних.",
                "Leasing", MessageBoxButton.OK, MessageBoxImage.Warning);
            return new DatabaseOptions();
        }
    }

    protected override void OnInitialized()
    {
        base.OnInitialized();

        var regionManager = Container.Resolve<IRegionManager>();
        regionManager.RequestNavigate("ContentRegion", nameof(ContractsView));
    }

    protected override void OnExit(ExitEventArgs e)
    {
        Container.Resolve<IPipelineService>().Dispose();
        base.OnExit(e);
    }
}
