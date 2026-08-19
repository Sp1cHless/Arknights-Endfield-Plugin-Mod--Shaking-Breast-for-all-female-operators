using System.Windows;
using System.Windows.Controls;
using SecondaryMotion.Manager.ViewModels;

namespace SecondaryMotion.Manager.Pages;

public partial class CharactersPage : Page {
    CharactersViewModel _vm = null!;

    public CharactersPage() {
        InitializeComponent();
        _vm = new CharactersViewModel(App.Ctx);
        DataContext = _vm;
    }

    void OnLoaded(object sender, RoutedEventArgs e) {
        // re-sync when navigating back (preset switches happen elsewhere)
        _vm.Refresh();
    }
    void OnUnloaded(object sender, RoutedEventArgs e) { }
}
