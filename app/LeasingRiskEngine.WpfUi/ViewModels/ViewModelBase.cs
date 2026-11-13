using Prism.Mvvm;
using Prism.Navigation.Regions;

namespace LeasingRiskEngine.WpfUi.ViewModels;

public abstract class ViewModelBase : BindableBase, INavigationAware
{
    public virtual void OnNavigatedTo(NavigationContext navigationContext) { }

    public virtual bool IsNavigationTarget(NavigationContext navigationContext) => true;

    public virtual void OnNavigatedFrom(NavigationContext navigationContext) { }

    protected void RaiseAllPropertiesChanged() => RaisePropertyChanged(string.Empty);
}
