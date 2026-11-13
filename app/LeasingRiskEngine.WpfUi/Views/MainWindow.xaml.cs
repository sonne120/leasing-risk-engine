using LeasingRiskEngine.WpfUi.ViewModels;
using Prism.Navigation.Regions;
using Wpf.Ui.Controls;

namespace LeasingRiskEngine.WpfUi.Views;

public partial class MainWindow : FluentWindow
{
    private readonly IRegionManager _regionManager;

    public MainWindow(IRegionManager regionManager)
    {
        _regionManager = regionManager;
        InitializeComponent();
        RootNavigation.SelectionChanged += OnNavigationChanged;
    }

    private void OnNavigationChanged(NavigationView sender, System.Windows.RoutedEventArgs args)
    {
        if (sender.SelectedItem is NavigationViewItem item &&
            item.Tag is string key)
        {
            _regionManager.RequestNavigate("ContentRegion", key);
            if (DataContext is MainWindowViewModel vm)
                vm.OnNavigated(key);
        }
    }
}
