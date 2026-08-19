using System.Windows;
using System.Windows.Controls;
using SecondaryMotion.Manager.Services;
using SecondaryMotion.Manager.ViewModels;

namespace SecondaryMotion.Manager.Pages;

public partial class MainPage : Page {
    MainViewModel _vm = null!;

    public MainPage() {
        InitializeComponent();
        _vm = new MainViewModel(App.Ctx);
        DataContext = _vm;
    }

    void OnLoaded(object sender, RoutedEventArgs e) { }
    void OnUnloaded(object sender, RoutedEventArgs e) {
        _vm.Dispose();
    }
}
