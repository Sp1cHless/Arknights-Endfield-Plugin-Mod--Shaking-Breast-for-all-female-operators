using System.Windows;
using System.Windows.Controls;
using SecondaryMotion.Manager.ViewModels;

namespace SecondaryMotion.Manager.Pages;

public partial class DeveloperPage : Page {
    DeveloperViewModel _vm = null!;

    public DeveloperPage() {
        InitializeComponent();
        _vm = new DeveloperViewModel(App.Ctx);
        DataContext = _vm;
    }

    void OnLoaded(object sender, RoutedEventArgs e) { }
    void OnUnloaded(object sender, RoutedEventArgs e) => _vm.Dispose();
}
