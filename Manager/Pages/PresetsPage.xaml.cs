using System.Windows;
using System.Windows.Controls;
using SecondaryMotion.Manager.ViewModels;

namespace SecondaryMotion.Manager.Pages;

public partial class PresetsPage : Page {
    PresetsViewModel _vm = null!;

    public PresetsPage() {
        InitializeComponent();
        _vm = new PresetsViewModel(App.Ctx);
        _vm.AskText = (title, prompt, def) => {
            var dlg = new InputDialog(title, prompt, def) { Owner = Window.GetWindow(this) };
            return dlg.ShowDialog() == true ? dlg.Answer : null;
        };
        _vm.Confirm = (title, msg) =>
            MessageBox.Show(msg, title, MessageBoxButton.YesNo,
                            MessageBoxImage.Question) == MessageBoxResult.Yes;
        DataContext = _vm;
    }

    void OnLoaded(object sender, RoutedEventArgs e) { }
    void OnUnloaded(object sender, RoutedEventArgs e) { }
}
