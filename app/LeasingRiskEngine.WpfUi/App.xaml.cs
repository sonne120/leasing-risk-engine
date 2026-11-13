using System.Windows;
using LeasingRiskEngine.WpfUi.Services;
using LeasingRiskEngine.WpfUi.ViewModels;
using LeasingRiskEngine.WpfUi.Views;
using Prism.DryIoc;
using Prism.Ioc;

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

        containerRegistry.RegisterForNavigation<ContractsView, ContractsViewModel>(
            nameof(ContractsView));
        containerRegistry.RegisterForNavigation<MonteCarloView, MonteCarloViewModel>(
            nameof(MonteCarloView));
        containerRegistry.RegisterForNavigation<RiskDashboardView, RiskDashboardViewModel>(
            nameof(RiskDashboardView));
        containerRegistry.RegisterForNavigation<StressTestsView, StressTestsViewModel>(
            nameof(StressTestsView));
    }

    protected override void OnInitialized()
    {
        base.OnInitialized();

        var regionManager = Container.Resolve<Prism.Navigation.Regions.IRegionManager>();
        regionManager.RequestNavigate("ContentRegion", nameof(ContractsView));
    }
}
